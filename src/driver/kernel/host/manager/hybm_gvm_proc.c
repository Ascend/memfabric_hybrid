/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_gvm_proc.h"

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/dma-mapping.h>

#include "hybm_gvm_ioctl.h"
#include "hybm_gvm_log.h"
#include "hybm_gvm_p2p.h"
#include "hybm_gvm_phy_page.h"
#include "hybm_gvm_s2a.h"
#include "hybm_gvm_symbol_get.h"

#define GVM_NODE_FLAG_ALLOCATED 1ULL
#define GVM_NODE_FLAG_REMOTE    2ULL
#define GVM_NODE_FLAG_NO_SHARE  3ULL
#define GVM_NODE_FLAG_DMA       4ULL

#define GVM_KEY_SDID_OFFSET    32
#define GVM_SDID_SERVER_OFFSET 22
#define GVM_GLOBAL_SERVER_NUM  48
#define GVM_GLOBAL_PA_MASK     (0xffffffffffffULL) // 256T (2^48)

struct hybm_gvm_process *g_proc_list[HYBM_MAX_DEVICE_NUM] = {NULL};

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

static u64 g_global_pa_offset[48] = {
    [3] = 0xb94000000000ULL,
    [18] = 0xb84000000000ULL,
};

static u64 gvm_get_local_pa_from_global(u64 pa, u32 sdid)
{
    u32 server_id = gvm_get_server_id_by_sdid(sdid);
    u64 offset;
    if (server_id >= GVM_GLOBAL_SERVER_NUM) {
        hybm_gvm_err("translate failed, server id is invalid(%u)", server_id);
        return 0ULL;
    }

    offset = g_global_pa_offset[server_id];
    if (offset == 0ULL) {
        hybm_gvm_err("translate failed, server id is not support(%u)", server_id);
        return 0ULL;
    }

    return (pa + offset) & GVM_GLOBAL_PA_MASK;
}

static u64 gvm_get_global_pa_from_local(u64 pa, u32 sdid)
{
    u32 server_id = gvm_get_server_id_by_sdid(sdid);
    u64 offset;
    if (server_id >= GVM_GLOBAL_SERVER_NUM) {
        hybm_gvm_err("translate failed, server id is invalid(%u)", server_id);
        return 0ULL;
    }

    offset = g_global_pa_offset[server_id];
    if (offset == 0ULL) {
        hybm_gvm_err("translate failed, server id is not support(%u)", server_id);
        return 0ULL;
    }

    return (pa - offset) & GVM_GLOBAL_PA_MASK;
}

static bool gvm_tree_insert(struct rb_root *root, struct gvm_node *data)
{
    struct rb_node **new = &(root->rb_node);
    struct rb_node *parent = NULL;
    while (*new) {
        struct gvm_node *tmp = container_of(*new, struct gvm_node, tree_node);
        parent = *new;
        if (data->shm_key < tmp->shm_key) {
            new = &((*new)->rb_left);
        } else if (data->shm_key > tmp->shm_key) {
            new = &((*new)->rb_right);
        } else {
            return false;
        }
    }
    /* Add new node and rebalance tree. */
    rb_link_node(&data->tree_node, parent, new);
    rb_insert_color(&data->tree_node, root);
    return true;
}

static void gvm_tree_remove(struct rb_root *root, struct gvm_node *data)
{
    rb_erase(&data->tree_node, root);
}

static struct gvm_node *gvm_tree_search(struct rb_root *root, u64 key)
{
    struct rb_node *node = root->rb_node;
    while (node) {
        struct gvm_node *data = container_of(node, struct gvm_node, tree_node);
        if (key < data->shm_key) {
            node = node->rb_left;
        } else if (key > data->shm_key) {
            node = node->rb_right;
        } else {
            return data;
        }
    }
    return NULL;
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

    gvm_proc->mem_array = kzalloc(sizeof(struct mem_node) * num, GFP_KERNEL | __GFP_ACCOUNT);
    if (gvm_proc->mem_array == NULL) {
        hybm_gvm_err("kzalloc gvm mem array fail.");
        return -ENOMEM;
    }

    gvm_proc->va_start = start;
    gvm_proc->va_end = end;
    gvm_proc->va_page_num = num;
    gvm_proc->key_tree = RB_ROOT;
    INIT_LIST_HEAD(&gvm_proc->fetch_head);
    init_rwsem(&gvm_proc->ioctl_rwsem);
    return 0;
}

static void hybm_free_gvm_proc(struct hybm_gvm_process **gvm_proc)
{
    if (*gvm_proc == NULL) {
        return;
    }

    if ((*gvm_proc)->mem_array != NULL) {
        kfree((*gvm_proc)->mem_array);
        (*gvm_proc)->mem_array = NULL;
    }

    kfree((*gvm_proc));
    (*gvm_proc) = NULL;
}

static struct hybm_gvm_process *hybm_gvm_get_proc_by_sdid(u32 sdid)
{
    u32 devid = gvm_get_devid_by_sdid(sdid);
    struct hybm_gvm_process *proc = NULL;

    if (devid >= HYBM_MAX_DEVICE_NUM) {
        hybm_gvm_err("devid is invalid. sdid(%x)", sdid);
        return NULL;
    }

    proc = g_proc_list[devid];
    if (proc == NULL || proc->initialized != true) {
        hybm_gvm_err("local proc not init!. sdid(%x)", sdid);
        return NULL;
    }
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

    if (arch_cmpxchg64((u64 *)&g_proc_list[devid], 0ULL, (u64)gvm_proc) != 0ULL) {
        hybm_gvm_err("has same devid proc! old_proc:%p now:%p", g_proc_list[devid], gvm_proc);
        hybm_free_gvm_proc(&gvm_proc);
        return NULL;
    }

    hybm_gvm_info("create gvm proc:%p", gvm_proc);
    return gvm_proc;
}

static int hybm_gvm_proc_create(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_process *gvm_proc = NULL;
    struct hybm_gvm_proc_create_para *arg = &args->data.proc_create_para;

    if (gvm_test_and_set_init_flag(file) == true) {
        hybm_gvm_err("gvm already initialized.");
        return -EALREADY;
    }

    if (arg->start >= arg->end || (arg->end - arg->start) % HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input va range is invalid. st(0x%llx),ed(0x%llx)", arg->start, arg->end);
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

static struct gvm_node *hybm_gvm_search_in_range(struct hybm_gvm_process *proc, u64 va, u64 size)
{
    u64 st = (va - proc->va_start) / HYBM_GVM_PAGE_SIZE;
    u64 ed = st + size / HYBM_GVM_PAGE_SIZE;
    while (st < ed && st < proc->va_page_num) {
        if (proc->mem_array[st].head_node != NULL) {
            return proc->mem_array[st].head_node;
        }
        st++;
    }

    return NULL;
}

static struct gvm_node *hybm_gvm_get_node(struct hybm_gvm_process *proc, u64 va)
{
    struct gvm_node *node = NULL;
    u64 idx = 0;

    if (va % HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input param is not a multiple of page. va(0x%llx)", va);
        return NULL;
    }

    if (va < proc->va_start || va >= proc->va_end) {
        hybm_gvm_err("input param is out of va range. input:va(0x%llx) range:start(0x%llx),end(0x%llx)", va,
                     proc->va_start, proc->va_end);
        return NULL;
    }

    idx = (va - proc->va_start) / HYBM_GVM_PAGE_SIZE;
    node = (struct gvm_node *)proc->mem_array[idx].head_node;
    if (node == NULL) {
        hybm_gvm_err("input va is not allocated yet. va(0x%llx)", va);
        return NULL;
    }

    if (node->va != va) {
        hybm_gvm_err("input va is not match the allocated start addr. va(0x%llx),start(0x%llx)", va, node->va);
        return NULL;
    }

    return node;
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

    vma = proc->vma;
    if (!pfn_valid(pfn)) {
        hybm_gvm_err("Invalid PFN: 0x%llx", pfn);
        return -EINVAL;
    }

    if (vma->vm_end < virt_addr + size || vma->vm_start > virt_addr) {
        hybm_gvm_err("vma range is small, va:0x%llx,size:0x%llx,st:0x%lx,ed:0x%lx", virt_addr, size, vma->vm_start,
                     vma->vm_end);
        return -EINVAL;
    }

    ret = remap_pfn_range(vma, virt_addr, pfn, size, vma->vm_page_prot);
    return ret;
}

static void hybm_gvm_unmap_phys(struct mm_struct *mm, u64 virt_addr, u64 size)
{
    // TODO
}

static void hybm_gvm_dma_unmap(struct hybm_gvm_process *proc, u64 *pa_list, u32 num)
{
    u32 i;
    struct device *dev = uda_get_device(proc->devid);

    for (i = 0; i < num; i++) {
        if (dma_mapping_error(dev, pa_list[i]) == 0) {
            dma_unmap_page(dev, pa_list[i], HYBM_HPAGE_SIZE, DMA_BIDIRECTIONAL);
        }
    }
    hybm_gvm_debug("hybm_gvm_dma_unmap, pa:0x%llx", pa_list[0]);
}

static int hybm_gvm_dma_map(struct hybm_gvm_process *proc, u64 pa, u64 *pa_list, u32 num)
{
    u32 i;
    struct device *dev = uda_get_device(proc->devid);
    struct page *pg;
    int ret;

    for (i = 0; i < num; i++) {
        pg = pfn_to_page(PFN_DOWN(pa + HYBM_HPAGE_SIZE * i));
        pa_list[i] = devdrv_dma_map_page(dev, pg, 0, HYBM_HPAGE_SIZE, DMA_BIDIRECTIONAL);
        ret = dma_mapping_error(dev, pa_list[i]);
        if (ret != 0) {
            hybm_gvm_err("dma map failed, pa:0x%llx,ret:%d", pa + HYBM_HPAGE_SIZE * i, ret);
            hybm_gvm_dma_unmap(proc, pa_list, i);
            return -EINVAL;
        }
    }

    hybm_gvm_debug("hybm_gvm_dma_map, pa:0x%llx ret_pa:0x%llx", pa, pa_list[0]);
    return 0;
}

static int hybm_gvm_mem_add_dma(struct hybm_gvm_process *proc, u32 idx)
{
    u64 *pa_list = NULL;
    int ret;

    if (idx >= proc->va_page_num || proc->mem_array[idx].pa == 0 || proc->mem_array[idx].dma_list) {
        hybm_gvm_err("input idx is invalid.");
        return -EINVAL;
    }

    pa_list = kzalloc(sizeof(u64) * HYBM_GVM_PAGE_NUM, GFP_KERNEL | __GFP_ACCOUNT);
    if (pa_list == NULL) {
        hybm_gvm_err("kzalloc pa_list fail.");
        return -ENOMEM;
    }

    ret = hybm_gvm_dma_map(proc, proc->mem_array[idx].pa, pa_list, HYBM_GVM_PAGE_NUM);
    if (ret != 0) {
        kfree(pa_list);
        return ret;
    }

    proc->mem_array[idx].dma_list = pa_list;
    return 0;
}

static void hybm_gvm_mem_free_inner(struct hybm_gvm_process *proc, struct gvm_node *node, u64 skip_id)
{
    u64 st = (node->va - proc->va_start) / HYBM_GVM_PAGE_SIZE;
    u64 ed = st + node->size / HYBM_GVM_PAGE_SIZE;

    hybm_gvm_debug("hybm_gvm_mem_free, va:0x%llx size:0x%llx", node->va, node->size);

    if (ed >= proc->va_page_num) {
        hybm_gvm_err("input va is out of va range. idx_ed(%llu),va_page_num(%llu)", ed, proc->va_page_num);
        return;
    }

    while (st < ed) {
        if (proc->mem_array[st].pa != 0 && st != skip_id) {
            hybm_gvm_to_agent_unmap(proc->devid, proc->pasid, proc->va_start + st * HYBM_GVM_PAGE_SIZE,
                                    HYBM_GVM_PAGE_SIZE, HYBM_GVM_PAGE_SIZE);
            hybm_gvm_unmap_phys(proc->mm, proc->va_start + st * HYBM_GVM_PAGE_SIZE, HYBM_GVM_PAGE_SIZE);
        }
        if (proc->mem_array[st].pa != 0 && !gvm_check_flag(&node->flag, GVM_NODE_FLAG_REMOTE)) {
            hybm_gvm_free_pg(proc->mem_array[st].pa);
        }
        if (gvm_check_flag(&node->flag, GVM_NODE_FLAG_DMA) && proc->mem_array[st].dma_list) {
            hybm_gvm_dma_unmap(proc, proc->mem_array[st].dma_list, HYBM_GVM_PAGE_NUM);
            kfree(proc->mem_array[st].dma_list);
            proc->mem_array[st].dma_list = NULL;
        }

        proc->mem_array[st].pa = 0;
        proc->mem_array[st].head_node = NULL;
        st++;
    }
}

static int hybm_gvm_mem_alloc_inner(struct hybm_gvm_process *proc, struct gvm_node *node)
{
    u64 st = (node->va - proc->va_start) / HYBM_GVM_PAGE_SIZE;
    u64 ed = st + node->size / HYBM_GVM_PAGE_SIZE;
    int ret;

    if (ed >= proc->va_page_num) {
        hybm_gvm_err("input va is out of va range. idx_ed(%llu),va_page_num(%llu)", ed, proc->va_page_num);
        return -EINVAL;
    }

    while (st < ed) {
        proc->mem_array[st].pa = hybm_gvm_alloc_pg();
        if (proc->mem_array[st].pa == 0) {
            hybm_gvm_err("alloc pa failed.");
            ret = -EBUSY;
            goto failed_return;
        }

        proc->mem_array[st].head_node = (void *)node;
        ret = hybm_gvm_map_phys_to_user(proc, proc->va_start + st * HYBM_GVM_PAGE_SIZE, proc->mem_array[st].pa,
                                        HYBM_GVM_PAGE_SIZE);
        if (ret != 0) {
            hybm_gvm_err("map host mem failed. ret:%d", ret);
            goto failed_return;
        }

        if (gvm_check_flag(&node->flag, GVM_NODE_FLAG_DMA)) {
            ret = hybm_gvm_mem_add_dma(proc, st);
            if (ret != 0) {
                hybm_gvm_err("dma host mem failed. ret:%d", ret);
                hybm_gvm_unmap_phys(proc->mm, proc->va_start + st * HYBM_GVM_PAGE_SIZE, HYBM_GVM_PAGE_SIZE);
                goto failed_return;
            }

            ret = hybm_gvm_to_agent_mmap(proc->devid, proc->pasid, proc->va_start + st * HYBM_GVM_PAGE_SIZE,
                                         HYBM_GVM_PAGE_SIZE, proc->mem_array[st].dma_list, HYBM_GVM_PAGE_NUM);
        } else {
            ret = hybm_gvm_to_agent_mmap(proc->devid, proc->pasid, proc->va_start + st * HYBM_GVM_PAGE_SIZE,
                                         HYBM_GVM_PAGE_SIZE, &proc->mem_array[st].pa, 1U);
        }
        if (ret != 0) {
            hybm_gvm_err("map device sdma failed. ret:%d", ret);
            hybm_gvm_unmap_phys(proc->mm, proc->va_start + st * HYBM_GVM_PAGE_SIZE, HYBM_GVM_PAGE_SIZE);
            goto failed_return;
        }

        st++;
    }

    gvm_set_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED);
    return 0;

failed_return:
    hybm_gvm_mem_free_inner(proc, node, st);
    return ret;
}

static int hybm_gvm_mem_alloc(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_mem_alloc_para *arg = &args->data.mem_alloc_para;
    struct gvm_node *node = NULL;
    u64 va = arg->addr;
    u64 size = arg->size;
    int ret;

    if (va % HYBM_GVM_PAGE_SIZE || size % HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input param is not a multiple of page. va(0x%llx),size(0x%llx)", va, size);
        return -EINVAL;
    }

    if (va < proc->va_start || va + size > proc->va_end) {
        hybm_gvm_err("input param is out of va range. input:va(0x%llx),size(0x%llx) range:va(0x%llx),size(0x%llx)\n",
                     va, size, proc->va_start, proc->va_end);
        return -EINVAL;
    }

    node = hybm_gvm_search_in_range(proc, va, size);
    if (node != NULL) {
        hybm_gvm_err("input range has already been allocated. input:va(0x%llx),size(0x%llx) "
                     "allocated:va(0x%llx),size(0x%llx)", va, size, node->va, node->size);
        return -EBUSY;
    }

    node = kzalloc(sizeof(struct gvm_node), GFP_KERNEL | __GFP_ACCOUNT);
    if (node == NULL) {
        hybm_gvm_err("kzalloc gvm node fail.");
        return -ENOMEM;
    }

    node->va = va;
    node->size = size;
    if (arg->dma_flag) {
        gvm_set_flag(&node->flag, GVM_NODE_FLAG_NO_SHARE);
        gvm_set_flag(&node->flag, GVM_NODE_FLAG_DMA);
    }
    INIT_LIST_HEAD(&node->wlist_head);
    ret = hybm_gvm_mem_alloc_inner(proc, node);
    if (ret != 0) {
        hybm_gvm_err("alloc mem failed, ret:%d. input:va(0x%llx),size(0x%llx)", ret, va, size);
        kfree(node);
        return -EBUSY;
    }

    hybm_gvm_debug("alloc mem success. input:va(0x%llx),size(0x%llx)flag(0x%llx)", va, size, node->flag);
    return 0;
}

static int hybm_gvm_mem_free(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_mem_free_para *arg = &args->data.mem_free_para;
    struct gvm_node *node = NULL;
    struct gvm_wlist_node *wnode = NULL;
    struct list_head *pos = NULL, *n = NULL;
    u64 va = arg->addr;

    node = hybm_gvm_get_node(proc, va);
    if (node == NULL) {
        return -EINVAL;
    }

    if (gvm_check_flag(&node->flag, GVM_NODE_FLAG_REMOTE)) {
        hybm_gvm_err("this mem is opened, please call close func. va(0x%llx)", va);
        return -EBUSY;
    }

    hybm_gvm_mem_free_inner(proc, node, UINT64_MAX);
    gvm_clear_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED);

    list_for_each_safe(pos, n, &node->wlist_head)
    {
        wnode = list_entry(pos, struct gvm_wlist_node, node);
        hybm_gvm_p2p_unset_wl(proc->sdid, proc->devid, wnode->sdid, node->shm_key, wnode->rproc);
        list_del_init(&wnode->node);
        kfree(wnode);
    }

    gvm_tree_remove(&proc->key_tree, node);
    kfree(node);
    hybm_gvm_debug("free mem success. input:va(0x%llx)", va);
    return 0;
}

static void hybm_gvm_free_all_mem(struct hybm_gvm_process *proc)
{
    u64 i;
    struct gvm_node *node = NULL;
    struct gvm_wlist_node *wnode = NULL;
    struct list_head *pos = NULL, *n = NULL;

    for (i = 0; i < proc->va_page_num; i++) {
        if (proc->mem_array[i].head_node == NULL) {
            continue;
        }

        node = (struct gvm_node *)proc->mem_array[i].head_node;

        hybm_gvm_mem_free_inner(proc, node, UINT64_MAX);
        gvm_clear_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED);

        if (!gvm_check_flag(&node->flag, GVM_NODE_FLAG_REMOTE)) {
            list_for_each_safe(pos, n, &node->wlist_head)
            {
                wnode = list_entry(pos, struct gvm_wlist_node, node);
                hybm_gvm_p2p_unset_wl(proc->sdid, proc->devid, wnode->sdid, node->shm_key, wnode->rproc);
                list_del_init(&wnode->node);
                kfree(wnode);
            }
        }

        gvm_tree_remove(&proc->key_tree, node);
        kfree(node);
    }
}

static int hybm_gvm_get_key(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_get_key_para *arg = &args->data.get_key_para;
    struct gvm_node *node = NULL;
    u64 va = arg->addr;

    node = hybm_gvm_get_node(proc, va);
    if (node == NULL) {
        return -EINVAL;
    }

    if (gvm_check_flag(&node->flag, GVM_NODE_FLAG_NO_SHARE)) {
        hybm_gvm_err("set key failed, this mem can't share. va(0x%llx)", va);
        return -EINVAL;
    }

    if (node->shm_key == 0) {
        node->shm_key = gvm_generate_key(proc->sdid, ++proc->key_idx);
        if (gvm_tree_insert(&proc->key_tree, node) == false) {
            hybm_gvm_err("set key failed, has duplicated key! maybe retry. key(0x%llx)", node->shm_key);
            node->shm_key = 0;
            return -EAGAIN;
        }
    }

    arg->key = node->shm_key;
    hybm_gvm_debug("get mem key success. va(0x%llx),key(0x%llx)", va, arg->key);
    return 0;
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

    node = gvm_tree_search(&proc->key_tree, arg->key);
    if (node == NULL) {
        hybm_gvm_err("input key is absent. key(0x%llx)", arg->key);
        return -EINVAL;
    }

    wnode = kzalloc(sizeof(struct gvm_wlist_node), GFP_KERNEL | __GFP_ACCOUNT);
    if (wnode == NULL) {
        hybm_gvm_err("kzalloc gvm_wlist_node fail.");
        return -ENOMEM;
    }

    wnode->sdid = arg->sdid;
    INIT_LIST_HEAD(&wnode->node);
    list_add_tail(&wnode->node, &node->wlist_head);

    ret = hybm_gvm_p2p_set_wl(proc->sdid, proc->devid, arg->sdid, node, &wnode->rproc);
    if (ret == SET_WL_RET_DUPLICATIVE) {
        list_del_init(&wnode->node);
        kfree(wnode);
        hybm_gvm_debug("already set duplicated whitelist. input:key(0x%llx),sdid(%x)", arg->key, arg->sdid);
        return 0;
    }
    if (ret != 0) {
        list_del_init(&wnode->node);
        kfree(wnode);
        hybm_gvm_err("set white list fail. ret(%d)key(0x%llx)sdid(%x)", ret, arg->key, arg->sdid);
        return -EBUSY;
    }

    hybm_gvm_debug("hybm_gvm_set_whitelist success. input:key(0x%llx),sdid(%x)", arg->key, arg->sdid);
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

    node = gvm_tree_search(&proc->key_tree, key);
    if (node == NULL) {
        hybm_gvm_err("input key has not been set in whitelist! key(0x%llx)", key);
        return -EINVAL;
    }

    if (node->va != va) {
        hybm_gvm_err("input addr is not match the local addr. local:key(0x%llx)va(0x%llx)size(0x%llx) "
                     "input:key(0x%llx)va(0x%llx)",
                     node->shm_key, node->va, node->size, key, va);
        return -EBUSY;
    }

    ret = hybm_gvm_p2p_get_addr(proc, proc->sdid, proc->devid, gvm_get_sdid_by_key(key), node);
    if (ret != 0) {
        hybm_gvm_err("open mem failed! key(0x%llx),ret:%d", key, ret);
        return ret;
    }

    hybm_gvm_debug("hybm_gvm_mem_open success. va(0x%llx),key(0x%llx)", va, arg->key);
    return 0;
}

static int hybm_gvm_mem_close(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_mem_close_para *arg = &args->data.mem_close_para;
    struct gvm_node *node = NULL;
    u64 va = arg->addr;

    node = hybm_gvm_get_node(proc, va);
    if (node == NULL) {
        hybm_gvm_err("mem is not open! va(0x%llx)", va);
        return -EINVAL;
    }

    if (!gvm_check_flag(&node->flag, GVM_NODE_FLAG_REMOTE) || !gvm_check_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED)) {
        hybm_gvm_err("mem is not remote! va(0x%llx)flag(0x%llx)", va, node->flag);
        return -EBUSY;
    }

    hybm_gvm_mem_free_inner(proc, node, UINT64_MAX);
    gvm_clear_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED);
    hybm_gvm_debug("hybm_gvm_mem_close success. va(0x%llx)key(0x%llx)", va, node->shm_key);
    return 0;
}

static int hybm_gvm_mem_fetch(struct file *file, struct hybm_gvm_process *proc, struct hybm_gvm_ioctl_arg *args)
{
    struct hybm_gvm_mem_fetch_para *arg = &args->data.mem_fetch_para;
    struct gvm_dev_mem_node *node = NULL;
    u64 va = arg->addr;
    u64 size = arg->size;
    int ret;

    if (va % HYBM_GVM_PAGE_SIZE != 0 || size != HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input invalid! va(0x%llx),size(0x%llx)", va, size);
        return -EBUSY;
    }

    node = kzalloc(sizeof(struct gvm_dev_mem_node), GFP_KERNEL | __GFP_ACCOUNT);
    if (node == NULL) {
        hybm_gvm_err("kzalloc gvm node fail.");
        return -ENOMEM;
    }

    if (proc->sdid == arg->sdid) {
        ret = hybm_gvm_to_agent_fetch(proc->devid, proc->pasid, va, size, node->pa, HYBM_GVM_PAGE_NUM);
    } else {
        ret = hybm_gvm_p2p_fetch(proc->sdid, proc->devid, arg->sdid, va, node->pa);
        if (ret == 0) {
            ret = hybm_gvm_to_agent_mmap(proc->devid, proc->pasid, va, size, node->pa, HYBM_GVM_PAGE_NUM);
        }
    }
    if (ret != 0) {
        hybm_gvm_err("fetch mem failed! va(0x%llx),size(0x%llx)", va, size);
        kfree(node);
        return ret;
    }

    node->va = va;
    node->size = size;
    INIT_LIST_HEAD(&node->node);
    list_add_tail(&node->node, &proc->fetch_head);
    hybm_gvm_debug("hybm_gvm_mem_fetch success. va(0x%llx),size(0x%llx)", va, size);
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
    struct list_head *pos = NULL, *n = NULL;
    struct gvm_dev_mem_node *node = NULL;
    int ret;

    list_for_each_safe(pos, n, &proc->fetch_head)
    {
        node = list_entry(pos, struct gvm_dev_mem_node, node);
        ret = hybm_gvm_to_agent_unmap(proc->devid, proc->pasid, node->va, node->size, HYBM_HPAGE_SIZE);
        if (ret != 0) {
            hybm_gvm_err("unmap fetch mem failed! va(0x%llx),size(0x%llx),ret:%d", node->va, node->size, ret);
        }
        list_del_init(&node->node);
        kfree(node);
    }

    hybm_gvm_free_all_mem(proc);

    g_proc_list[proc->devid] = NULL;
    hybm_free_gvm_proc(&proc);
    priv->process = NULL;
}

static int hybm_gvm_proc_get_pa_inner(struct hybm_gvm_process *proc, u64 va, u64 size, u64 *arr, u64 num)
{
    struct gvm_node *node = NULL;
    u64 idx, i;

    idx = (va - proc->va_start) / HYBM_GVM_PAGE_SIZE;
    if (va < proc->va_start || (idx + num) > proc->va_page_num) {
        hybm_gvm_err("input addr is error. va(0x%llx)num(%llu)proc_num(%llu)", va, num, proc->va_page_num);
        return -EINVAL;
    }

    node = (struct gvm_node *)proc->mem_array[idx].head_node;
    if (node == NULL || node->va != va || node->size != size) {
        hybm_gvm_err("input addr is error. input:va(0x%llx),size(0x%llx) range:va(0x%llx),size(0x%llx)", va, size,
                     node->va, node->size);
        return -EINVAL;
    }
    if (!gvm_check_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED)) {
        hybm_gvm_err("pa is not set. va(0x%llx),size(0x%llx)", va, size);
        return -EINVAL;
    }

    for (i = 0; i < num; i++) {
        arr[i] = proc->mem_array[idx + i].pa;
        if (arr[i] == 0) {
            hybm_gvm_err("pa is zero. va(0x%llx),idx(%llu)", va, idx + i);
            return -EINVAL;
        }
        arr[i] = gvm_get_global_pa_from_local(arr[i], proc->sdid);
        if (arr[i] == 0) {
            hybm_gvm_err("get global pa failed. va(0x%llx),idx(%llu)", va, idx + i);
            return -EINVAL;
        }
    }
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
        hybm_gvm_err("input size is errer. size(0x%llx)num(%llu)", size, max_num);
        return -EINVAL;
    }

    hybm_gvm_ioctl_lock(proc, HYBM_GVM_WLOCK);
    ret = hybm_gvm_proc_get_pa_inner(proc, va, size, arr, size / HYBM_GVM_PAGE_SIZE);
    hybm_gvm_ioctl_unlock(proc, HYBM_GVM_WLOCK);
    hybm_gvm_debug("hybm_gvm_proc_get_pa va:0x%llx size:0x%llx pa_0:0x%llx", va, size, arr[0]);
    return ret;
}

int hybm_gvm_proc_set_pa(struct hybm_gvm_process *proc, u32 sdid, struct gvm_node *node, u64 *arr, u64 num)
{
    u64 st = (node->va - proc->va_start) / HYBM_GVM_PAGE_SIZE;
    u64 i, lpa;
    int ret;

    if (num != (node->size / HYBM_GVM_PAGE_SIZE)) {
        hybm_gvm_err("node size is not equal arr num. node_num(%llu),arr_num(%llu)", num,
                     node->size / HYBM_GVM_PAGE_SIZE);
        return -EINVAL;
    }
    if (st + num >= proc->va_page_num) {
        hybm_gvm_err("input va is out of va range. idx_ed(%llu),va_page_num(%llu)", st + num, proc->va_page_num);
        return -EINVAL;
    }

    for (i = 0; i < num; i++) {
        if (proc->mem_array[i + st].pa != 0 || proc->mem_array[i + st].head_node != NULL) {
            hybm_gvm_err("mem has already been opened or allocated. va(0x%llx)", node->va + i * HYBM_GVM_PAGE_SIZE);
            return -EINVAL;
        }
    }

    for (i = 0; i < num; i++) {
        proc->mem_array[i + st].pa = arr[i];
        proc->mem_array[i + st].head_node = node;
        lpa = arr[i];
        if (gvm_get_server_id_by_sdid(sdid) == gvm_get_server_id_by_sdid(proc->sdid)) {
            lpa = gvm_get_local_pa_from_global(lpa, sdid);
        }

        ret = hybm_gvm_to_agent_mmap(proc->devid, proc->pasid, proc->va_start + (i + st) * HYBM_GVM_PAGE_SIZE,
                                     HYBM_GVM_PAGE_SIZE, &lpa, 1U);
        if (ret != 0) {
            hybm_gvm_err("map device sdma failed. ret:%d", ret);
            hybm_gvm_mem_free_inner(proc, node, i + st);
            return ret;
        }
    }

    gvm_set_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED);
    return 0;
}

static int hybm_gvm_insert_remote_inner(struct hybm_gvm_process *proc, u64 key, u64 va, u64 size)
{
    struct gvm_node *node = NULL;
    node = gvm_tree_search(&proc->key_tree, key);
    if (node != NULL) {
        if (node->shm_key != key || node->va != va || node->size != size) {
            hybm_gvm_err("has different node. local:key(0x%llx)va(0x%llx)size(0x%llx) input:key(0x%llx)va(0x%llx)"
                         "size(0x%llx)\n",
                         node->shm_key, node->va, node->size, key, va, size);
            return -EBUSY;
        }
        return SET_WL_RET_DUPLICATIVE;
    }

    node = kzalloc(sizeof(struct gvm_node), GFP_KERNEL | __GFP_ACCOUNT);
    if (node == NULL) {
        hybm_gvm_err("kzalloc gvm node fail.");
        return -ENOMEM;
    }

    node->shm_key = key;
    node->va = va;
    node->size = size;
    INIT_LIST_HEAD(&node->wlist_head);
    gvm_set_flag(&node->flag, GVM_NODE_FLAG_REMOTE);

    if (gvm_tree_insert(&proc->key_tree, node) == false) {
        hybm_gvm_err("set key failed, unknown error. key(0x%llx)", node->shm_key);
        kfree(node);
        return -EBUSY;
    }

    return 0;
}

int hybm_gvm_insert_remote(u32 sdid, u64 key, u64 va, u64 size, struct hybm_gvm_process **rp)
{
    struct hybm_gvm_process *proc = hybm_gvm_get_proc_by_sdid(sdid);
    int ret;
    if (proc == NULL) {
        return -EINVAL;
    }

    hybm_gvm_ioctl_lock(proc, HYBM_GVM_WLOCK);
    ret = hybm_gvm_insert_remote_inner(proc, key, va, size);
    hybm_gvm_ioctl_unlock(proc, HYBM_GVM_WLOCK);
    *rp = proc;
    hybm_gvm_debug("hybm_gvm_insert_remote key:0x%llx va:0x%llx size:0x%llx", key, va, size);
    return ret;
}

static int hybm_gvm_remove_remote_inner(struct hybm_gvm_process *proc, u64 key)
{
    struct gvm_node *node = NULL;
    node = gvm_tree_search(&proc->key_tree, key);
    if (node == NULL) {
        hybm_gvm_warn("key has removed. key(0x%llx)", key);
        return -EINVAL;
    }

    if (gvm_check_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED)) {
        hybm_gvm_mem_free_inner(proc, node, UINT64_MAX);
        gvm_clear_flag(&node->flag, GVM_NODE_FLAG_ALLOCATED);
    }

    gvm_tree_remove(&proc->key_tree, node);
    kfree(node);
    return 0;
}

int hybm_gvm_unset_remote(u32 sdid, u64 key, struct hybm_gvm_process *rp)
{
    struct hybm_gvm_process *proc = hybm_gvm_get_proc_by_sdid(sdid);
    int ret;
    if (proc == NULL || proc != rp) {
        hybm_gvm_warn("proc has changed! key(0x%llx)", key);
        return 0;
    }

    hybm_gvm_ioctl_lock(proc, HYBM_GVM_WLOCK);
    ret = hybm_gvm_remove_remote_inner(proc, key);
    hybm_gvm_ioctl_unlock(proc, HYBM_GVM_WLOCK);
    if (ret != 0) {
        hybm_gvm_warn("key has changed! key(0x%llx)", key);
    }
    hybm_gvm_debug("hybm_gvm_unset_remote key:0x%llx sdid:%u", key, sdid);
    return 0;
}

int hybm_gvm_fetch_remote(u32 sdid, u64 va, u64 *pa_list)
{
    struct hybm_gvm_process *proc = hybm_gvm_get_proc_by_sdid(sdid);
    struct list_head *pos = NULL, *n = NULL;
    struct gvm_dev_mem_node *node = NULL;
    u32 i;
    if (proc == NULL) {
        hybm_gvm_err("not found proc, sdid(0x%x)", sdid);
        return -EINVAL;
    }

    list_for_each_safe(pos, n, &proc->fetch_head)
    {
        node = list_entry(pos, struct gvm_dev_mem_node, node);
        if (node->va == va) {
            for (i = 0; i < HYBM_GVM_PAGE_NUM; i++) {
                pa_list[i] = node->pa[i];
            }
            return 0;
        }
    }

    hybm_gvm_err("not found va, maybe not fetch, va(0x%llx)", va);
    return -EINVAL;
}
