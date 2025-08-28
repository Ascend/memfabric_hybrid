/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/pid.h>

#include "hybm_gvm_agent_msg.h"
#include "hybm_gvm_log.h"
#include "hybm_gvm_proc_info.h"

#define DEVMM_P2P_PAGE_MAX_NUM_QUERY_MSG 32
#define DEVMM_CHAN_PAGE_FAULT_P2P_ID     7

#define HYBM_ROCE_MEM_NUM (32)

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

struct gvm_agent_info {
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

    struct list_head roce_head;
    void *ib_priv;
    int devpid;
};
struct gvm_agent_info g_gvm_agent_info = {0};

bool gvm_agent_check_task(void)
{
    struct task_struct *task;
    if (g_gvm_agent_info.devpid <= 0) {
        return false;
    }

    task = find_get_task_by_vpid(g_gvm_agent_info.devpid);
    if (task) {
        if (task->exit_state != 0) {
            g_gvm_agent_info.devpid = -1;
        }
        put_task_struct(task);
    } else {
        g_gvm_agent_info.devpid = -1;
    }

    return (g_gvm_agent_info.devpid > 0);
}

int gvm_agent_init_recv(struct hybm_gvm_agent_msg *msg, u32 devid)
{
    int devpid, pasid, svspid, ret;
    struct hybm_gvm_agent_init_msg *init_body;
    if (msg == NULL || msg->type != HYBM_GVM_AGENT_MSG_INIT) {
        hybm_gvm_err("input msg is invalid.");
        return -EINVAL;
    }

    init_body = (struct hybm_gvm_agent_init_msg *)msg->body;
    ret = g_gvm_agent_info.devdrv_query_pid_func(init_body->hostpid, devid, DEVDRV_PROCESS_CP1, 0, &devpid);
    if (ret != 0) {
        hybm_gvm_err("query devpid failed. ret:%d,devid:%u,hostpid:%d", ret, devid, init_body->hostpid);
        return -EINVAL;
    }

    pasid = g_gvm_agent_info.get_pasid_func(devpid, devid);
    if (pasid < 0) {
        hybm_gvm_err("get pasid failed. ret:%d,hostpid:%d,devpid:%d", pasid, init_body->hostpid, devpid);
        return -EINVAL;
    }

    svspid = g_gvm_agent_info.get_svsp_pasid_func(devpid, devid);
    if (svspid < 0) {
        hybm_gvm_err("get svsp pasid failed. ret:%d,hostpid:%d,devpid:%d", svspid, init_body->hostpid, devpid);
        return -EINVAL;
    }

    init_body->pasid = pasid;
    init_body->svspid = svspid;
    g_gvm_agent_info.devpid = devpid;
    hybm_gvm_debug("gvm_agent_init_recv, hostpid:%d,devpid:%d,pasid:%d,svspid:%d", init_body->hostpid, devpid, pasid,
                   svspid);
    return 0;
}

static int gvm_agent_map_svsp(u64 va, u64 size, u64 *pa_list, u32 num, u32 pasid)
{
    u64 i, iva, ret_va, page_size;
    int ret = 0;

    iva = va;
    page_size = size / num;
    for (i = 0; i < num; i++) {
        ret_va = g_gvm_agent_info.svsp_mmap_func(page_size, pasid, iva, page_size, true);
        if (ret_va != iva) {
            hybm_gvm_err("map mem failed, va:0x%llx,pa:0x%llx,ret_va:0x%llx", iva, pa_list[i], ret_va);
            break;
        }

        ret = g_gvm_agent_info.svsp_populate_func(iva, PFN_DOWN(pa_list[i]), page_size, pasid);
        if (ret != 0) {
            hybm_gvm_err("populate mem failed, va:0x%llx,pa:0x%llx,ret:%d", iva, pa_list[i], ret);
            i++;
            break;
        }
        iva += page_size;
    }

    if (ret != 0) {
        while (i > 0) {
            i--;
            g_gvm_agent_info.svsp_unmap_func(va + i * page_size, page_size, pasid);
        }
    }
    return ret;
}

static int gvm_agent_add_roce_node(u64 va, u64 size, u64 *pa_list, u32 num)
{
    u32 i;
    struct gvm_dev_mem_node *node = NULL;

    if (size % num || (size / num != HYBM_HPAGE_SIZE) || num != HYBM_GVM_PAGE_NUM) {
        hybm_gvm_err("input is error, size:0x%llx,num:0x%u", size, num);
        return -EINVAL;
    }

    node = kzalloc(sizeof(struct gvm_dev_mem_node), GFP_KERNEL | __GFP_ACCOUNT);
    if (node == NULL) {
        hybm_gvm_err("kzalloc gvm agent node fail.");
        return -ENOMEM;
    }

    node->va = va;
    node->size = size;
    node->num = num;
    node->page_size = size / num;
    for (i = 0; i < num; i++) {
        node->pa[i] = pa_list[i];
    }

    INIT_LIST_HEAD(&node->node);
    list_add_tail(&node->node, &g_gvm_agent_info.roce_head);
    return 0;
}

int gvm_agent_map_recv(struct hybm_gvm_agent_msg *msg, u32 devid)
{
    u64 va, size;
    u32 pasid, num;
    struct hybm_gvm_agent_mmap_msg *map_body;
    int ret;
    if (msg == NULL || msg->type != HYBM_GVM_AGENT_MSG_MAP) {
        hybm_gvm_err("input msg type is invalid.");
        return -EINVAL;
    }

    map_body = (struct hybm_gvm_agent_mmap_msg *)msg->body;
    va = map_body->va;
    pasid = map_body->pasid;
    num = map_body->page_num;
    size = map_body->size;
    if (size != HYBM_GVM_PAGE_SIZE || size % num) {
        hybm_gvm_err("input addr error.size:0x%llx,num:%u", size, num);
        return -EINVAL;
    }

    if ((va >= HYBM_SVM_START && va < HYBM_SVM_END) || (va >= HYBM_SVSP_START && va < HYBM_SVSP_END)) {
        ret = gvm_agent_map_svsp(va, size, map_body->pa_list, num, pasid);
        hybm_gvm_debug("gvm_agent_map_svsp, va:0x%llx,size:0x%llx,ret:%d", va, size, ret);
    } else {
        ret = gvm_agent_add_roce_node(va, size, map_body->pa_list, num);
        hybm_gvm_debug("gvm_agent_map_roce, va:0x%llx,size:0x%llx,ret:%d", va, size, ret);
    }

    return ret;
}

int gvm_agent_unmap_recv(struct hybm_gvm_agent_msg *msg, u32 devid)
{
    u64 va, size, page_size, num, i;
    struct hybm_gvm_agent_unmap_msg *unmap_body;
    struct list_head *pos = NULL, *n = NULL;
    struct gvm_dev_mem_node *node;
    u32 pasid;

    if (msg == NULL || msg->type != HYBM_GVM_AGENT_MSG_UNMAP) {
        hybm_gvm_err("input msg type is invalid.");
        return -EINVAL;
    }

    unmap_body = (struct hybm_gvm_agent_unmap_msg *)msg->body;
    pasid = unmap_body->pasid;
    va = unmap_body->va;
    size = unmap_body->size;
    page_size = unmap_body->page_size;
    if (va % HYBM_GVM_PAGE_SIZE || size % HYBM_GVM_PAGE_SIZE || page_size % PAGE_SIZE || size % page_size) {
        hybm_gvm_err("input addr error. va:0x%llx,size:0x%llx,pg_size:0x%llx", va, size, page_size);
        return -EINVAL;
    }

    if ((va >= HYBM_SVM_START && va < HYBM_SVM_END) || (va >= HYBM_SVSP_START && va < HYBM_SVSP_END)) {
        if (!gvm_agent_check_task()) {
            hybm_gvm_debug("gvm_agent_unmap, process exited, skip unmap");
        } else {
            num = size / page_size;
            for (i = 0; i < num; i++) {
                g_gvm_agent_info.svsp_unmap_func(va + i * page_size, page_size, pasid);
            }
        }
    } else {
        list_for_each_safe(pos, n, &g_gvm_agent_info.roce_head)
        {
            node = list_entry(pos, struct gvm_dev_mem_node, node);
            if (node->va == va) {
                list_del_init(&node->node);
                kfree(node);
                break;
            }
        }
    }

    hybm_gvm_debug("gvm_agent_unmap, va:0x%llx,size:0x%llx,pg_size:0x%llx", va, size, page_size);
    return 0;
}

static int gvm_agent_map_svm_pa(int hostpid, u32 devid, u64 va, u64 size, u64 *pa_list, u64 *out, u32 num, u32 pasid)
{
    struct devmm_svm_process_id id = {0};
    int ret = 0;
    u32 i;
    id.hostpid = hostpid;
    id.devid = devid;

    // TODO: 先get page_size check一下
    ret = g_gvm_agent_info.get_mem_pa_func(&id, va, size, pa_list, num);
    if (ret != 0) {
        hybm_gvm_err("get svm pa failed. ret:%d", ret);
        return -EINVAL;
    }

    for (i = 0; i < num; i++) {
        out[i] = pa_list[i];
    }

    ret = gvm_agent_map_svsp(va, size, pa_list, num, pasid);
    g_gvm_agent_info.put_mem_pa_func(&id, va, size, pa_list, num);
    hybm_gvm_debug("gvm_agent_fetch_map, va:0x%llx,size:0x%llx,num:%x,ret:%d", va, size, num, ret);
    return ret;
}

int gvm_agent_fetch_recv(struct hybm_gvm_agent_msg *msg, u32 devid)
{
    u64 va;
    u64 *pa_list = NULL;
    u32 pasid, num;
    struct hybm_gvm_agent_fetch_msg *fetch_body;
    int ret = 0;

    if (msg == NULL || msg->type != HYBM_GVM_AGENT_MSG_FETCH) {
        hybm_gvm_err("input msg type is invalid.");
        return -EINVAL;
    }
    
    fetch_body = (struct hybm_gvm_agent_fetch_msg *)msg->body;
    if (fetch_body->va % HYBM_GVM_PAGE_SIZE || fetch_body->size != HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input addr error.va:0x%llx,size:0x%llx", fetch_body->va, fetch_body->size);
        return -EINVAL;
    }

    va = fetch_body->va;
    pasid = fetch_body->pasid;
    num = HYBM_GVM_PAGE_NUM; // = fetch_body->size / HYBM_GVM_PAGE_SIZE;
    pa_list = kzalloc(sizeof(u64) * HYBM_GVM_PAGE_NUM, GFP_KERNEL | __GFP_ACCOUNT);
    if (pa_list == NULL) {
        hybm_gvm_err("Kzalloc pa_list is NULL.");
        return -EINVAL;
    }

    ret = gvm_agent_map_svm_pa(fetch_body->hostpid, devid, va, HYBM_GVM_PAGE_SIZE, pa_list, fetch_body->pa_list,
                               HYBM_GVM_PAGE_NUM, pasid);
    if (ret != 0) {
        hybm_gvm_err("map svm mem failed. va:0x%llx,ret:%d", va, ret);
    }

    kfree(pa_list);
    hybm_gvm_debug("gvm_agent_fetch, va:0x%llx,size:0x%llx", va, fetch_body->size);
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
        hybm_gvm_err("Data is NULL. ");
        return -EINVAL;
    }
    if (real_out_len == NULL) {
        hybm_gvm_err("Out_data_len is NULL. ");
        return -EINVAL;
    }
    if (in_data_len < sizeof(struct hybm_gvm_agent_msg)) {
        hybm_gvm_err("In_data_len is invalid. (in_data_len=%u)", in_data_len);
        return -EMSGSIZE;
    }
    if (msg->valid != HYBM_GVM_S2A_MSG_SEND_MAGIC) {
        hybm_gvm_err("Invalid magic. (magic=0x%x;devid=%u;cmdtype=%d)", msg->valid, devid, msg->type);
        return -EINVAL;
    }
    if (msg->type < 0 || msg->type >= HYBM_GVM_AGENT_MSG_MAX || rcv_ops[msg->type] == NULL) {
        hybm_gvm_err("msg type is error. type(%d)", msg->type);
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
    hybm_gvm_info("device common message chan is initing device. (dev=%u)", devid);
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
    struct list_head *pos = NULL, *n = NULL;
    struct gvm_dev_mem_node *node = NULL;
    u32 page_num;

    if ((memory_data == NULL) || (client_context == NULL)) {
        hybm_gvm_err("input err check private_data, client_context.");
        return false;
    }
    if (size != HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input size must equals to 1G, va:0x%lx,size:0x%lx", addr, size);
        return false;
    }

    list_for_each_safe(pos, n, &g_gvm_agent_info.roce_head)
    {
        node = list_entry(pos, struct gvm_dev_mem_node, node);
        if (node->va == addr) {
            break;
        }
    }
    if (node == NULL || node->va != addr) {
        hybm_gvm_err("input addr has not been mapped, va:0x%lx", addr);
        return false;
    }

    page_size = node->page_size;
    va_aligned_start = round_down(addr, page_size);
    va_aligned_end = round_up((addr + size), page_size);
    aligned_size = va_aligned_end - va_aligned_start;

    page_num = (aligned_size) / page_size;
    mm_context = (struct gvm_peer_mem_context *)vzalloc(sizeof(struct gvm_peer_mem_context) + sizeof(u64) * page_num);
    if (mm_context == NULL) {
        hybm_gvm_err("kalloc mm_context fail. addr=0x%lx, num=%u", addr, page_num);
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
    hybm_gvm_debug("gvm_peer_mem_acquire, va:0x%lx,size:0x%lx,pg_size:0x%llx", addr, size, page_size);
    return true;
}

int gvm_peer_mem_get_pages(unsigned long addr, size_t size, int write, int force, struct sg_table *sg_head,
                           void *context, u64 core_context)
{
    struct gvm_peer_mem_context *mm_context = (struct gvm_peer_mem_context *)context;
    struct list_head *pos = NULL, *n = NULL;
    struct gvm_dev_mem_node *node = NULL;
    u64 va;
    u32 st, i;

    if ((mm_context == NULL) || (mm_context->inited_flag != GVM_PEER_INITED_FLAG)) {
        hybm_gvm_err("mm_context(%pK) is null or has not been initialized(%u).", mm_context,
                     (mm_context != NULL ? mm_context->inited_flag : 0));
        return -EINVAL;
    }

    if (mm_context->page_size != HYBM_HPAGE_SIZE || mm_context->aligned_size % HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input param is invalid, pg_size:0x%x,size:0x%llx", mm_context->page_size,
                     mm_context->aligned_size);
        return -EINVAL;
    }

    mutex_lock(&mm_context->context_mutex);
    if (mm_context->get_flag == 1) {
        hybm_gvm_err("va=0x%lx, already got pages", addr);
        mutex_unlock(&mm_context->context_mutex);
        return -EINVAL;
    }

    for (va = addr; va < (addr + size); va += HYBM_GVM_PAGE_SIZE) {
        list_for_each_safe(pos, n, &g_gvm_agent_info.roce_head)
        {
            node = list_entry(pos, struct gvm_dev_mem_node, node);
            if (node->va == va) {
                break;
            }
        }

        if (node == NULL || node->va != va || node->page_size != HYBM_HPAGE_SIZE) {
            hybm_gvm_err("va=0x%llx, not found agent mem node!", va);
            mutex_unlock(&mm_context->context_mutex);
            return -EINVAL;
        }

        st = (va - addr) / mm_context->page_size;
        for (i = 0;i < HYBM_GVM_PAGE_NUM; i++) {
            mm_context->pa_list[st + i] = node->pa[i];
        }
    }

    mm_context->get_flag = 1;
    mutex_unlock(&mm_context->context_mutex);
    hybm_gvm_debug("gvm_peer_mem_get_pages, va:0x%lx,size:0x%lx,pg_size:0x%x", addr, size, mm_context->page_size);
    return 0;
}

int gvm_peer_mem_dma_map(struct sg_table *sg_head, void *context, struct device *dma_device, int *nmap)
{
    struct gvm_peer_mem_context *mm_context = (struct gvm_peer_mem_context *)context;
    struct scatterlist *sg = NULL;
    u32 i = 0;
    int ret;

    if ((mm_context == NULL) || (mm_context->inited_flag != GVM_PEER_INITED_FLAG)) {
        hybm_gvm_err("mm_context(%pK) is null or has not been initialized(%u).", mm_context,
                     (mm_context != NULL ? mm_context->inited_flag : 0));
        return -EINVAL;
    }
    mutex_lock(&mm_context->context_mutex);
    if (mm_context->sg_head != NULL) {
        hybm_gvm_err("sg_head has already been allocated, va=0x%llx.", mm_context->va);
        ret = -EINVAL;
        goto dma_map_exit;
    }
    ret = sg_alloc_table(sg_head, mm_context->pa_num, GFP_KERNEL);
    if (ret) {
        hybm_gvm_err("alloc sg_head fail, num=%u.", mm_context->pa_num);
        goto dma_map_exit;
    }
    for_each_sg(sg_head->sgl, sg, mm_context->pa_num, i)
    {
        if (sg == NULL) {
            hybm_gvm_err("sg is null.");
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
    hybm_gvm_debug("gvm_peer_mem_dma_map, va:0x%llx,pa:0x%llx", mm_context->va, mm_context->pa_list[0]);
    return ret;
}

int gvm_peer_mem_dma_unmap(struct sg_table *sg_head, void *context, struct device *dma_device)
{
    struct gvm_peer_mem_context *mm_context = (struct gvm_peer_mem_context *)context;

    if ((mm_context == NULL) || (sg_head == NULL) || (mm_context->inited_flag != GVM_PEER_INITED_FLAG)) {
        hybm_gvm_err("mm_context(%d) has not been initialized(%u),sg_head null(%d).", (mm_context == NULL),
                     (mm_context != NULL ? mm_context->inited_flag : 0), (sg_head == NULL));
        return -EINVAL;
    }
    mutex_lock(&mm_context->context_mutex);
    if (sg_head != mm_context->sg_head) {
        mutex_unlock(&mm_context->context_mutex);
        hybm_gvm_err("input sg_head is incorrect.");
        return -EINVAL;
    }

    sg_free_table(sg_head);
    mm_context->sg_head = NULL;
    mutex_unlock(&mm_context->context_mutex);
    hybm_gvm_debug("gvm_peer_mem_dma_unmap, va:0x%llx,pa:0x%llx", mm_context->va, mm_context->pa_list[0]);
    return 0;
}

unsigned long gvm_peer_mem_get_page_size(void *mm_context)
{
    struct gvm_peer_mem_context *context = (struct gvm_peer_mem_context *)mm_context;
    unsigned long page_size;

    if ((context == NULL) || (context->inited_flag != GVM_PEER_INITED_FLAG)) {
        hybm_gvm_err("mm_context is NULL.");
        return 0;
    }
    page_size = context->page_size;
    hybm_gvm_debug("gvm_peer_mem_get_page_size, va:0x%llx", context->va);
    return page_size;
}

void gvm_peer_mem_put_pages(struct sg_table *sg_head, void *context)
{
    struct gvm_peer_mem_context *mm_context = (struct gvm_peer_mem_context *)context;
    // TODO: not implemented
    hybm_gvm_debug("gvm_peer_mem_put_pages, va:0x%llx", mm_context->va);
    return;
}

void gvm_peer_mem_release(void *context)
{
    struct gvm_peer_mem_context *mm_context = (struct gvm_peer_mem_context *)context;
    // TODO: not implemented
    hybm_gvm_debug("gvm_peer_mem_release, va:0x%llx", mm_context->va);
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
    return g_gvm_agent_info.ib_register_func(&gvm_mem_client, &mem_invalidate_callback);
}

void hybm_gvm_ib_unregister_peer(void *priv)
{
    g_gvm_agent_info.ib_unregister_func(priv);
}

static int agent_symbol_get(void)
{
    g_gvm_agent_info.agentdrv_msg_register_func = __symbol_get("agentdrv_register_common_msg_client");
    if (g_gvm_agent_info.agentdrv_msg_register_func == NULL) {
        hybm_gvm_err("get symbol agentdrv_register_common_msg_client fail.");
        return -EBUSY;
    }

    g_gvm_agent_info.agentdrv_msg_unregister_func = __symbol_get("agentdrv_unregister_common_msg_client");
    if (g_gvm_agent_info.agentdrv_msg_unregister_func == NULL) {
        hybm_gvm_err("get symbol agentdrv_unregister_common_msg_client fail.");
        return -EBUSY;
    }

    g_gvm_agent_info.get_mem_pa_func = __symbol_get("devmm_get_mem_pa_list");
    if (g_gvm_agent_info.get_mem_pa_func == NULL) {
        hybm_gvm_err("get symbol devmm_get_mem_pa_list fail.");
        return -EBUSY;
    }

    g_gvm_agent_info.put_mem_pa_func = __symbol_get("devmm_put_mem_pa_list");
    if (g_gvm_agent_info.put_mem_pa_func == NULL) {
        hybm_gvm_err("get symbol devmm_put_mem_pa_list fail.");
        return -EBUSY;
    }

    g_gvm_agent_info.get_pasid_func = __symbol_get("svm_get_pasid");
    if (g_gvm_agent_info.get_pasid_func == NULL) {
        hybm_gvm_err("get symbol svm_get_pasid fail.");
        return -EBUSY;
    }

    g_gvm_agent_info.get_svsp_pasid_func = __symbol_get("svm_get_svsp_pasid");
    if (g_gvm_agent_info.get_svsp_pasid_func == NULL) {
        hybm_gvm_err("get symbol svm_get_svsp_pasid fail.");
        return -EBUSY;
    }

    g_gvm_agent_info.svsp_mmap_func = __symbol_get("svm_svsp_roce_mmap");
    if (g_gvm_agent_info.svsp_mmap_func == NULL) {
        hybm_gvm_err("get symbol svm_svsp_roce_mmap fail.");
        return -EBUSY;
    }

    g_gvm_agent_info.svsp_populate_func = __symbol_get("svm_svsp_roce_populate");
    if (g_gvm_agent_info.svsp_populate_func == NULL) {
        hybm_gvm_err("get symbol svm_svsp_roce_populate fail.");
        return -EBUSY;
    }

    g_gvm_agent_info.svsp_unmap_func = __symbol_get("svm_svsp_roce_munmap");
    if (g_gvm_agent_info.svsp_unmap_func == NULL) {
        hybm_gvm_err("get symbol svm_svsp_roce_munmap fail.");
        return -EBUSY;
    }

    g_gvm_agent_info.devdrv_query_pid_func = __symbol_get("devdrv_query_process_by_host_pid_kernel");
    if (g_gvm_agent_info.devdrv_query_pid_func == NULL) {
        hybm_gvm_err("get symbol devdrv_query_process_by_host_pid_kernel fail.");
        return -EBUSY;
    }

    g_gvm_agent_info.ib_register_func = __symbol_get("ib_register_peer_memory_client");
    if (g_gvm_agent_info.ib_register_func == NULL) {
        hybm_gvm_err("get symbol ib_register_peer_memory_client fail.");
        return -EBUSY;
    }

    g_gvm_agent_info.ib_unregister_func = __symbol_get("ib_unregister_peer_memory_client");
    if (g_gvm_agent_info.ib_unregister_func == NULL) {
        hybm_gvm_err("get symbol ib_unregister_peer_memory_client fail.");
        return -EBUSY;
    }

    return 0;
}

static void agent_symbol_put(void)
{
    if (g_gvm_agent_info.agentdrv_msg_register_func != NULL) {
        __symbol_put("agentdrv_register_common_msg_client");
        g_gvm_agent_info.agentdrv_msg_register_func = NULL;
    }
    if (g_gvm_agent_info.agentdrv_msg_unregister_func != NULL) {
        __symbol_put("agentdrv_unregister_common_msg_client");
        g_gvm_agent_info.agentdrv_msg_unregister_func = NULL;
    }
    if (g_gvm_agent_info.get_mem_pa_func != NULL) {
        __symbol_put("devmm_get_mem_pa_list");
        g_gvm_agent_info.get_mem_pa_func = NULL;
    }
    if (g_gvm_agent_info.put_mem_pa_func != NULL) {
        __symbol_put("devmm_put_mem_pa_list");
        g_gvm_agent_info.put_mem_pa_func = NULL;
    }
    if (g_gvm_agent_info.get_pasid_func != NULL) {
        __symbol_put("svm_get_pasid");
        g_gvm_agent_info.get_pasid_func = NULL;
    }
    if (g_gvm_agent_info.get_svsp_pasid_func != NULL) {
        __symbol_put("svm_get_svsp_pasid");
        g_gvm_agent_info.get_svsp_pasid_func = NULL;
    }
    if (g_gvm_agent_info.svsp_mmap_func != NULL) {
        __symbol_put("svm_svsp_roce_mmap");
        g_gvm_agent_info.svsp_mmap_func = NULL;
    }
    if (g_gvm_agent_info.svsp_populate_func != NULL) {
        __symbol_put("svm_svsp_roce_populate");
        g_gvm_agent_info.svsp_populate_func = NULL;
    }
    if (g_gvm_agent_info.svsp_unmap_func != NULL) {
        __symbol_put("svm_svsp_roce_munmap");
        g_gvm_agent_info.svsp_unmap_func = NULL;
    }
    if (g_gvm_agent_info.devdrv_query_pid_func != NULL) {
        __symbol_put("devdrv_query_process_by_host_pid_kernel");
        g_gvm_agent_info.devdrv_query_pid_func = NULL;
    }
    if (g_gvm_agent_info.ib_register_func != NULL) {
        __symbol_put("ib_register_peer_memory_client");
        g_gvm_agent_info.ib_register_func = NULL;
    }
    if (g_gvm_agent_info.ib_unregister_func != NULL) {
        __symbol_put("ib_unregister_peer_memory_client");
        g_gvm_agent_info.ib_unregister_func = NULL;
    }
}

static int __init gvm_agent_init(void)
{
    int ret;
    hybm_gvm_info("gvm agent init start.");
    INIT_LIST_HEAD(&g_gvm_agent_info.roce_head);
    g_gvm_agent_info.devpid = -1;

    ret = agent_symbol_get();
    if (ret != 0) {
        agent_symbol_put();
        goto init_failed1;
    }

    ret = g_gvm_agent_info.agentdrv_msg_register_func(&gvm_common_msg_client);
    if (ret != 0) {
        hybm_gvm_err("register_common failed. (ret=%d)", ret);
        goto init_failed1;
    }

    g_gvm_agent_info.ib_priv = hybm_gvm_ib_register_peer();
    if (g_gvm_agent_info.ib_priv == NULL) {
        hybm_gvm_err("register ib handle failed.");
        goto init_failed2;
    }

    hybm_gvm_info("gvm agent init success.");
    return 0;

init_failed2:
    g_gvm_agent_info.agentdrv_msg_unregister_func(&gvm_common_msg_client);
init_failed1:
    agent_symbol_put();
    return ret;
}

static void __exit gvm_agent_exit(void)
{
    struct list_head *pos = NULL, *n = NULL;
    struct gvm_dev_mem_node *node;

    hybm_gvm_info("gvm agent exit.");
    if (!list_empty(&g_gvm_agent_info.roce_head)) {
        hybm_gvm_err("gvm roce list is not empty!");

        list_for_each_safe(pos, n, &g_gvm_agent_info.roce_head)
        {
            node = list_entry(pos, struct gvm_dev_mem_node, node);
            // nothing
            list_del_init(&node->node);
            kfree(node);
        }
    }

    hybm_gvm_ib_unregister_peer(g_gvm_agent_info.ib_priv);
    g_gvm_agent_info.agentdrv_msg_unregister_func(&gvm_common_msg_client);
    agent_symbol_put();
    g_gvm_agent_info.devpid = -1;
}

module_init(gvm_agent_init);
module_exit(gvm_agent_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("global virtual memory manager for Userland applications");
