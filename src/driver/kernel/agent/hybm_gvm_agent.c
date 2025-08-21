/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "hybm_gvm_agent_msg.h"
#include "hybm_gvm_log.h"
#include "hybm_gvm_proc_info.h"

#define DEVMM_P2P_PAGE_MAX_NUM_QUERY_MSG 32
#define DEVMM_CHAN_PAGE_FAULT_P2P_ID     7

#define HYBM_ROCE_MEM_NUM (32)

#define HYBM_SVSP_START 0x100000000000ULL
#define HYBM_SVSP_END   0x850000000000ULL

/* common msg */
enum agentdrv_common_msg_type {
    AGENTDRV_COMMON_MSG_PCIVNIC = 0,
    AGENTDRV_COMMON_MSG_SMMU,
    AGENTDRV_COMMON_MSG_DEVMM,
    AGENTDRV_COMMON_MSG_VMNG,
    AGENTDRV_COMMON_MSG_PROFILE = 4,
    AGENTDRV_COMMON_MSG_DEVDRV_MANAGER,
    AGENTDRV_COMMON_MSG_DEVDRV_TSDRV,
    AGENTDRV_COMMON_MSG_HDC,
    AGENTDRV_COMMON_MSG_SYSFS,
    AGENTDRV_COMMON_MSG_ESCHED,
    AGENTDRV_COMMON_MSG_DP_PROC_MNG,
    AGENTDRV_COMMON_MSG_TYPE_MAX
};

enum devdrv_process_type {
    DEVDRV_PROCESS_CP1 = 0,  /* aicpu_scheduler */
    DEVDRV_PROCESS_CP2,      /* custom_process */
    DEVDRV_PROCESS_DEV_ONLY, /* TDT */
    DEVDRV_PROCESS_QS,       /* queue_scheduler */
    DEVDRV_PROCESS_HCCP,     /* hccp server */
    DEVDRV_PROCESS_USER,     /* user proc, can bind many on host or device. */
    DEVDRV_PROCESS_TYPE_MAX
};

struct agentdrv_common_msg_client {
    enum agentdrv_common_msg_type type;
    int (*common_msg_recv)(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
    void (*init_notify)(u32 devid);
};

struct devmm_svm_process_id {
    int32_t hostpid;

    union {
        uint16_t devid;
        uint16_t vm_id;
    };

    uint16_t vfid;
};

struct gvm_agent_mem_node {
    u64 va;
    u64 pa;
    u64 size;
};

#define IB_PEER_MEMORY_NAME_MAX 64
#define IB_PEER_MEMORY_VER_MAX  16

struct peer_memory_client {
    char name[IB_PEER_MEMORY_NAME_MAX];
    char version[IB_PEER_MEMORY_VER_MAX];
    int (*acquire)(unsigned long addr, size_t size, void *peer_mem_private_data, char *peer_mem_name,
                   void **client_context);
    int (*get_pages)(unsigned long addr, size_t size, int write, int force, struct sg_table *sg_head,
                     void *client_context, u64 core_context);
    void (*put_pages)(struct sg_table *sg_head, void *client_context);
    int (*dma_map)(struct sg_table *sg_head, void *client_context, struct device *dma_device, int *nmap);
    int (*dma_unmap)(struct sg_table *sg_head, void *client_context, struct device *dma_device);
    unsigned long (*get_page_size)(void *client_context);
    void (*release)(void *client_context);
    void *(*get_context_private_data)(u64 peer_id);
    void (*put_context_private_data)(void *context);
};

typedef int (*invalidate_peer_memory)(void *reg_handle, u64 core_context);

#define GVM_PEER_MEM_NAME    "GVM_PEER_MEM"
#define GVM_PEER_MEM_VERSION "1.0"
#define GVM_PEER_INITED_FLAG 0xF1234567UL

struct gvm_agent_symbol_info {
    int (*agentdrv_msg_register_func)(struct agentdrv_common_msg_client *msg_client);
    int (*agentdrv_msg_unregister_func)(const struct agentdrv_common_msg_client *msg_client);

    int (*get_mem_pa_func)(struct devmm_svm_process_id *process_id, u64 addr, u64 size, u64 *pa_list, u32 pa_num);
    void (*put_mem_pa_func)(struct devmm_svm_process_id *process_id, u64 addr, u64 size, u64 *pa_list, u32 pa_num);

    int (*get_pasid_func)(int vpid, int dev_id);
    int (*get_svsp_pasid_func)(int vpid, int dev_id);
    u64 (*svsp_mmap_func)(u64 len, int pasid, u64 addr, u64 map_size, bool specify_address);
    int (*svsp_populate_func)(u64 addr, u64 pfn, u64 size, int pasid);
    void (*svsp_unmap_func)(u64 start, u64 len, int pasid);
    int (*devdrv_query_pid_func)(u32 host_pid, u32 chip_id, enum devdrv_process_type cp_type, u32 vfid, int *pid);

    void *(*ib_register_func)(const struct peer_memory_client *peer_client,
                              invalidate_peer_memory *invalidate_callback);
    void (*ib_unregister_func)(void *reg_handle);

    struct gvm_agent_mem_node *node_list;
    void *ib_priv;
};
struct gvm_agent_symbol_info g_gvm_agent_symbol = {0};

int gvm_agent_init_recv(struct hybm_gvm_agent_msg *msg, u32 devid)
{
    int devpid, pasid, svspid, ret;
    if (msg == NULL || msg->type != HYBM_GVM_AGENT_MSG_INIT) {
        hybm_gvm_err("input msg is invalid.\n");
        return -EINVAL;
    }

    ret = g_gvm_agent_symbol.devdrv_query_pid_func(msg->init.hostpid, devid, DEVDRV_PROCESS_CP1, 0, &devpid);
    if (ret != 0) {
        hybm_gvm_err("query devpid failed. ret:%d,devid:%u,hostpid:%d\n", ret, devid, msg->init.hostpid);
        return -EINVAL;
    }

    pasid = g_gvm_agent_symbol.get_pasid_func(devpid, devid);
    if (pasid < 0) {
        hybm_gvm_err("get pasid failed. ret:%d,hostpid:%d,devpid:%d\n", pasid, msg->init.hostpid, devpid);
        return -EINVAL;
    }

    svspid = g_gvm_agent_symbol.get_svsp_pasid_func(devpid, devid);
    if (svspid < 0) {
        hybm_gvm_err("get svsp pasid failed. ret:%d,hostpid:%d,devpid:%d\n", svspid, msg->init.hostpid, devpid);
        return -EINVAL;
    }

    msg->init.pasid = pasid;
    msg->init.svspid = svspid;
    hybm_gvm_debug("gvm_agent_init_recv, hostpid:%d,devpid:%d,pasid:%d,svspid:%d\n", msg->init.hostpid, devpid, pasid,
                   svspid);
    return 0;
}

int gvm_agent_map_recv(struct hybm_gvm_agent_msg *msg, u32 devid)
{
    u64 va, pa, size;
    u32 pasid, i;
    int ret;
    if (msg == NULL || msg->type != HYBM_GVM_AGENT_MSG_MAP) {
        hybm_gvm_err("input msg type is invalid.\n");
        return -EINVAL;
    }

    va = msg->mmap.va;
    pasid = msg->mmap.pasid;
    pa = msg->mmap.pa;
    size = msg->mmap.size;
    if (size != HYBM_GVM_PAGE_SIZE || pa % PAGE_SIZE) {
        hybm_gvm_err("input addr error.pa:0x%llx,size:0x%llx\n", pa, size);
        return -EINVAL;
    }

    if (va == 0ULL) {
        va = g_gvm_agent_symbol.svsp_mmap_func(size, pasid, va, size, false);
    } else {
        va = g_gvm_agent_symbol.svsp_mmap_func(size, pasid, va, size, true);
    }
    if (va < HYBM_SVSP_START || va > HYBM_SVSP_END || (msg->mmap.va != 0ULL && msg->mmap.va != va)) {
        hybm_gvm_err("mmap addr error.iva:0x%llx,va:0x%llx,pa:0x%llx,size:0x%llx\n", msg->mmap.va, va, pa, size);
        return -EINVAL;
    }

    ret = g_gvm_agent_symbol.svsp_populate_func(va, pa / PAGE_SIZE, size, pasid);
    if (ret != 0) {
        g_gvm_agent_symbol.svsp_unmap_func(va, size, pasid);
        hybm_gvm_err("mem populate failed, va:0x%llx,pa:0x%llx,size:0x%llx,ret:%d\n", va, pa, size, ret);
        return ret;
    }

    if (msg->mmap.va == 0ULL) {
        for (i = 0; i < HYBM_ROCE_MEM_NUM; i++) {
            if (g_gvm_agent_symbol.node_list[i].pa == 0) {
                g_gvm_agent_symbol.node_list[i].va = va;
                g_gvm_agent_symbol.node_list[i].pa = pa;
                g_gvm_agent_symbol.node_list[i].size = size;
                break;
            }
        }
        if (i == HYBM_ROCE_MEM_NUM) {
            hybm_gvm_err("mem node is full!");
            g_gvm_agent_symbol.svsp_unmap_func(va, size, pasid);
            return -EINVAL;
        }
    }

    msg->mmap.va = va;
    hybm_gvm_debug("gvm_agent_map_recv, va:0x%llx,pa:0x%llx,size:0x%llx\n", va, pa, size);
    return ret;
}

int gvm_agent_unmap_recv(struct hybm_gvm_agent_msg *msg, u32 devid)
{
    u64 va, size, page_size, num, i;
    u32 pasid;
    if (msg == NULL || msg->type != HYBM_GVM_AGENT_MSG_UNMAP) {
        hybm_gvm_err("input msg type is invalid.\n");
        return -EINVAL;
    }

    pasid = msg->unmap.pasid;
    va = msg->unmap.va;
    size = msg->unmap.size;
    page_size = msg->unmap.page_size;
    if (va % HYBM_GVM_PAGE_SIZE || size % HYBM_GVM_PAGE_SIZE || page_size % PAGE_SIZE || size % page_size) {
        hybm_gvm_err("input addr error. va:0x%llx,size:0x%llx,pg_size:0x%llx\n", va, size, page_size);
        return -EINVAL;
    }

    num = size / page_size;
    for (i = 0; i < num; i++) {
        g_gvm_agent_symbol.svsp_unmap_func(va + i * page_size, page_size, pasid);
    }

    for (i = 0; i < HYBM_ROCE_MEM_NUM; i++) {
        if (g_gvm_agent_symbol.node_list[i].va == va) {
            g_gvm_agent_symbol.node_list[i].va = 0;
            g_gvm_agent_symbol.node_list[i].pa = 0;
            g_gvm_agent_symbol.node_list[i].size = 0;
            break;
        }
    }

    hybm_gvm_debug("gvm_agent_unmap, va:0x%llx,size:0x%llx\n", va, size);
    return 0;
}

static int gvm_agent_map_svm_pa(int hostpid, u64 va, u64 size, u64 *pa_list, u64 num, u32 pasid)
{
    u64 i, iva, ret_va;
    struct devmm_svm_process_id id = {0};
    int ret = 0;
    id.hostpid = hostpid;

    ret = g_gvm_agent_symbol.get_mem_pa_func(&id, va, size, pa_list, num);
    if (ret != 0) {
        hybm_gvm_err("get svm pa failed. ret:%d\n", ret);
        return -EINVAL;
    }

    iva = va;
    for (i = 0; i < num; i++) {
        ret_va = g_gvm_agent_symbol.svsp_mmap_func(HYBM_HPAGE_SIZE, pasid, iva, HYBM_HPAGE_SIZE, true);
        if (ret_va != iva) {
            hybm_gvm_err("map mem failed, va:0x%llx,pa:0x%llx,ret_va:0x%llx\n", iva, pa_list[i], ret_va);
            break;
        }

        ret = g_gvm_agent_symbol.svsp_populate_func(iva, pa_list[i] / PAGE_SIZE, HYBM_HPAGE_SIZE, pasid);
        if (ret != 0) {
            hybm_gvm_err("populate mem failed, va:0x%llx,pa:0x%llx,ret:%d\n", iva, pa_list[i], ret);
            i++;
            break;
        }
        iva += HYBM_HPAGE_SIZE;
    }
    g_gvm_agent_symbol.put_mem_pa_func(&id, va, size, pa_list, num);

    if (ret != 0) {
        while (i > 0) {
            i--;
            g_gvm_agent_symbol.svsp_unmap_func(va + i * HYBM_HPAGE_SIZE, HYBM_HPAGE_SIZE, pasid);
        }
    }
    return ret;
}

int gvm_agent_fetch_recv(struct hybm_gvm_agent_msg *msg, u32 devid)
{
    u64 va, iva;
    u64 *pa_list = NULL;
    u32 pasid, num, i;
    int ret = 0;

    if (msg == NULL || msg->type != HYBM_GVM_AGENT_MSG_FETCH) {
        hybm_gvm_err("input msg type is invalid.\n");
        return -EINVAL;
    }
    if (msg->fetch.va % HYBM_GVM_PAGE_SIZE || msg->fetch.size % HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input addr error.va:0x%llx,size:0x%llx\n", msg->fetch.va, msg->fetch.size);
        return -EINVAL;
    }

    va = msg->fetch.va;
    pasid = msg->fetch.pasid;
    num = msg->fetch.size / HYBM_GVM_PAGE_SIZE;
    pa_list = kzalloc(sizeof(u64) * HYBM_GVM_PAGE_NUM, GFP_KERNEL | __GFP_ACCOUNT);
    if (pa_list == NULL) {
        hybm_gvm_err("Kzalloc pa_list is NULL.\n");
        return -EINVAL;
    }

    for (i = 0; i < num; i++) {
        iva = va + i * HYBM_GVM_PAGE_SIZE;
        ret = gvm_agent_map_svm_pa(msg->fetch.hostpid, iva, HYBM_GVM_PAGE_SIZE, pa_list, HYBM_GVM_PAGE_NUM, pasid);
        if (ret != 0) {
            hybm_gvm_err("map svm mem failed. va:0x%llx,ret:%d\n", iva, ret);
            break;
        }
    }

    if (ret != 0) {
        num = (iva - va) / HYBM_HPAGE_SIZE;
        for (i = 0; i < num; i++) {
            g_gvm_agent_symbol.svsp_unmap_func(va + i * HYBM_HPAGE_SIZE, HYBM_HPAGE_SIZE, pasid);
        }
    }
    kfree(pa_list);
    return ret;
}

typedef int (*hybm_gvm_agent_msg_rcv_func_t)(struct hybm_gvm_agent_msg *msg, u32 devid);
static const hybm_gvm_agent_msg_rcv_func_t rcv_ops[HYBM_GVM_AGENT_MSG_MAX] = {
    [HYBM_GVM_AGENT_MSG_INIT] = gvm_agent_init_recv,
    [HYBM_GVM_AGENT_MSG_MAP] = gvm_agent_map_recv,
    [HYBM_GVM_AGENT_MSG_UNMAP] = gvm_agent_unmap_recv,
    [HYBM_GVM_AGENT_MSG_FETCH] = gvm_agent_fetch_recv,
};

static int gvm_common_msg_process(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    struct hybm_gvm_agent_msg *msg = (struct hybm_gvm_agent_msg *)data;
    int ret;

    if (data == NULL) {
        hybm_gvm_err("Data is NULL. \n");
        return -EINVAL;
    }
    if (real_out_len == NULL) {
        hybm_gvm_err("Out_data_len is NULL. \n");
        return -EINVAL;
    }
    if (in_data_len != sizeof(struct hybm_gvm_agent_msg)) {
        hybm_gvm_err("In_data_len is invalid. (in_data_len=%u)\n", in_data_len);
        return -EMSGSIZE;
    }
    if (msg->valid != HYBM_GVM_S2A_MSG_SEND_MAGIC) {
        hybm_gvm_err("Invalid magic. (magic=0x%x;devid=%u;cmdtype=%d)\n", msg->valid, devid, msg->type);
        return -EINVAL;
    }
    if (msg->type < 0 || msg->type >= HYBM_GVM_AGENT_MSG_MAX || rcv_ops[msg->type] == NULL) {
        hybm_gvm_err("msg type is error. type(%d)\n", msg->type);
        ret = -EINVAL;
        goto save_msg_ret;
    }

    ret = rcv_ops[msg->type](msg, devid);
    *real_out_len = in_data_len;
save_msg_ret:
    msg->valid = HYBM_GVM_S2A_MSG_RCV_MAGIC;
    msg->result = ret;
    return 0;
}

static void gvm_common_msg_notify(u32 devid)
{
    hybm_gvm_info("device common message chan is initing device. (dev=%u)\n", devid);
    return;
}

static struct agentdrv_common_msg_client gvm_common_msg_client = {
    .type = AGENTDRV_COMMON_MSG_PROFILE, // TODO: update
    .common_msg_recv = gvm_common_msg_process,
    .init_notify = gvm_common_msg_notify,
};

struct gvm_peer_mem_context {
    struct mutex context_mutex;
    struct sg_table *sg_head;
    u32 inited_flag;
    u32 get_flag;
    u64 va;
    u64 len;
    u64 va_aligned_start;
    u64 aligned_size;
    u32 page_size;
    u32 pa_num;
    u64 pa_list[0];
};

int gvm_peer_mem_acquire(unsigned long addr, size_t size, void *peer_mem_data, char *peer_mem_name,
                         void **client_context)
{
    struct peer_memory_data *memory_data = (struct peer_memory_data *)peer_mem_data;
    u64 va_aligned_start, va_aligned_end, aligned_size, page_size;
    struct gvm_peer_mem_context *mm_context = NULL;
    u32 page_num, i;

    if ((memory_data == NULL) || (client_context == NULL)) {
        hybm_gvm_err("input err check private_data, client_context.\n");
        return false;
    }
    if (size != HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input size must 1G, va:0x%lx,size:0x%lx\n", addr, size);
        return false;
    }

    for (i = 0; i < HYBM_ROCE_MEM_NUM; i++) {
        if (g_gvm_agent_symbol.node_list[i].va == addr) {
            break;
        }
    }
    if (i == HYBM_ROCE_MEM_NUM) {
        hybm_gvm_err("not found this mem, va:0x%lx\n", addr);
        return false;
    }

    page_size = HYBM_HPAGE_SIZE;
    va_aligned_start = round_down(addr, page_size);
    va_aligned_end = round_up((addr + size), page_size);
    aligned_size = va_aligned_end - va_aligned_start;

    page_num = (aligned_size) / page_size;
    mm_context = (struct gvm_peer_mem_context *)vzalloc(sizeof(struct gvm_peer_mem_context) + sizeof(u64) * page_num);
    if (mm_context == NULL) {
        hybm_gvm_err("kalloc err. addr=0x%lx, num=%u\n", addr, page_num);
        return false;
    }
    mm_context->sg_head = NULL;
    mm_context->va = addr;
    mm_context->len = size;
    mm_context->va_aligned_start = va_aligned_start;
    mm_context->aligned_size = aligned_size;
    mm_context->page_size = page_size;
    mm_context->pa_num = page_num;
    mutex_init(&mm_context->context_mutex);
    mm_context->inited_flag = GVM_PEER_INITED_FLAG;
    *client_context = (void *)mm_context;
    hybm_gvm_debug("gvm_peer_mem_acquire, va:0x%lx,size:0x%lx,pg_size:0x%llx\n", addr, size, page_size);
    return true;
}

int gvm_peer_mem_get_pages(unsigned long addr, size_t size, int write, int force, struct sg_table *sg_head,
                           void *context, u64 core_context)
{
    struct gvm_peer_mem_context *mm_context = (struct gvm_peer_mem_context *)context;
    u64 pa = 0ULL;
    u32 i;

    if ((mm_context == NULL) || (mm_context->inited_flag != GVM_PEER_INITED_FLAG)) {
        hybm_gvm_err("mm_context(%pK) is null or hasnot inited(%u).\n", mm_context,
                     (mm_context != NULL ? mm_context->inited_flag : 0));
        return -EINVAL;
    }

    if (mm_context->page_size != HYBM_HPAGE_SIZE || mm_context->aligned_size != HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input param is invalid, pg_size:0x%x,size:0x%llx\n", mm_context->page_size,
                     mm_context->aligned_size);
        return -EINVAL;
    }

    for (i = 0; i < HYBM_ROCE_MEM_NUM; i++) {
        if (g_gvm_agent_symbol.node_list[i].va == addr) {
            pa = g_gvm_agent_symbol.node_list[i].pa;
            break;
        }
    }
    if (i == HYBM_ROCE_MEM_NUM) {
        hybm_gvm_err("not found this mem, va:0x%lx\n", addr);
        return -EINVAL;
    }

    mutex_lock(&mm_context->context_mutex);
    if (mm_context->get_flag == 1) {
        hybm_gvm_err("va=0x%lx, already geted pages", addr);
        mutex_unlock(&mm_context->context_mutex);
        return -EINVAL;
    }

    for (i = 0; i < mm_context->pa_num; i++) {
        mm_context->pa_list[i] = pa + i * HYBM_HPAGE_SIZE;
    }
    mm_context->get_flag = 1;
    mutex_unlock(&mm_context->context_mutex);
    hybm_gvm_debug("gvm_peer_mem_get_pages, va:0x%lx,size:0x%lx,pg_size:0x%x,pa:0x%llx\n", addr, size,
                   mm_context->page_size, pa);
    return 0;
}

int gvm_peer_mem_dma_map(struct sg_table *sg_head, void *context, struct device *dma_device, int *nmap)
{
    struct gvm_peer_mem_context *mm_context = (struct gvm_peer_mem_context *)context;
    struct scatterlist *sg = NULL;
    u32 i = 0;
    int ret;

    if ((mm_context == NULL) || (mm_context->inited_flag != GVM_PEER_INITED_FLAG)) {
        hybm_gvm_err("mm_context(%pK) is null or hasnot inited(%u).\n", mm_context,
                     (mm_context != NULL ? mm_context->inited_flag : 0));
        return -EINVAL;
    }
    mutex_lock(&mm_context->context_mutex);
    if (mm_context->sg_head != NULL) {
        hybm_gvm_err("sg_allocated, va=0x%llx.\n", mm_context->va);
        ret = -EINVAL;
        goto dma_map_exit;
    }
    ret = sg_alloc_table(sg_head, mm_context->pa_num, GFP_KERNEL);
    if (ret) {
        hybm_gvm_err("alloc err, num=%u.\n", mm_context->pa_num);
        goto dma_map_exit;
    }
    for_each_sg(sg_head->sgl, sg, mm_context->pa_num, i)
    {
        if (sg == NULL) {
            hybm_gvm_err("sg is null.\n");
            ret = -ENOMEM;
            goto dma_map_exit;
        }
        sg_set_page(sg, NULL, mm_context->page_size, 0);
        /* if netcard smmu opened, dma_address need use dma_device to get dma addr
         */
        sg->dma_address = mm_context->pa_list[i];
        sg->dma_length = mm_context->page_size;
    }
    *nmap = mm_context->pa_num;
    mm_context->sg_head = sg_head;

dma_map_exit:
    mutex_unlock(&mm_context->context_mutex);
    hybm_gvm_debug("gvm_peer_mem_dma_map, va:0x%llx,pa:0x%llx\n", mm_context->va, mm_context->pa_list[0]);
    return ret;
}

int gvm_peer_mem_dma_unmap(struct sg_table *sg_head, void *context, struct device *dma_device)
{
    struct gvm_peer_mem_context *mm_context = (struct gvm_peer_mem_context *)context;

    if ((mm_context == NULL) || (sg_head == NULL) || (mm_context->inited_flag != GVM_PEER_INITED_FLAG)) {
        hybm_gvm_err("mm_context(%d) is not inited(%u),sg_head null(%d).\n", (mm_context == NULL),
                     (mm_context != NULL ? mm_context->inited_flag : 0), (sg_head == NULL));
        return -EINVAL;
    }
    mutex_lock(&mm_context->context_mutex);
    if (sg_head != mm_context->sg_head) {
        mutex_unlock(&mm_context->context_mutex);
        hybm_gvm_err("sg_head not eq map.\n");
        return -EINVAL;
    }

    sg_free_table(sg_head);
    mm_context->sg_head = NULL;
    mutex_unlock(&mm_context->context_mutex);
    hybm_gvm_debug("gvm_peer_mem_dma_unmap, va:0x%llx,pa:0x%llx\n", mm_context->va, mm_context->pa_list[0]);
    return 0;
}

unsigned long gvm_peer_mem_get_page_size(void *mm_context)
{
    struct gvm_peer_mem_context *context = (struct gvm_peer_mem_context *)mm_context;
    unsigned long page_size;

    if ((context == NULL) || (context->inited_flag != GVM_PEER_INITED_FLAG)) {
        hybm_gvm_err("mm_context is NULL.\n");
        return 0;
    }
    page_size = context->page_size;
    hybm_gvm_debug("gvm_peer_mem_get_page_size, va:0x%llx\n", context->va);
    return page_size;
}

void gvm_peer_mem_put_pages(struct sg_table *sg_head, void *context)
{
    struct gvm_peer_mem_context *mm_context = (struct gvm_peer_mem_context *)context;
    hybm_gvm_debug("gvm_peer_mem_put_pages, va:0x%llx\n", mm_context->va);
    return;
}

void gvm_peer_mem_release(void *context)
{
    struct gvm_peer_mem_context *mm_context = (struct gvm_peer_mem_context *)context;
    hybm_gvm_debug("gvm_peer_mem_release, va:0x%llx\n", mm_context->va);
    return;
}

static struct peer_memory_client gvm_mem_client = {
    .name = GVM_PEER_MEM_NAME,
    .version = GVM_PEER_MEM_VERSION,
    .acquire = gvm_peer_mem_acquire,
    .get_pages = gvm_peer_mem_get_pages,
    .dma_map = gvm_peer_mem_dma_map,
    .dma_unmap = gvm_peer_mem_dma_unmap,
    .put_pages = gvm_peer_mem_put_pages,
    .get_page_size = gvm_peer_mem_get_page_size,
    .release = gvm_peer_mem_release,
};

static invalidate_peer_memory mem_invalidate_callback;

void *hybm_gvm_ib_register_peer(void)
{
    return g_gvm_agent_symbol.ib_register_func(&gvm_mem_client, &mem_invalidate_callback);
}

void hybm_gvm_ib_unregister_peer(void *priv)
{
    g_gvm_agent_symbol.ib_unregister_func(priv);
}

static int agent_symbol_get(void)
{
    g_gvm_agent_symbol.agentdrv_msg_register_func = __symbol_get("agentdrv_register_common_msg_client");
    if (g_gvm_agent_symbol.agentdrv_msg_register_func == NULL) {
        hybm_gvm_err("get symbol agentdrv_register_common_msg_client fail.\n");
        return -EBUSY;
    }

    g_gvm_agent_symbol.agentdrv_msg_unregister_func = __symbol_get("agentdrv_unregister_common_msg_client");
    if (g_gvm_agent_symbol.agentdrv_msg_unregister_func == NULL) {
        hybm_gvm_err("get symbol agentdrv_unregister_common_msg_client fail.\n");
        return -EBUSY;
    }

    g_gvm_agent_symbol.get_mem_pa_func = __symbol_get("devmm_get_mem_pa_list");
    if (g_gvm_agent_symbol.get_mem_pa_func == NULL) {
        hybm_gvm_err("get symbol devmm_get_mem_pa_list fail.\n");
        return -EBUSY;
    }

    g_gvm_agent_symbol.put_mem_pa_func = __symbol_get("devmm_put_mem_pa_list");
    if (g_gvm_agent_symbol.put_mem_pa_func == NULL) {
        hybm_gvm_err("get symbol devmm_put_mem_pa_list fail.\n");
        return -EBUSY;
    }

    g_gvm_agent_symbol.get_pasid_func = __symbol_get("svm_get_pasid");
    if (g_gvm_agent_symbol.get_pasid_func == NULL) {
        hybm_gvm_err("get symbol svm_get_pasid fail.\n");
        return -EBUSY;
    }

    g_gvm_agent_symbol.get_svsp_pasid_func = __symbol_get("svm_get_svsp_pasid");
    if (g_gvm_agent_symbol.get_svsp_pasid_func == NULL) {
        hybm_gvm_err("get symbol svm_get_svsp_pasid fail.\n");
        return -EBUSY;
    }

    g_gvm_agent_symbol.svsp_mmap_func = __symbol_get("svm_svsp_roce_mmap");
    if (g_gvm_agent_symbol.svsp_mmap_func == NULL) {
        hybm_gvm_err("get symbol svm_svsp_roce_mmap fail.\n");
        return -EBUSY;
    }

    g_gvm_agent_symbol.svsp_populate_func = __symbol_get("svm_svsp_roce_populate");
    if (g_gvm_agent_symbol.svsp_populate_func == NULL) {
        hybm_gvm_err("get symbol svm_svsp_roce_populate fail.\n");
        return -EBUSY;
    }

    g_gvm_agent_symbol.svsp_unmap_func = __symbol_get("svm_svsp_roce_munmap");
    if (g_gvm_agent_symbol.svsp_unmap_func == NULL) {
        hybm_gvm_err("get symbol svm_svsp_roce_munmap fail.\n");
        return -EBUSY;
    }

    g_gvm_agent_symbol.devdrv_query_pid_func = __symbol_get("devdrv_query_process_by_host_pid_kernel");
    if (g_gvm_agent_symbol.devdrv_query_pid_func == NULL) {
        hybm_gvm_err("get symbol devdrv_query_process_by_host_pid_kernel fail.\n");
        return -EBUSY;
    }

    g_gvm_agent_symbol.ib_register_func = __symbol_get("ib_register_peer_memory_client");
    if (g_gvm_agent_symbol.ib_register_func == NULL) {
        hybm_gvm_err("get symbol ib_register_peer_memory_client fail.\n");
        return -EBUSY;
    }

    g_gvm_agent_symbol.ib_unregister_func = __symbol_get("ib_unregister_peer_memory_client");
    if (g_gvm_agent_symbol.ib_unregister_func == NULL) {
        hybm_gvm_err("get symbol ib_unregister_peer_memory_client fail.\n");
        return -EBUSY;
    }

    return 0;
}

static void agent_symbol_put(void)
{
    if (g_gvm_agent_symbol.agentdrv_msg_register_func != NULL) {
        __symbol_put("agentdrv_register_common_msg_client");
        g_gvm_agent_symbol.agentdrv_msg_register_func = NULL;
    }
    if (g_gvm_agent_symbol.agentdrv_msg_unregister_func != NULL) {
        __symbol_put("agentdrv_unregister_common_msg_client");
        g_gvm_agent_symbol.agentdrv_msg_unregister_func = NULL;
    }
    if (g_gvm_agent_symbol.get_mem_pa_func != NULL) {
        __symbol_put("devmm_get_mem_pa_list");
        g_gvm_agent_symbol.get_mem_pa_func = NULL;
    }
    if (g_gvm_agent_symbol.put_mem_pa_func != NULL) {
        __symbol_put("devmm_put_mem_pa_list");
        g_gvm_agent_symbol.put_mem_pa_func = NULL;
    }
    if (g_gvm_agent_symbol.get_pasid_func != NULL) {
        __symbol_put("svm_get_pasid");
        g_gvm_agent_symbol.get_pasid_func = NULL;
    }
    if (g_gvm_agent_symbol.get_svsp_pasid_func != NULL) {
        __symbol_put("svm_get_svsp_pasid");
        g_gvm_agent_symbol.get_svsp_pasid_func = NULL;
    }
    if (g_gvm_agent_symbol.svsp_mmap_func != NULL) {
        __symbol_put("svm_svsp_roce_mmap");
        g_gvm_agent_symbol.svsp_mmap_func = NULL;
    }
    if (g_gvm_agent_symbol.svsp_populate_func != NULL) {
        __symbol_put("svm_svsp_roce_populate");
        g_gvm_agent_symbol.svsp_populate_func = NULL;
    }
    if (g_gvm_agent_symbol.svsp_unmap_func != NULL) {
        __symbol_put("svm_svsp_roce_munmap");
        g_gvm_agent_symbol.svsp_unmap_func = NULL;
    }
    if (g_gvm_agent_symbol.devdrv_query_pid_func != NULL) {
        __symbol_put("devdrv_query_process_by_host_pid_kernel");
        g_gvm_agent_symbol.devdrv_query_pid_func = NULL;
    }
    if (g_gvm_agent_symbol.ib_register_func != NULL) {
        __symbol_put("ib_register_peer_memory_client");
        g_gvm_agent_symbol.ib_register_func = NULL;
    }
    if (g_gvm_agent_symbol.ib_unregister_func != NULL) {
        __symbol_put("ib_unregister_peer_memory_client");
        g_gvm_agent_symbol.ib_unregister_func = NULL;
    }
}

static int __init gvm_agent_init(void)
{
    int ret;
    hybm_gvm_info("gvm agent init start.\n");
    g_gvm_agent_symbol.node_list =
        kzalloc(sizeof(struct gvm_agent_mem_node) * HYBM_ROCE_MEM_NUM, GFP_KERNEL | __GFP_ACCOUNT);
    if (g_gvm_agent_symbol.node_list == NULL) {
        hybm_gvm_err("kzalloc node_list fail.\n");
        return -ENOMEM;
    }

    ret = agent_symbol_get();
    if (ret != 0) {
        agent_symbol_put();
        goto init_failed1;
    }

    ret = g_gvm_agent_symbol.agentdrv_msg_register_func(&gvm_common_msg_client);
    if (ret != 0) {
        hybm_gvm_err("register_common failed. (ret=%d)\n", ret);
        goto init_failed1;
    }

    g_gvm_agent_symbol.ib_priv = hybm_gvm_ib_register_peer();
    if (g_gvm_agent_symbol.ib_priv == NULL) {
        hybm_gvm_err("register ib handle failed.\n");
        goto init_failed2;
    }

    hybm_gvm_info("gvm agent init success.\n");
    return 0;

init_failed2:
    g_gvm_agent_symbol.agentdrv_msg_unregister_func(&gvm_common_msg_client);
init_failed1:
    agent_symbol_put();
    return ret;
}

static void __exit gvm_agent_exit(void)
{
    hybm_gvm_info("gvm agent exit.\n");
    if (g_gvm_agent_symbol.node_list != NULL) {
        kfree(g_gvm_agent_symbol.node_list);
        g_gvm_agent_symbol.node_list = NULL;
    }
    hybm_gvm_ib_unregister_peer(g_gvm_agent_symbol.ib_priv);
    g_gvm_agent_symbol.agentdrv_msg_unregister_func(&gvm_common_msg_client);
    agent_symbol_put();
}

module_init(gvm_agent_init);
module_exit(gvm_agent_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("global virtual memory manager for Userland applications");
