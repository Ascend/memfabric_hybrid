/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_gvm_proc.h"

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/dma-mapping.h>
#include <linux/rwlock_types.h>

#include "hybm_gvm_ioctl.h"
#include "hybm_gvm_log.h"
#include "hybm_gvm_p2p.h"
#include "hybm_gvm_phy_page.h"
#include "hybm_gvm_s2a.h"
#include "hybm_gvm_tree.h"
#include "hybm_gvm_symbol_get.h"

#define GVM_NODE_FLAG_ALLOCATED 1ULL
#define GVM_NODE_FLAG_REMOTE    2ULL
#define GVM_NODE_FLAG_NO_SHARE  3ULL
#define GVM_NODE_FLAG_DMA       4ULL

#define GVM_KEY_SDID_OFFSET    32
#define GVM_SDID_SERVER_OFFSET 22
#define GVM_GLOBAL_SERVER_NUM  48
#define GVM_SINGLE_NODE_CPU_NUM  4

#define GVM_GLOBAL_DRAM_PA_START    (0x800000000000ULL)
#define GVM_GLOBAL_DRAM_PA_PSIZE    (0x00aa80000000ULL) // 682G
#define GVM_GLOBAL_PA_MASK          (0xffffffffffffULL) // 256T (2^48)

static DEFINE_RWLOCK(proc_rwlock);
static struct hybm_gvm_process *g_proc_list[HYBM_MAX_DEVICE_NUM] = {NULL};

static inline bool gvm_check_flag(u64 *flag, u64 shift)
{
    return (((*flag) >> shift) & 1ULL);
}

static inline void gvm_set_flag(u64 *flag, u64 shift)
{
    (*flag) |= (1ULL << shift);
}

static inline void gvm_clear_flag(u64 *flag, u64 shift)
{
    (*flag) &= (~(1ULL << shift));
}

static inline u32 gvm_get_devid_by_sdid(u32 sdid)
{
    return (sdid & (((u32)1 << UDEVID_BIT_LEN) - 1));
}

static inline u64 gvm_generate_key(u32 sdid, u32 id)
{
    return (((u64)sdid << GVM_KEY_SDID_OFFSET) | id);
}

static inline u32 gvm_get_sdid_by_key(u64 key)
{
    return (key >> GVM_KEY_SDID_OFFSET);
}

static inline u32 gvm_get_server_id_by_sdid(u32 sdid)
{
    return (sdid >> GVM_SDID_SERVER_OFFSET);
}

static u64 g_local_pa_start[GVM_SINGLE_NODE_CPU_NUM] = {
    0x029580000000ULL,
    0x0a9580000000ULL,
    0x129580000000ULL,
    0x1a9580000000ULL
};

static u64 gvm_get_local_pa_from_global(u64 pa, u32 sdid)
{
    u32 cpu_id = 0;
    if (pa < GVM_GLOBAL_DRAM_PA_START) {
        hybm_gvm_err("translate failed, pa is invalid");
        return 0;
    }

    cpu_id = (pa - GVM_GLOBAL_DRAM_PA_START) / GVM_GLOBAL_DRAM_PA_PSIZE % GVM_SINGLE_NODE_CPU_NUM;
    return g_local_pa_start[cpu_id] + ((pa - GVM_GLOBAL_DRAM_PA_START) % GVM_GLOBAL_DRAM_PA_PSIZE);
}

static u64 gvm_get_global_pa_from_local(u64 pa, u32 sdid)
{
    u32 i;
    u32 server_id = gvm_get_server_id_by_sdid(sdid);
    if (server_id >= GVM_GLOBAL_SERVER_NUM) {
        hybm_gvm_err("translate failed, server id is invalid(%u)", server_id);
        return 0ULL;
    }

    for (i = 0; i < GVM_SINGLE_NODE_CPU_NUM; i++) {
        if (g_local_pa_start[i] <= pa && pa < (g_local_pa_start[i] + GVM_GLOBAL_DRAM_PA_PSIZE)) {
            return GVM_GLOBAL_DRAM_PA_START + GVM_GLOBAL_DRAM_PA_PSIZE * (server_id * GVM_SINGLE_NODE_CPU_NUM + i) +
                   (pa - g_local_pa_start[i]);
        }
    }

    hybm_gvm_err("translate failed, pa is invalid");
    return 0;
}

static struct gvm_node *hybm_gvm_node_alloc(u64 va, u64 size)
{
    struct gvm_node *node = NULL;
    u32 pa_num = size / HYBM_GVM_PAGE_SIZE;
    if (!IS_MULTIPLE_OF(va, HYBM_GVM_PAGE_SIZE) || !IS_MULTIPLE_OF(size, HYBM_GVM_PAGE_SIZE)) {
        hybm_gvm_err("input va is not aligned or size is not a multiple of page. size(0x%llx)", size);
        return NULL;
    }

    if (size > HYBM_GVM_ALLOC_MAX_SIZE) {
        hybm_gvm_err("input size is too large, size(0x%llx)", size);
        return NULL;
    }
    if (pa_num > SIZE_MAX / sizeof(struct gvm_pa_node)) {
        hybm_gvm_err("input size is too large, size(0x%llx)", size);
        return NULL;
    }
    node = kzalloc(sizeof(struct gvm_node) + sizeof(struct gvm_pa_node) * pa_num, GFP_KERNEL | __GFP_ACCOUNT);
    if (node == NULL) {
        hybm_gvm_err("kzalloc gvm node fail.");
        return NULL;
    }

    node->va = va;
    node->size = size;
    node->pa_num = pa_num;
    RB_CLEAR_NODE(&node->key_node);
    RB_CLEAR_NODE(&node->va_node);
    kref_init(&node->ref);
    mutex_init(&node->lock);
    INIT_LIST_HEAD(&node->wlist_head);
    return node;
}

static void hybm_gvm_node_ref_release(struct kref *kref)
{
    struct gvm_node *node = container_of(kref, struct gvm_node, ref);
    hybm_gvm_debug("release gvm node");
    kfree(node);
}

static struct gvm_dev_mem_node *hybm_gvm_dev_node_alloc(u64 va, u64 size)
{
    struct gvm_dev_mem_node *node = NULL;
    node = kzalloc(sizeof(struct gvm_dev_mem_node), GFP_KERNEL | __GFP_ACCOUNT);
    if (node == NULL) {
        hybm_gvm_err("kzalloc gvm dev node fail.");
        return NULL;
    }

    node->va = va;
    node->size = size;
    RB_CLEAR_NODE(&node->va_node);
    kref_init(&node->ref);
    return node;
}

static void hybm_gvm_dev_node_ref_release(struct kref *kref)
{
    struct gvm_dev_mem_node *node = container_of(kref, struct gvm_dev_mem_node, ref);
    hybm_gvm_debug("release gvm dev node");
    kfree(node);
}

// @return true if `init_flag` has already been set.
static bool gvm_test_and_set_init_flag(struct file *file)
{
    int init_flag = atomic_add_return(1, &((struct gvm_private_data *)file->private_data)->init_flag);
    if (init_flag != 1) {
        atomic_sub(1, &((struct gvm_private_data *)file->private_data)->init_flag);
        return true;
    }
    return false;
}

static int hybm_init_gvm_proc(struct hybm_gvm_process *gvm_proc, u64 start, u64 end, u32 devid)
{
    u64 num = (end - start) / HYBM_GVM_PAGE_SIZE;
    struct spod_info spod = {0};
    int ret;

    ret = dbl_get_spod_info(devid, &spod);
    if (ret != 0) {
        hybm_gvm_err("Get spod info fail. (ret=%d; devid=%u)", ret, devid);
        return -EBUSY;
    }
    gvm_proc->sdid = spod.sdid;
    gvm_proc->devid = devid;

    ret = hybm_gvm_to_agent_init(devid, &gvm_proc->pasid, &gvm_proc->svspid);
    if (ret != 0) {
        hybm_gvm_err("Get sdma info fail. (ret=%d; devid=%u)", ret, devid);
        return -EBUSY;
    }

    gvm_proc->va_start = start;
    gvm_proc->va_end = end;
    gvm_proc->va_page_num = num;

    gvm_proc->key_tree.tree = RB_ROOT;
    mutex_init(&gvm_proc->key_tree.lock);

    gvm_proc->va_tree.tree = RB_ROOT;
    mutex_init(&gvm_proc->va_tree.lock);

    gvm_proc->fetch_tree.tree = RB_ROOT;
    mutex_init(&gvm_proc->fetch_tree.lock);

    kref_init(&gvm_proc->ref);
    return 0;
}

static void hybm_free_gvm_proc(struct hybm_gvm_process **gvm_proc)
{
    if (*gvm_proc == NULL) {
        return;
    }

    mutex_destroy(&(*gvm_proc)->key_tree.lock);
    mutex_destroy(&(*gvm_proc)->va_tree.lock);
    mutex_destroy(&(*gvm_proc)->fetch_tree.lock);

    kfree((*gvm_proc));
    (*gvm_proc) = NULL;
}

void hybm_proc_ref_release(struct kref *kref)
{
    struct hybm_gvm_process *proc = container_of(kref, struct hybm_gvm_process, ref);

    hybm_gvm_debug("release gvm proc");
    hybm_free_gvm_proc(&proc);
}

static struct hybm_gvm_process *hybm_gvm_get_proc_by_sdid(u32 sdid)
{
    u32 devid = gvm_get_devid_by_sdid(sdid);
    struct hybm_gvm_process *proc = NULL;

    if (devid >= HYBM_MAX_DEVICE_NUM) {
        hybm_gvm_err("devid is invalid. sdid(0x%x)", sdid);
        return NULL;
    }

    read_lock_bh(&proc_rwlock);
    proc = g_proc_list[devid];
    if (proc == NULL || proc->initialized != true) {
        hybm_gvm_err("local proc not init or has exited!. sdid(0x%x)", sdid);
        read_unlock_bh(&proc_rwlock);
        return NULL;
    }
    kref_get(&proc->ref);
    read_unlock_bh(&proc_rwlock);
    return proc;
}

static struct hybm_gvm_process *hybm_alloc_gvm_proc(u64 start, u64 end, u32 devid)
{
    struct hybm_gvm_process *gvm_proc = NULL;
    int ret;

    gvm_proc = kzalloc(sizeof(struct hybm_gvm_process), GFP_KERNEL | __GFP_ACCOUNT);
    if (gvm_proc == NULL) {
        hybm_gvm_err("kzalloc gvm process fail.");
        return NULL;
    }

    ret = hybm_init_gvm_proc(gvm_proc, start, end, devid);
    if (ret != 0) {
        hybm_gvm_err("gvm proc init fail. (ret=%d)", ret);
        hybm_free_gvm_proc(&gvm_proc);
        return NULL;
    }

    write_lock_bh(&proc_rwlock);
    if (g_proc_list[devid] != NULL) {
        hybm_gvm_err("proc with same devid already exists!");
        write_unlock_bh(&proc_rwlock);
        hybm_free_gvm_proc(&gvm_proc);
        return NULL;
    }

    g_proc_list[devid] = gvm_proc;
    write_unlock_bh(&proc_rwlock);
    hybm_gvm_info("create gvm proc");
    return gvm_proc;
}

static int hybm_gvm_proc_create(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_process *gvm_proc = NULL;
    struct hybm_gvm_proc_create_para *arg = &args->data.proc_create_para;
    if (file == NULL) {
        hybm_gvm_err("input param file is invalid..");
        return -EINVAL;
    }

    if (gvm_test_and_set_init_flag(file) == true) {
        hybm_gvm_err("gvm already initialized.");
        return -EALREADY;
    }

    if (arg->start >= arg->end || (arg->end - arg->start) % HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input va range is invalid. reversed(%u), size(%llu)", arg->start > arg->end,
                     arg->start > arg->end ? 0 : arg->end - arg->start);
        return -EINVAL;
    }

    if (arg->devid >= HYBM_MAX_DEVICE_NUM) {
        hybm_gvm_err("input devid is invalid. devid(%u)", arg->devid);
        return -EINVAL;
    }

    gvm_proc = hybm_alloc_gvm_proc(arg->start, arg->end, arg->devid);
    if (gvm_proc == NULL) {
        hybm_gvm_err("alloc gvm_proc struct fail.");
        return -ENOMEM;
    }

    gvm_proc->initialized = true;
    ((struct gvm_private_data *)file->private_data)->process = gvm_proc;
    arg->svspid = gvm_proc->svspid;
    arg->sdid = gvm_proc->sdid;
    hybm_gvm_debug("hybm_gvm_proc_create success. pasid:%d", gvm_proc->pasid);
    return 0;
}

// 将物理地址映射到用户空间虚拟地址
static int hybm_gvm_map_phys_to_user(struct hybm_gvm_process *proc, u64 virt_addr, u64 phys_addr, u64 size)
{
    struct vm_area_struct *vma;
    u64 pfn = PFN_DOWN(phys_addr);
    int ret;

    if (proc->vma == NULL || proc->mm == NULL) {
        hybm_gvm_err("gvm not mmap!");
        return -EINVAL;
    }

    if (size > UINT64_MAX - virt_addr) {
        hybm_gvm_err("input va+size overflow!");
        return -EINVAL;
    }

    vma = proc->vma;
    if (!pfn_valid(pfn)) {
        hybm_gvm_err("Invalid PFN: 0x%llx", pfn);
        return -EINVAL;
    }

    if (vma->vm_end < virt_addr + size || vma->vm_start > virt_addr) {
        hybm_gvm_err("vma range is small, size:0x%llx", size);
        return -EINVAL;
    }

    ret = remap_pfn_range(vma, virt_addr, pfn, size, vma->vm_page_prot);
    return ret;
}

static void hybm_gvm_unmap_phys(struct mm_struct *mm, u64 virt_addr, u64 size)
{
}

static void hybm_gvm_dma_unmap(struct hybm_gvm_process *proc, u64 *pa_list, u32 num)
{
    u32 i;
    struct device *dev = uda_get_device(proc->devid);
    if (dev == NULL) {
        hybm_gvm_err("uda_get_device failed, devid:%u", proc->devid);
        return;
    }

    for (i = 0; i < num; i++) {
        if (dma_mapping_error(dev, pa_list[i]) == 0) {
            dma_unmap_page(dev, pa_list[i], HYBM_HPAGE_SIZE, DMA_BIDIRECTIONAL);
        }
    }
    hybm_gvm_debug("hybm_gvm_dma_unmap");
}

static int hybm_gvm_dma_map(struct hybm_gvm_process *proc, u64 pa, u64 *pa_list, u32 num)
{
    u32 i;
    struct device *dev = uda_get_device(proc->devid);
    if (dev == NULL) {
        hybm_gvm_err("uda_get_device failed, devid:%u", proc->devid);
        return -EINVAL;
    }
    struct page *pg;
    int ret;

    for (i = 0; i < num; i++) {
        pg = pfn_to_page(PFN_DOWN(pa + HYBM_HPAGE_SIZE * i));
        pa_list[i] = devdrv_dma_map_page(dev, pg, 0, HYBM_HPAGE_SIZE, DMA_BIDIRECTIONAL);

        ret = dma_mapping_error(dev, pa_list[i]);
        if (ret != 0) {
            hybm_gvm_err("dma map failed, idx:%u, ret:%d", i, ret);
            hybm_gvm_dma_unmap(proc, pa_list, i);
            return -EINVAL;
        }
    }

    hybm_gvm_debug("hybm_gvm_dma_map");
    return 0;
}

static int hybm_gvm_mem_add_dma(struct hybm_gvm_process *proc, struct gvm_pa_node *pa_node)
{
    u64 *pa_list = NULL;
    int ret;

    if (pa_node->pa == 0) {
        hybm_gvm_err("input pa is invalid.");
        return -EINVAL;
    }

    pa_list = kzalloc(sizeof(u64) * HYBM_GVM_PAGE_NUM, GFP_KERNEL | __GFP_ACCOUNT);
    if (pa_list == NULL) {
        hybm_gvm_err("kzalloc pa_list fail.");
        return -ENOMEM;
    }

    ret = hybm_gvm_dma_map(proc, pa_node->pa, pa_list, HYBM_GVM_PAGE_NUM);
    if (ret != 0) {
        kfree(pa_list);
        return ret;
    }

    pa_node->dma_list = pa_list;
    return 0;
}

// node在上层加锁,函数内只会往device发消息
static void hybm_gvm_mem_free_inner(struct hybm_gvm_process *proc, struct gvm_node *node, u64 skip_id)
{
    u32 i;

    hybm_gvm_debug("hybm_gvm_mem_free, size:0x%llx", node->size);
    for (i = 0; i < node->pa_num; i++) {
        if (node->pmem[i].pa != 0 && i != skip_id) {
            hybm_gvm_to_agent_unmap(proc->devid, proc->pasid, node->va + i * HYBM_GVM_PAGE_SIZE,
                                    HYBM_GVM_PAGE_SIZE, HYBM_GVM_PAGE_SIZE);
            hybm_gvm_unmap_phys(proc->mm, node->va + i * HYBM_GVM_PAGE_SIZE, HYBM_GVM_PAGE_SIZE);
        }
        if (node->pmem[i].pa != 0 && !gvm_check_flag(&node->flag, GVM_NODE_FLAG_REMOTE)) {
            hybm_gvm_free_pg(node->pmem[i].pa);
        }
        if (gvm_check_flag(&node->flag, GVM_NODE_FLAG_DMA) && node->pmem[i].dma_list) {
            hybm_gvm_dma_unmap(proc, node->pmem[i].dma_list, HYBM_GVM_PAGE_NUM);
            kfree(node->pmem[i].dma_list);
            node->pmem[i].dma_list = NULL;
        }

        node->pmem[i].pa = 0;
    }
    gvm_clear_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED);
}

// 同时会清理本次计数
static void hybm_gvm_node_free(struct hybm_gvm_process *proc, struct gvm_node *node)
{
    struct gvm_wlist_node *wnode = NULL;
    struct list_head *pos = NULL, *n = NULL;
    u32 sdid;
    void *rproc;

    mutex_lock(&node->lock);
    hybm_gvm_mem_free_inner(proc, node, UINT64_MAX);

    while (!list_empty_careful(&node->wlist_head)) {
        wnode = list_first_entry(&node->wlist_head,
        struct gvm_wlist_node, node);
        sdid = wnode->sdid;
        rproc = wnode->rproc;

        mutex_unlock(&node->lock); // unlock when p2p
        hybm_gvm_p2p_unset_wl(proc->sdid, proc->devid, sdid, node->shm_key, rproc);
        mutex_lock(&node->lock);

        list_for_each_safe(pos, n, &node->wlist_head)
        {
            wnode = list_entry(pos, struct gvm_wlist_node, node);
            if (wnode->sdid == sdid && wnode->rproc == rproc) {
                list_del_init(&wnode->node);
                kfree(wnode);
                break;
            }
        }
    }
    mutex_unlock(&node->lock);

    gvm_key_tree_remove(&proc->key_tree, node, hybm_gvm_node_ref_release);
    gvm_va_tree_remove_and_dec(&proc->va_tree, (void *)node, hybm_gvm_node_ref_release); // 减去init时加的计数
    kref_put(&node->ref, hybm_gvm_node_ref_release); // 减去search时加的计数
}

// node在上层加锁,函数内只会往device发消息
static int hybm_gvm_mem_alloc_inner(struct hybm_gvm_process *proc, struct gvm_node *node)
{
    u64 va = node->va;
    u32 i;
    int ret;

    for (i = 0; i < node->pa_num; i++) {
        node->pmem[i].pa = hybm_gvm_alloc_pg();
        if (node->pmem[i].pa == 0) {
            hybm_gvm_err("alloc pa failed.");
            ret = -EBUSY;
            goto failed_return;
        }

        ret = hybm_gvm_map_phys_to_user(proc, va + i * HYBM_GVM_PAGE_SIZE, node->pmem[i].pa, HYBM_GVM_PAGE_SIZE);
        if (ret != 0) {
            hybm_gvm_err("map host mem failed. ret:%d", ret);
            goto failed_return;
        }

        if (gvm_check_flag(&node->flag, GVM_NODE_FLAG_DMA)) {
            ret = hybm_gvm_mem_add_dma(proc, &node->pmem[i]);
            if (ret != 0) {
                hybm_gvm_err("dma host mem failed. ret:%d", ret);
                hybm_gvm_unmap_phys(proc->mm, va + i * HYBM_GVM_PAGE_SIZE, HYBM_GVM_PAGE_SIZE);
                goto failed_return;
            }

            ret = hybm_gvm_to_agent_mmap(proc->devid, proc->pasid, va + i * HYBM_GVM_PAGE_SIZE,
                                         HYBM_GVM_PAGE_SIZE, node->pmem[i].dma_list, HYBM_GVM_PAGE_NUM);
        } else {
            ret = hybm_gvm_to_agent_mmap(proc->devid, proc->pasid, va + i * HYBM_GVM_PAGE_SIZE,
                                         HYBM_GVM_PAGE_SIZE, &node->pmem[i].pa, 1U);
        }
        if (ret != 0) {
            hybm_gvm_err("map device sdma failed. ret:%d", ret);
            hybm_gvm_unmap_phys(proc->mm, va + i * HYBM_GVM_PAGE_SIZE, HYBM_GVM_PAGE_SIZE);
            goto failed_return;
        }
        hybm_gvm_debug("alloc mem and map, idx:%u", i);
    }

    gvm_set_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED);
    return 0;

failed_return:
    hybm_gvm_mem_free_inner(proc, node, i);
    return ret;
}

static int hybm_gvm_mem_alloc(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_mem_alloc_para *arg = &args->data.mem_alloc_para;
    struct gvm_node *node = NULL;
    u64 va = arg->addr;
    u64 size = arg->size;
    int ret;

    if (!IS_MULTIPLE_OF(va, HYBM_GVM_PAGE_SIZE) || !IS_MULTIPLE_OF(size, HYBM_GVM_PAGE_SIZE)) {
        hybm_gvm_err("input va is not aligned or size is not a multiple of page. size(0x%llx)", size);
        return -EINVAL;
    }

    if (size > UINT64_MAX - va) {
        hybm_gvm_err("input va+size overflow. size(0x%llx)", size);
    }

    if (va < proc->va_start || va + size > proc->va_end) {
        hybm_gvm_err("input va range is not contained by proc va. input:size(0x%llx) proc:size(0x%llx)\n", size,
                     proc->va_end - proc->va_start);
        return -EINVAL;
    }

    if (gvm_va_tree_cross(&proc->va_tree, va, size)) {
        hybm_gvm_err("input range has already been allocated. input:size(0x%llx)", size);
        return -EBUSY;
    }

    node = hybm_gvm_node_alloc(va, size);
    if (node == NULL) {
        return -ENOMEM;
    }

    mutex_lock(&node->lock); // 先加锁然后插入va_tree,占位
    if (!gvm_va_tree_insert(&proc->va_tree, (void *)node)) {
        mutex_unlock(&node->lock);
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        return -EBUSY;
    }

    if (arg->dma_flag) {
        gvm_set_flag(&node->flag, GVM_NODE_FLAG_NO_SHARE);
        gvm_set_flag(&node->flag, GVM_NODE_FLAG_DMA);
    }
    ret = hybm_gvm_mem_alloc_inner(proc, node);
    mutex_unlock(&node->lock);
    if (ret != 0) {
        hybm_gvm_err("alloc mem failed, ret:%d. input:size(0x%llx)", ret, size);
        gvm_va_tree_remove_and_dec(&proc->va_tree, (void *)node, hybm_gvm_node_ref_release);
        return -EBUSY;
    }
    hybm_gvm_debug("alloc mem success. input:size(0x%llx)dma(%u)", size, arg->dma_flag);
    return 0;
}

static int hybm_gvm_mem_free(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_mem_free_para *arg = &args->data.mem_free_para;
    struct gvm_node *node = NULL;
    u64 va = arg->addr;

    node = (struct gvm_node *)gvm_va_tree_search_and_inc(&proc->va_tree, va);
    if (node == NULL) {
        hybm_gvm_err("cannot find mem starts with input va");
        return -EINVAL;
    }

    if (gvm_check_flag(&node->flag, GVM_NODE_FLAG_REMOTE)) {
        hybm_gvm_err("this mem is opened, please call close func.");
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        return -EBUSY;
    }

    hybm_gvm_node_free(proc, node);
    hybm_gvm_debug("free mem success.");
    return 0;
}

static void hybm_gvm_free_all_mem(struct hybm_gvm_process *proc)
{
    struct gvm_node *node = NULL;

    node = (struct gvm_node *)gvm_va_tree_get_first_and_inc(&proc->va_tree);
    while (node != NULL) {
        hybm_gvm_node_free(proc, node);
        node = (struct gvm_node *)gvm_va_tree_get_first_and_inc(&proc->va_tree);
    }
}

static int hybm_gvm_get_key(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_get_key_para *arg = &args->data.get_key_para;
    struct gvm_node *node = NULL;
    u64 va = arg->addr;
    int ret = 0;

    node = (struct gvm_node *)gvm_va_tree_search_and_inc(&proc->va_tree, va);
    if (node == NULL) {
        hybm_gvm_err("cannot find mem starts with input va");
        return -EINVAL;
    }

    if (gvm_check_flag(&node->flag, GVM_NODE_FLAG_NO_SHARE)) {
        hybm_gvm_err("set key failed, this mem can't be shared.");
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        return -EINVAL;
    }

    if (!gvm_check_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED)) {
        hybm_gvm_err("set key failed, this mem is not allocated.");
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        return -EINVAL;
    }

    mutex_lock(&node->lock);
    if (node->shm_key == 0) {
        if (proc->key_idx == U32_MAX) {
            hybm_gvm_err("set key failed, available key exhausted!.");
            ret = -EBUSY;
        } else {
            node->shm_key = gvm_generate_key(proc->sdid, ++proc->key_idx);
            if (!gvm_key_tree_insert(&proc->key_tree, node)) {
                hybm_gvm_err("set key failed, duplicated key exists! maybe retry. key(0x%llx)", node->shm_key);
                node->shm_key = 0;
                ret = -EAGAIN;
            }
        }
    }

    mutex_unlock(&node->lock);
    kref_put(&node->ref, hybm_gvm_node_ref_release);
    arg->key = node->shm_key;
    if (ret == 0) {
        hybm_gvm_debug("get mem key success. key(0x%llx)", arg->key);
    }
    return ret;
}

static int hybm_gvm_set_whitelist(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_set_whitelist_para *arg = &args->data.set_wl_para;
    struct gvm_node *node = NULL;
    struct gvm_wlist_node *wnode = NULL;
    int ret;

    if (gvm_get_sdid_by_key(arg->key) != proc->sdid) {
        hybm_gvm_err("input key is not from local device. key(0x%llx)", arg->key);
        return -EINVAL;
    }

    if (arg->sdid == proc->sdid) {
        hybm_gvm_err("input sdid is same as local process. input:%u local:%u", arg->sdid, proc->sdid);
        return -EINVAL;
    }

    node = gvm_key_tree_search_and_inc(&proc->key_tree, arg->key);
    if (node == NULL) {
        hybm_gvm_err("input key is absent. key(0x%llx)", arg->key);
        return -EINVAL;
    }

    wnode = kzalloc(sizeof(struct gvm_wlist_node), GFP_KERNEL | __GFP_ACCOUNT);
    if (wnode == NULL) {
        hybm_gvm_err("kzalloc gvm_wlist_node fail.");
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        return -ENOMEM;
    }

    wnode->sdid = arg->sdid;
    INIT_LIST_HEAD(&wnode->node);
    ret = hybm_gvm_p2p_set_wl(proc->sdid, proc->devid, arg->sdid, node, &wnode->rproc);
    if (ret != 0) {
        list_del_init(&wnode->node);
        kfree(wnode);
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        if (ret == SET_WL_RET_DUPLICATIVE) {
            hybm_gvm_debug("already set duplicated whitelist. input:key(0x%llx),sdid(0x%x)", arg->key, arg->sdid);
            return 0;
        } else {
            hybm_gvm_err("set white list fail. ret(%d)key(0x%llx)sdid(0x%x)", ret, arg->key, arg->sdid);
            return -EBUSY;
        }
    }

    mutex_lock(&node->lock);
    list_add_tail(&wnode->node, &node->wlist_head);
    mutex_unlock(&node->lock);
    kref_put(&node->ref, hybm_gvm_node_ref_release);
    hybm_gvm_debug("hybm_gvm_set_whitelist success. input:key(0x%llx),sdid(0x%x)", arg->key, arg->sdid);
    return 0;
}

static int hybm_gvm_mem_open(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_mem_open_para *arg = &args->data.mem_open_para;
    struct gvm_node *node = NULL;
    u64 va = arg->addr, key = arg->key;
    int ret;

    if (gvm_get_sdid_by_key(key) == proc->sdid) {
        hybm_gvm_err("input key is not from remote device. key(0x%llx)", key);
        return -EINVAL;
    }

    node = gvm_key_tree_search_and_inc(&proc->key_tree, key);
    if (node == NULL) {
        hybm_gvm_err("input key has not been set in whitelist! key(0x%llx)", key);
        return -EINVAL;
    }

    if (node->va != va) {
        hybm_gvm_err("input va is not match the local va. key(0x%llx)", key);
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        return -EBUSY;
    }

    ret = hybm_gvm_p2p_get_addr(proc, proc->sdid, proc->devid, gvm_get_sdid_by_key(key), node);
    if (ret != 0) {
        hybm_gvm_err("open mem failed! key(0x%llx), ret:%d", key, ret);
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        return ret;
    }

    kref_put(&node->ref, hybm_gvm_node_ref_release);
    hybm_gvm_debug("hybm_gvm_mem_open success. key(0x%llx)", arg->key);
    return 0;
}

static int hybm_gvm_mem_close(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_mem_close_para *arg = &args->data.mem_close_para;
    struct gvm_node *node = NULL;
    u64 va = arg->addr;

    node = (struct gvm_node *)gvm_va_tree_search_and_inc(&proc->va_tree, va);
    if (node == NULL) {
        hybm_gvm_err("mem is not open!");
        return -EINVAL;
    }

    if (!gvm_check_flag(&node->flag, GVM_NODE_FLAG_REMOTE) || !gvm_check_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED)) {
        hybm_gvm_err("mem is not remote! key(0x%llx),flag(0x%llx)", node->shm_key, node->flag);
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        return -EBUSY;
    }

    mutex_lock(&node->lock);
    hybm_gvm_mem_free_inner(proc, node, UINT64_MAX);
    mutex_unlock(&node->lock);
    hybm_gvm_debug("hybm_gvm_mem_close success. key(0x%llx)", node->shm_key);
    kref_put(&node->ref, hybm_gvm_node_ref_release);
    return 0;
}

static int hybm_gvm_mem_fetch(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_mem_fetch_para *arg = &args->data.mem_fetch_para;
    struct gvm_dev_mem_node *node = NULL;
    u64 va = arg->addr;
    u64 size = arg->size;
    int ret;

    if (!va || !size || (size > UINT64_MAX - va) ||
        (!arg->no_record && (va % HYBM_GVM_PAGE_SIZE || size != HYBM_GVM_PAGE_SIZE))) {
        hybm_gvm_err("input invalid! size(0x%llx)no_record(%u)", size, arg->no_record);
        return -EBUSY;
    }

    if (arg->no_record) {
        va = arg->new_addr;
    }

    node = hybm_gvm_dev_node_alloc(va, size);
    if (node == NULL) {
        return -ENOMEM;
    }

    if (arg->no_record) {
        node->pa_num = 0;
        ret = hybm_gvm_to_agent_register(proc->devid, proc->pasid, arg->addr, size, va, &node->page_size);
    } else if (proc->sdid == arg->sdid) {
        node->pa_num = HYBM_GVM_PAGE_NUM;
        ret = hybm_gvm_to_agent_fetch(proc->devid, proc->pasid, va, size, &node->page_size, node->pa, HYBM_GVM_PAGE_NUM);
    } else {
        node->pa_num = HYBM_GVM_PAGE_NUM;
        node->page_size = HYBM_HPAGE_SIZE;
        ret = hybm_gvm_p2p_fetch(proc->sdid, proc->devid, arg->sdid, va, node->pa);
        if (ret == 0) {
            ret = hybm_gvm_to_agent_mmap(proc->devid, proc->pasid, va, size, node->pa, HYBM_GVM_PAGE_NUM);
        }
    }
    if (ret != 0) {
        hybm_gvm_err("fetch mem failed! size(0x%llx)", size);
        kref_put(&node->ref, hybm_gvm_dev_node_ref_release);
        return ret;
    }

    if (!gvm_va_tree_insert(&proc->fetch_tree, (void *)node)) {
        hybm_gvm_to_agent_unmap(proc->devid, proc->pasid, node->va, node->size, node->page_size);
        kref_put(&node->ref, hybm_gvm_dev_node_ref_release);
        return -EBUSY;
    }

    hybm_gvm_debug("hybm_gvm_mem_fetch success. size(0x%llx)no_record(%u)", size, arg->no_record);
    return 0;
}

struct gvm_ioctl_handlers_st gvm_ioctl_handlers[HYBM_GVM_CMD_MAX_CMD] = {
    [_IOC_NR(HYBM_GVM_CMD_PROC_CREATE)] = {hybm_gvm_proc_create},
    [_IOC_NR(HYBM_GVM_CMD_MEM_ALLOC)] = {hybm_gvm_mem_alloc},
    [_IOC_NR(HYBM_GVM_CMD_MEM_FREE)] = {hybm_gvm_mem_free},
    [_IOC_NR(HYBM_GVM_CMD_GET_KEY)] = {hybm_gvm_get_key},
    [_IOC_NR(HYBM_GVM_CMD_SET_WL)] = {hybm_gvm_set_whitelist},
    [_IOC_NR(HYBM_GVM_CMD_MEM_OPEN)] = {hybm_gvm_mem_open},
    [_IOC_NR(HYBM_GVM_CMD_MEM_CLOSE)] = {hybm_gvm_mem_close},
    [_IOC_NR(HYBM_GVM_CMD_MEM_FETCH)] = {hybm_gvm_mem_fetch},
};

void hybm_gvm_proc_destroy(struct gvm_private_data *priv)
{
    struct hybm_gvm_process *proc = priv->process;
    struct gvm_dev_mem_node *node = NULL;
    int ret;

    node = (struct gvm_dev_mem_node *)gvm_va_tree_get_first_and_inc(&proc->fetch_tree);
    while (node != NULL) {
        ret = hybm_gvm_to_agent_unmap(proc->devid, proc->pasid, node->va, node->size, node->page_size);
        if (ret != 0) {
            hybm_gvm_err("unmap fetch mem failed! size(0x%llx),ret:%d", node->size, ret);
        }
        gvm_va_tree_remove_and_dec(&proc->fetch_tree, (void *)node, hybm_gvm_dev_node_ref_release); // 减去init时加的计数
        kref_put(&node->ref, hybm_gvm_dev_node_ref_release); // 减去search时加的计数

        node = (struct gvm_dev_mem_node *)gvm_va_tree_get_first_and_inc(&proc->fetch_tree);
    }

    hybm_gvm_free_all_mem(proc);

    write_lock_bh(&proc_rwlock);
    g_proc_list[proc->devid] = NULL;
    write_unlock_bh(&proc_rwlock);
    kref_put(&proc->ref, hybm_proc_ref_release);
    priv->process = NULL;
}

static int hybm_gvm_proc_get_pa_inner(struct hybm_gvm_process *proc, u64 va, u64 size, u64 *arr, u64 num)
{
    struct gvm_node *node = NULL;
    u64 i;

    node = (struct gvm_node *)gvm_va_tree_search_and_inc(&proc->va_tree, va);
    if (node == NULL) {
        hybm_gvm_err("cannot find mem starts with input va");
        return -EINVAL;
    }
    if (node->va != va || node->size != size || node->pa_num != num) {
        hybm_gvm_err("input va,size,pa_num not match. input:size(0x%llx)pa_num(%llu) range:size(0x%llx)pa_num(%u)",
                     size, num, node->size, node->pa_num);
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        return -EINVAL;
    }
    if (!gvm_check_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED) || gvm_check_flag(&node->flag, GVM_NODE_FLAG_NO_SHARE)) {
        hybm_gvm_err("node flag error. size(0x%llx)flag(0x%llx)", size, node->flag);
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        return -EINVAL;
    }

    for (i = 0; i < num; i++) {
        arr[i] = node->pmem[i].pa;
        if (arr[i] == 0) {
            hybm_gvm_err("pa is zero. idx(%llu)", i);
            kref_put(&node->ref, hybm_gvm_node_ref_release);
            return -EINVAL;
        }
        arr[i] = gvm_get_global_pa_from_local(arr[i], proc->sdid);
        if (arr[i] == 0) {
            hybm_gvm_err("get global pa failed. idx(%llu)", i);
            kref_put(&node->ref, hybm_gvm_node_ref_release);
            return -EINVAL;
        }
    }
    kref_put(&node->ref, hybm_gvm_node_ref_release);
    return 0;
}

int hybm_gvm_proc_get_pa(u32 sdid, u64 va, u64 size, u64 *arr, u64 max_num)
{
    struct hybm_gvm_process *proc = hybm_gvm_get_proc_by_sdid(sdid);
    u64 i;
    int ret;
    for (i = 0; i < max_num; i++)
        arr[i] = 0; // init

    if (proc == NULL) {
        return -EINVAL;
    }

    if (size % HYBM_GVM_PAGE_SIZE || size / HYBM_GVM_PAGE_SIZE > max_num) {
        hybm_gvm_err("input size is invalid. size(0x%llx)num(%llu)", size, max_num);
        kref_put(&proc->ref, hybm_proc_ref_release);
        return -EINVAL;
    }

    ret = hybm_gvm_proc_get_pa_inner(proc, va, size, arr, size / HYBM_GVM_PAGE_SIZE);
    kref_put(&proc->ref, hybm_proc_ref_release);
    hybm_gvm_debug("hybm_gvm_proc_get_pa size:0x%llx", size);
    return ret;
}

int hybm_gvm_proc_set_pa(struct hybm_gvm_process *proc, u32 sdid, struct gvm_node *node, u64 *arr, u64 num)
{
    u64 i, lpa;
    int ret;

    if (!gvm_check_flag(&node->flag, GVM_NODE_FLAG_REMOTE)) {
        hybm_gvm_err("set remote pa failed, node is local. flag(0x%llx)", node->flag);
        return -EINVAL;
    }
    if (num != node->pa_num) {
        hybm_gvm_err("node size is not equal arr num. node_num(%llu),arr_num(%u)", num, node->pa_num);
        return -EINVAL;
    }

    mutex_lock(&node->lock);
    for (i = 0; i < num; i++) {
        if (node->pmem[i].pa != 0) {
            hybm_gvm_err("mem has already been opened or allocated. idx(%llu)", i);
            mutex_unlock(&node->lock);
            return -EINVAL;
        }
    }

    for (i = 0; i < num; i++) {
        node->pmem[i].pa = arr[i];
        lpa = arr[i];
        if (gvm_get_server_id_by_sdid(sdid) == gvm_get_server_id_by_sdid(proc->sdid)) {
            lpa = gvm_get_local_pa_from_global(lpa, sdid);
        }

        ret = hybm_gvm_to_agent_mmap(proc->devid, proc->pasid, node->va + i * HYBM_GVM_PAGE_SIZE,
                                     HYBM_GVM_PAGE_SIZE, &lpa, 1U);
        if (ret != 0) {
            hybm_gvm_err("map device sdma failed. ret:%d", ret);
            hybm_gvm_mem_free_inner(proc, node, i);
            mutex_unlock(&node->lock);
            return ret;
        }
        hybm_gvm_debug("open mem and map. idx(%llu)", i);
    }

    gvm_set_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED);
    mutex_unlock(&node->lock);
    return 0;
}

static int hybm_gvm_insert_remote_inner(struct hybm_gvm_process *proc, u64 key, u64 va, u64 size)
{
    struct gvm_node *node = NULL;
    int ret = 0;

    node = gvm_key_tree_search_and_inc(&proc->key_tree, key);
    if (node != NULL) {
        ret = SET_WL_RET_DUPLICATIVE;
        if (node->shm_key != key || node->va != va || node->size != size) {
            hybm_gvm_err("node key,va,size not match. local:key(0x%llx)size(0x%llx) input:key(0x%llx)size(0x%llx)",
                         node->shm_key, node->size, key, size);
            ret = -EBUSY;
        }
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        return ret;
    }

    node = hybm_gvm_node_alloc(va, size);
    if (node == NULL) {
        hybm_gvm_err("kzalloc gvm node fail.");
        return -ENOMEM;
    }

    node->shm_key = key;
    gvm_set_flag(&node->flag, GVM_NODE_FLAG_REMOTE);
    mutex_lock(&node->lock); // 先加锁然后插入va_tree,占位
    if (!gvm_va_tree_insert(&proc->va_tree, (void *)node)) {
        mutex_unlock(&node->lock);
        kref_put(&node->ref, hybm_gvm_node_ref_release);
        return -EBUSY;
    }

    if (!gvm_key_tree_insert(&proc->key_tree, node)) {
        hybm_gvm_err("set key failed, unknown error. key(0x%llx)", node->shm_key);
        mutex_unlock(&node->lock);
        gvm_va_tree_remove_and_dec(&proc->va_tree, (void *)node, hybm_gvm_node_ref_release);
        return -EBUSY;
    }

    mutex_unlock(&node->lock);
    return 0;
}

int hybm_gvm_insert_remote(u32 sdid, u64 key, u64 va, u64 size, struct hybm_gvm_process **rp)
{
    struct hybm_gvm_process *proc = hybm_gvm_get_proc_by_sdid(sdid);
    int ret;
    if (proc == NULL) {
        return -EINVAL;
    }

    ret = hybm_gvm_insert_remote_inner(proc, key, va, size);
    *rp = proc;
    kref_put(&proc->ref, hybm_proc_ref_release);
    hybm_gvm_debug("hybm_gvm_insert_remote key:0x%llx size:0x%llx", key, size);
    return ret;
}

static int hybm_gvm_remove_remote_inner(struct hybm_gvm_process *proc, u64 key)
{
    struct gvm_node *node = NULL;
    node = gvm_key_tree_search_and_inc(&proc->key_tree, key);
    if (node == NULL) {
        hybm_gvm_warn("key has removed. key(0x%llx)", key);
        return -EINVAL;
    }

    if (gvm_check_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED)) {
        mutex_lock(&node->lock);
        hybm_gvm_mem_free_inner(proc, node, UINT64_MAX);
        mutex_unlock(&node->lock);
    }

    gvm_key_tree_remove(&proc->key_tree, node, hybm_gvm_node_ref_release);
    gvm_va_tree_remove_and_dec(&proc->va_tree, (void *)node, hybm_gvm_node_ref_release); // 减去init时加的计数
    kref_put(&node->ref, hybm_gvm_node_ref_release); // 减去本次计数
    return 0;
}

int hybm_gvm_unset_remote(u32 sdid, u64 key, struct hybm_gvm_process *rp)
{
    struct hybm_gvm_process *proc = hybm_gvm_get_proc_by_sdid(sdid);
    int ret;
    if (proc == NULL) {
        return -EINVAL;
    }

    if (proc != rp) {
        hybm_gvm_warn("proc has changed! key(0x%llx)", key);
        kref_put(&proc->ref, hybm_proc_ref_release);
        return 0;
    }

    ret = hybm_gvm_remove_remote_inner(proc, key);
    kref_put(&proc->ref, hybm_proc_ref_release);
    if (ret != 0) {
        hybm_gvm_warn("key has changed! key(0x%llx)", key);
    }
    hybm_gvm_debug("hybm_gvm_unset_remote key:0x%llx sdid:0x%x", key, sdid);
    return 0;
}

int hybm_gvm_fetch_remote(u32 sdid, u64 va, u64 *pa_list)
{
    struct hybm_gvm_process *proc = hybm_gvm_get_proc_by_sdid(sdid);
    struct gvm_dev_mem_node *node = NULL;
    u32 i;
    int ret = 0;

    if (proc == NULL) {
        return -EINVAL;
    }

    node = (struct gvm_dev_mem_node *)gvm_va_tree_search_and_inc(&proc->fetch_tree, va);
    if (node == NULL) {
        hybm_gvm_err("not found this va, maybe not fetch.");
        kref_put(&proc->ref, hybm_proc_ref_release);
        return -EINVAL;
    }

    if (node->pa_num != HYBM_GVM_PAGE_NUM) {
        hybm_gvm_err("invalid pa_num(%u)", node->pa_num);
        ret = -EINVAL;
    } else {
        for (i = 0; i < HYBM_GVM_PAGE_NUM; i++) {
            pa_list[i] = node->pa[i];
        }
    }

    kref_put(&node->ref, hybm_gvm_dev_node_ref_release);
    kref_put(&proc->ref, hybm_proc_ref_release);
    return ret;
}