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

#define HYBM_GVM_QUERY_SVM_PA_MAX_NUM (2U << 20) // 2M

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

    u32 (*get_mem_page_size_func)(struct devmm_svm_process_id *process_id, u64 addr, u64 size);
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
    u32 devid;
    int devpid;
};
struct gvm_agent_info g_gvm_agent_info = {0};

int agentdrv_register_common_msg_client(struct agentdrv_common_msg_client *msg_client)
{
    if (g_gvm_agent_info.agentdrv_msg_register_func == NULL) {
        hybm_gvm_err("symbol agentdrv_register_common_msg_client not get.");
        return -ENXIO;
    }
    return g_gvm_agent_info.agentdrv_msg_register_func(msg_client);
}

int agentdrv_unregister_common_msg_client(const struct agentdrv_common_msg_client *msg_client)
{
    if (g_gvm_agent_info.agentdrv_msg_unregister_func == NULL) {
        hybm_gvm_err("symbol agentdrv_unregister_common_msg_client not get.");
        return -ENXIO;
    }
    return g_gvm_agent_info.agentdrv_msg_unregister_func(msg_client);
}

u32 devmm_get_mem_page_size(struct devmm_svm_process_id *process_id, u64 addr, u64 size)
{
    if (g_gvm_agent_info.get_mem_page_size_func == NULL) {
        hybm_gvm_err("symbol devmm_get_mem_page_size not get.");
        return 0;
    }
    return g_gvm_agent_info.get_mem_page_size_func(process_id, addr, size);
}

int devmm_get_mem_pa_list(struct devmm_svm_process_id *process_id, u64 addr, u64 size, u64 *pa_list, u32 pa_num)
{
    if (g_gvm_agent_info.get_mem_pa_func == NULL) {
        hybm_gvm_err("symbol devmm_get_mem_pa_list not get.");
        return -ENXIO;
    }
    return g_gvm_agent_info.get_mem_pa_func(process_id, addr, size, pa_list, pa_num);
}

void devmm_put_mem_pa_list(struct devmm_svm_process_id *process_id, u64 addr, u64 size, u64 *pa_list, u32 pa_num)
{
    if (g_gvm_agent_info.put_mem_pa_func == NULL) {
        hybm_gvm_err("symbol devmm_put_mem_pa_list not get.");
        return;
    }
    g_gvm_agent_info.put_mem_pa_func(process_id, addr, size, pa_list, pa_num);
}

int svm_get_pasid(int vpid, int dev_id)
{
    if (g_gvm_agent_info.get_pasid_func == NULL) {
        hybm_gvm_err("symbol svm_get_pasid not get.");
        return -ENXIO;
    }
    return g_gvm_agent_info.get_pasid_func(vpid, dev_id);
}

int svm_get_svsp_pasid(int vpid, int dev_id)
{
    if (g_gvm_agent_info.get_svsp_pasid_func == NULL) {
        hybm_gvm_err("symbol svm_get_svsp_pasid not get.");
        return -ENXIO;
    }
    return g_gvm_agent_info.get_svsp_pasid_func(vpid, dev_id);
}

u64 svm_svsp_roce_mmap(u64 len, int pasid, u64 addr, u64 map_size, bool specify_address)
{
    if (g_gvm_agent_info.svsp_mmap_func == NULL) {
        hybm_gvm_err("symbol svm_svsp_roce_mmap not get.");
        return 0;
    }
    return g_gvm_agent_info.svsp_mmap_func(len, pasid, addr, map_size, specify_address);
}

int svm_svsp_roce_populate(u64 addr, u64 pfn, u64 size, int pasid)
{
    if (g_gvm_agent_info.svsp_populate_func == NULL) {
        hybm_gvm_err("symbol svm_svsp_roce_populate not get.");
        return -ENXIO;
    }
    return g_gvm_agent_info.svsp_populate_func(addr, pfn, size, pasid);
}

void svm_svsp_roce_munmap(u64 start, u64 len, int pasid)
{
    if (g_gvm_agent_info.svsp_unmap_func == NULL) {
        hybm_gvm_err("symbol svm_svsp_roce_munmap not get.");
        return;
    }
    g_gvm_agent_info.svsp_unmap_func(start, len, pasid);
}

int devdrv_query_process_by_host_pid_kernel(u32 host_pid, u32 chip_id, enum devdrv_process_type cp_type, u32 vfid,
                                            int *pid)
{
    if (g_gvm_agent_info.devdrv_query_pid_func == NULL) {
        hybm_gvm_err("symbol devdrv_query_process_by_host_pid_kernel not get.");
        return -ENXIO;
    }
    return g_gvm_agent_info.devdrv_query_pid_func(host_pid, chip_id, cp_type, vfid, pid);
}

void *ib_register_peer_memory_client(const struct peer_memory_client *peer_client,
                                     invalidate_peer_memory *invalidate_callback)
{
    if (g_gvm_agent_info.ib_register_func == NULL) {
        hybm_gvm_err("symbol ib_register_peer_memory_client not get.");
        return NULL;
    }
    return g_gvm_agent_info.ib_register_func(peer_client, invalidate_callback);
}

void ib_unregister_peer_memory_client(void *reg_handle)
{
    if (g_gvm_agent_info.ib_unregister_func == NULL) {
        hybm_gvm_err("symbol ib_unregister_peer_memory_client not get.");
        return;
    }
    g_gvm_agent_info.ib_unregister_func(reg_handle);
}

bool gvm_agent_check_task(void)
{
    int svspid = -1;
    if (g_gvm_agent_info.devpid <= 0) {
        return false;
    }
    svspid = svm_get_svsp_pasid(g_gvm_agent_info.devpid, g_gvm_agent_info.devid);
    if (svspid < 0) {
        g_gvm_agent_info.devpid = -1;
    }
    return (g_gvm_agent_info.devpid > 0);
}

int gvm_agent_init_recv(struct hybm_gvm_agent_msg *msg, u32 devid)
{
    int devpid, pasid, svspid, ret;
    struct hybm_gvm_agent_init_msg *init_body;
    if (msg == NULL || msg->body == NULL || msg->type != HYBM_GVM_AGENT_MSG_INIT) {
        hybm_gvm_err("input msg is invalid.");
        return -EINVAL;
    }

    init_body = (struct hybm_gvm_agent_init_msg *)msg->body;
    ret = devdrv_query_process_by_host_pid_kernel(init_body->hostpid, devid, DEVDRV_PROCESS_CP1, 0, &devpid);
    if (ret != 0) {
        hybm_gvm_err("query devpid failed. ret:%d,devid:%u,hostpid:%d", ret, devid, init_body->hostpid);
        return -EINVAL;
    }

    pasid = svm_get_pasid(devpid, devid);
    if (pasid < 0) {
        hybm_gvm_err("get pasid failed. ret:%d,hostpid:%d,devpid:%d", pasid, init_body->hostpid, devpid);
        return -EINVAL;
    }

    svspid = svm_get_svsp_pasid(devpid, devid);
    if (svspid < 0) {
        hybm_gvm_err("get svsp pasid failed. ret:%d,hostpid:%d,devpid:%d", svspid, init_body->hostpid, devpid);
        return -EINVAL;
    }

    init_body->pasid = pasid;
    init_body->svspid = svspid;
    g_gvm_agent_info.devpid = devpid;
    g_gvm_agent_info.devid = devid;
    hybm_gvm_debug("gvm_agent_init_recv, hostpid:%d,devpid:%d,pasid:%d,svspid:%d", init_body->hostpid, devpid, pasid,
                   svspid);
    return 0;
}

static int gvm_agent_map_svsp(u64 va, u64 size, u64 *pa_list, u32 num, u32 pasid)
{
    if (pa_list == NULL) {
        hybm_gvm_err("input is error, pa_list is null");
        return -EINVAL;
    }
    u64 i, iva, ret_va, page_size;
    int ret = 0;

    iva = va;
    page_size = size / num;
    for (i = 0; i < num; i++) {
        ret_va = svm_svsp_roce_mmap(page_size, pasid, iva, page_size, true);
        if (ret_va != iva) {
            hybm_gvm_err("map mem failed, ret: 0x%llx page_size: %llx", ret_va, page_size);
            ret = -EINVAL;
            break;
        }

        ret = svm_svsp_roce_populate(iva, PFN_DOWN(pa_list[i]), page_size, pasid);
        if (ret != 0) {
            hybm_gvm_err("populate mem failed, ret: %d", ret);
            i++;
            break;
        }
        iva += page_size;
    }

    if (ret != 0) {
        while (i > 0) {
            i--;
            svm_svsp_roce_munmap(va + i * page_size, page_size, pasid);
        }
    }
    return ret;
}

static int gvm_agent_add_roce_node(u64 va, u64 size, u64 *pa_list, u32 num)
{
    u32 i;
    struct gvm_dev_mem_node *node = NULL;

    if (size != HYBM_HPAGE_SIZE * HYBM_GVM_PAGE_NUM || num != HYBM_GVM_PAGE_NUM) {
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
    node->pa_num = num;
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
    if (msg == NULL || msg->body == NULL || msg->type != HYBM_GVM_AGENT_MSG_MAP) {
        hybm_gvm_err("input msg type is invalid.");
        return -EINVAL;
    }

    map_body = (struct hybm_gvm_agent_mmap_msg *)msg->body;
    va = map_body->va;
    pasid = map_body->pasid;
    num = map_body->page_num;
    size = map_body->size;
    if (size != HYBM_GVM_PAGE_SIZE || num == 0 || size % num) {
        hybm_gvm_err("input size,num error. size:0x%llx,num:%u", size, num);
        return -EINVAL;
    }

    if ((va >= HYBM_SVM_START && va < HYBM_SVM_END) || (va >= HYBM_SVSP_START && va < HYBM_SVSP_END)) {
        ret = gvm_agent_map_svsp(va, size, map_body->pa_list, num, pasid);
        hybm_gvm_debug("map svsp, size:0x%llx, ret:%d", size, ret);
    } else {
        ret = gvm_agent_add_roce_node(va, size, map_body->pa_list, num);
        hybm_gvm_debug("map roce, size:0x%llx, ret:%d", size, ret);
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

    if (msg == NULL || msg->body ==NULL || msg->type != HYBM_GVM_AGENT_MSG_UNMAP) {
        hybm_gvm_err("input msg type is invalid.");
        return -EINVAL;
    }

    unmap_body = (struct hybm_gvm_agent_unmap_msg *)msg->body;
    pasid = unmap_body->pasid;
    va = unmap_body->va;
    size = unmap_body->size;
    page_size = unmap_body->page_size;
    if (!IS_MULTIPLE_OF(va, PAGE_SIZE) ||
        !IS_MULTIPLE_OF(page_size, PAGE_SIZE) || !IS_MULTIPLE_OF(size, page_size)) {
        hybm_gvm_err("input addr error. size:0x%llx, pg_size:0x%llx", size, page_size);
        return -EINVAL;
    }

    if ((va >= HYBM_SVM_START && va < HYBM_SVM_END) || (va >= HYBM_SVSP_START && va < HYBM_SVSP_END)) {
        if (!gvm_agent_check_task()) {
            hybm_gvm_debug("gvm_agent_unmap, process exited, skip unmap");
        } else {
            num = size / page_size;
            for (i = 0; i < num; i++) {
                svm_svsp_roce_munmap(va + i * page_size, page_size, pasid);
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

    hybm_gvm_debug("gvm_agent_unmap, size:0x%llx, pg_size:0x%llx", size, page_size);
    return 0;
}

static int gvm_agent_map_svm_pa(u32 devid, struct hybm_gvm_agent_fetch_msg *fetch)
{
    struct devmm_svm_process_id id = {0};
    u64 *pa_list = NULL;
    u64 new_ed;
    int ret = 0;
    u64 pg_size;
    u32 i;
    u32 num;
    id.hostpid = fetch->hostpid;
    id.devid = devid;

    pg_size = devmm_get_mem_page_size(&id, fetch->va, fetch->size);
    if (pg_size == 0) {
        hybm_gvm_err("query va page_size failed. size:0x%llx", fetch->size);
        return -EINVAL;
    }

    if (fetch->pa_num > 0 && pg_size != HYBM_HPAGE_SIZE) {
        hybm_gvm_err("record mem pg_size must be 2M, real is 0x%llx", pg_size);
        return -EBUSY;
    }

    if (fetch->pa_num == 0) {
        new_ed = GVM_ALIGN_UP(fetch->va + fetch->size, pg_size);
        fetch->va = GVM_ALIGN_DOWN(fetch->va, pg_size);
        fetch->size = new_ed - fetch->va;
    }

    num = fetch->size / pg_size;
    if (!IS_MULTIPLE_OF(fetch->size, pg_size) || num > HYBM_GVM_QUERY_SVM_PA_MAX_NUM) {
        hybm_gvm_err("query mem size is invalid, size:0x%llx pg_size:0x%llx", fetch->size, pg_size);
        return -EBUSY;
    }

    pa_list = kzalloc(sizeof(u64) * num, GFP_KERNEL | __GFP_ACCOUNT);
    if (pa_list == NULL) {
        hybm_gvm_err("Kzalloc pa_list is NULL. num:%u", num);
        return -EINVAL;
    }

    ret = devmm_get_mem_pa_list(&id, fetch->va, fetch->size, pa_list, num);
    if (ret != 0) {
        hybm_gvm_err("get svm pa failed. ret:%d", ret);
        kfree(pa_list);
        return -EINVAL;
    }

    fetch->page_size = pg_size;
    for (i = 0; i < fetch->pa_num; i++) {
        fetch->pa_list[i] = pa_list[i];
    }

    ret = gvm_agent_map_svsp(fetch->va, fetch->size, pa_list, num, fetch->pasid);
    devmm_put_mem_pa_list(&id, fetch->va, fetch->size, pa_list, num);
    kfree(pa_list);
    hybm_gvm_debug("gvm_agent_fetch_map, size:0x%llx,num:%x,ret:%d", fetch->size, num, ret);
    return ret;
}

int gvm_agent_fetch_recv(struct hybm_gvm_agent_msg *msg, u32 devid)
{
    u64 va;
    struct hybm_gvm_agent_fetch_msg *fetch_body;
    int ret = 0;

    if (msg == NULL || msg->body == NULL || msg->type != HYBM_GVM_AGENT_MSG_FETCH) {
        hybm_gvm_err("input msg type is invalid.");
        return -EINVAL;
    }

    fetch_body = (struct hybm_gvm_agent_fetch_msg *)msg->body;
    va = fetch_body->va;
    if (va < HYBM_SVM_START || va + fetch_body->size > HYBM_SVM_END) {
        hybm_gvm_err("input addr is out of range, size:0x%llx", fetch_body->size);
        return -EINVAL;
    }
    if (fetch_body->pa_num > 0 && (va % HYBM_GVM_PAGE_SIZE || fetch_body->size != HYBM_GVM_PAGE_SIZE)) {
        hybm_gvm_err("input addr error, size:0x%llx", fetch_body->size);
        return -EINVAL;
    }

    ret = gvm_agent_map_svm_pa(devid, fetch_body);
    if (ret != 0) {
        hybm_gvm_err("map svm mem failed, ret:%d", ret);
    }

    hybm_gvm_debug("gvm_agent_fetch, size:0x%llx", fetch_body->size);
    return ret;
}

static int gvm_agent_map_svsp_inner_v2(u64 va, u64 size, u64 *pa_list, u32 num, u32 pasid)
{
    if (pa_list == NULL) {
        hybm_gvm_err("input is error, pa_list is null");
        return -EINVAL;
    }
    u64 i, iva, ret_va, page_size;
    int ret = 0;
    page_size = size / num;
    if (va == HYBM_GVM_REGISTER_ADDR) {
        ret_va = svm_svsp_roce_mmap(HYBM_GVM_REGISTER_SIZE, pasid, va, page_size, true);
        if (ret_va != va) {
            hybm_gvm_err("map mem failed, ret va:0x%llx is inconsistent with input va", ret_va);
            return -EINVAL;
        }
    }

    iva = va;
    for (i = 0; i < num; i++) {
        ret = svm_svsp_roce_populate(iva, PFN_DOWN(pa_list[i]), page_size, pasid);
        if (ret != 0) {
            hybm_gvm_err("populate mem failed, ret: %d", ret);
            i++;
            break;
        }
        iva += page_size;
    }

    if (ret != 0) {
        while (i > 0) {
            i--;
            svm_svsp_roce_munmap(va + i * page_size, page_size, pasid);
        }
    }
    return ret;
}

static int gvm_agent_map_svsp_v2(struct hybm_gvm_agent_register_msg *reg, u64 base, u64 pre_size, u64 *pa_list, u32 num)
{
    u32 num2 = reg->size / PAGE_SIZE;
    u64 tmp;
    u64 i;
    u64 j;
    u64 *pa_list2 = NULL;
    int ret;

    pa_list2 = kzalloc(sizeof(u64) * num2, GFP_KERNEL | __GFP_ACCOUNT);
    if (pa_list2 == NULL) {
        hybm_gvm_err("Kzalloc pa_list is NULL. num:%u", num2);
        return -EINVAL;
    }

    for (i = 0; i < num2; i++) {
        tmp = reg->va + PAGE_SIZE * i;
        j = (tmp - base) / pre_size;
        pa_list2[i] = pa_list[j] + (tmp - base) % pre_size;
    }

    ret = gvm_agent_map_svsp_inner_v2(reg->new_va, reg->size, pa_list2, num2, reg->pasid);
    kfree(pa_list2);
    return ret;
}

static int gvm_agent_map_svm_new(u32 devid, struct hybm_gvm_agent_register_msg *reg)
{
    struct devmm_svm_process_id id = {0};
    u64 *pa_list = NULL;
    u64 new_ed;
    u64 new_st;
    int ret = 0;
    u64 pg_size;
    u32 num;
    id.hostpid = reg->hostpid;
    id.devid = devid;

    if (!IS_MULTIPLE_OF(reg->va, PAGE_SIZE) || !IS_MULTIPLE_OF(reg->size, PAGE_SIZE) ||
        reg->size > HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input addr error. size:0x%llx, va:0x%llx", reg->size, reg->va);
        return -EINVAL;
    }

    pg_size = devmm_get_mem_page_size(&id, reg->va, reg->size);
    if (pg_size == 0) {
        hybm_gvm_err("query va page_size failed. size:0x%llx", reg->size);
        return -EINVAL;
    }

    new_ed = GVM_ALIGN_UP(reg->va + reg->size, pg_size);
    new_st = GVM_ALIGN_DOWN(reg->va, pg_size);
    num = (new_ed - new_st) / pg_size;
    if (num > HYBM_GVM_QUERY_SVM_PA_MAX_NUM) {
        hybm_gvm_err("query mem size is invalid, num:0x%x pg_size:0x%llx", num, pg_size);
        return -EBUSY;
    }

    pa_list = kzalloc(sizeof(u64) * num, GFP_KERNEL | __GFP_ACCOUNT);
    if (pa_list == NULL) {
        hybm_gvm_err("Kzalloc pa_list is NULL. num:%u", num);
        return -EINVAL;
    }

    ret = devmm_get_mem_pa_list(&id, new_st, new_ed - new_st, pa_list, num);
    if (ret != 0) {
        hybm_gvm_err("get svm pa failed. ret:%d", ret);
        kfree(pa_list);
        return -EINVAL;
    }

    reg->page_size = PAGE_SIZE;
    if (reg->new_va == new_st && reg->size == new_ed - new_st) {
        ret = gvm_agent_map_svsp(reg->new_va, reg->size, pa_list, num, reg->pasid);
    } else {
        ret = gvm_agent_map_svsp_v2(reg, new_st, pg_size, pa_list, num);
    }
    devmm_put_mem_pa_list(&id, reg->va, reg->size, pa_list, num);
    kfree(pa_list);
    hybm_gvm_debug("gvm_agent_fetch_map, size:0x%llx,num:%x,ret:%d", reg->size, num, ret);
    return ret;
}

int gvm_agent_register_recv(struct hybm_gvm_agent_msg *msg, u32 devid)
{
    u64 va;
    struct hybm_gvm_agent_register_msg *reg_body;
    int ret = 0;

    if (msg == NULL || msg->body == NULL || msg->type != HYBM_GVM_AGENT_MSG_REGISTER) {
        hybm_gvm_err("input msg type is invalid.");
        return -EINVAL;
    }

    reg_body = (struct hybm_gvm_agent_register_msg *)msg->body;
    va = reg_body->va;
    if (va < HYBM_SVM_START || va + reg_body->size > HYBM_SVM_END) {
        hybm_gvm_err("input addr is out of range, size:0x%llx", reg_body->size);
        return -EINVAL;
    }

    ret = gvm_agent_map_svm_new(devid, reg_body);
    if (ret != 0) {
        hybm_gvm_err("map svm mem failed, ret:%d", ret);
    }

    hybm_gvm_debug("gvm_agent_register, size:0x%llx", reg_body->size);
    return ret;
}

typedef int (*hybm_gvm_agent_msg_rcv_func_t)(struct hybm_gvm_agent_msg *msg, u32 devid);
static const hybm_gvm_agent_msg_rcv_func_t rcv_ops[HYBM_GVM_AGENT_MSG_MAX] = {
    [HYBM_GVM_AGENT_MSG_INIT] = gvm_agent_init_recv,
    [HYBM_GVM_AGENT_MSG_MAP] = gvm_agent_map_recv,
    [HYBM_GVM_AGENT_MSG_UNMAP] = gvm_agent_unmap_recv,
    [HYBM_GVM_AGENT_MSG_FETCH] = gvm_agent_fetch_recv,
    [HYBM_GVM_AGENT_MSG_REGISTER] = gvm_agent_register_recv,
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
    .type = AGENTDRV_COMMON_MSG_PROFILE,
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

#define PROCESS_SIGN_LENGTH 49
struct peer_memory_data {
    u32 host_pid;
    u32 devid;
    u32 vfid;
    char pid_sign[PROCESS_SIGN_LENGTH];
    u32 mem_side; /* 0: device mem, 1: host mem */
};

static inline struct gvm_dev_mem_node* find_gvm_dev_mem_node_by_va(struct list_head* head, u64 addr)
{
    struct gvm_dev_mem_node *node = NULL;
    struct list_head *pos = NULL, *n = NULL;
    list_for_each_safe(pos, n, head)
    {
        node = list_entry(pos, struct gvm_dev_mem_node, node);
        if (node->va == addr) {
            break;
        }
    }
    return node;
}

int gvm_peer_mem_acquire(unsigned long addr, size_t size, void *peer_mem_data, char *peer_mem_name,
                         void **client_context)
{
    struct peer_memory_data *memory_data = (struct peer_memory_data *)peer_mem_data;
    u64 va_aligned_start, va_aligned_end, aligned_size, page_size;
    struct gvm_peer_mem_context *mm_context = NULL;
    struct gvm_dev_mem_node *node = NULL;
    u32 page_num;

    if ((memory_data == NULL) || (client_context == NULL)) {
        hybm_gvm_err("input err check private_data, client_context.");
        return false;
    }
    if (!IS_MULTIPLE_OF(addr, HYBM_GVM_PAGE_SIZE) || !IS_MULTIPLE_OF(size, HYBM_GVM_PAGE_SIZE) ||
        size > HYBM_GVM_ALLOC_MAX_SIZE) {
        hybm_gvm_err("invalid addr or size, size:0x%lx", size);
        return false;
    }

    if (size > UINT64_MAX - addr || GVM_ALIGN_UP(size, HYBM_GVM_PAGE_SIZE) > UINT64_MAX - addr) {
        hybm_gvm_err("input addr+size too large");
        return -EINVAL;
    }

    node = find_gvm_dev_mem_node_by_va(&g_gvm_agent_info.roce_head, addr);
    if (node == NULL || node->va != addr) {
        hybm_gvm_err("input addr has not been mapped");
        return false;
    }

    page_size = node->page_size;
    va_aligned_start = round_down(addr, page_size);
    va_aligned_end = round_up((addr + size), page_size);
    aligned_size = va_aligned_end - va_aligned_start;

    page_num = (aligned_size) / page_size;
    mm_context = (struct gvm_peer_mem_context *)vzalloc(sizeof(struct gvm_peer_mem_context) + sizeof(u64) * page_num);
    if (mm_context == NULL) {
        hybm_gvm_err("vzalloc mm_context fail. num=%u", page_num);
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
    memory_data->mem_side = 1U; // DEVMM_MEM_REMOTE_SIDE
    hybm_gvm_debug("gvm_peer_mem_acquire, size:0x%lx, pg_size:0x%llx", size, page_size);
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
        hybm_gvm_err("mm_context(%u) is null or has not been initialized(%u).", mm_context != NULL,
                     (mm_context != NULL ? mm_context->inited_flag : 0));
        return -EINVAL;
    }

    if (mm_context->page_size != HYBM_HPAGE_SIZE || mm_context->aligned_size % HYBM_GVM_PAGE_SIZE) {
        hybm_gvm_err("input param is invalid, pg_size:0x%x,size:0x%llx", mm_context->page_size,
                     mm_context->aligned_size);
        return -EINVAL;
    }

    // check for end addr overflow and loop value overflow
    if (size > UINT64_MAX - addr || GVM_ALIGN_UP(size, HYBM_GVM_PAGE_SIZE) > UINT64_MAX - addr) {
        hybm_gvm_err("input addr+size too large");
        return -EINVAL;
    }

    mutex_lock(&mm_context->context_mutex);
    if (mm_context->get_flag == 1) {
        hybm_gvm_err("already got pages");
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
            hybm_gvm_err("not found agent mem node!");
            mutex_unlock(&mm_context->context_mutex);
            return -EINVAL;
        }

        st = (va - addr) / mm_context->page_size;
        for (i = 0; i < HYBM_GVM_PAGE_NUM; i++) {
            mm_context->pa_list[st + i] = node->pa[i];
        }
    }

    mm_context->get_flag = 1;
    mutex_unlock(&mm_context->context_mutex);
    hybm_gvm_debug("gvm_peer_mem_get_pages, size:0x%lx, pg_size:0x%x", size, mm_context->page_size);
    return 0;
}

int gvm_peer_mem_dma_map(struct sg_table *sg_head, void *context, struct device *dma_device, int *nmap)
{
    struct gvm_peer_mem_context *mm_context = (struct gvm_peer_mem_context *)context;
    struct scatterlist *sg = NULL;
    u32 i = 0;
    int ret;
    if (sg_head == NULL || nmap == NULL) {
        hybm_gvm_err("sg_head(%u) or nmap(%u) is null.", sg_head != NULL, nmap != NULL);
        return -EINVAL;
    }
    if ((mm_context == NULL) || (mm_context->inited_flag != GVM_PEER_INITED_FLAG)) {
        hybm_gvm_err("mm_context(%u) is null or has not been initialized(%u).", mm_context != NULL,
                     (mm_context != NULL ? mm_context->inited_flag : 0));
        return -EINVAL;
    }
    mutex_lock(&mm_context->context_mutex);
    if (mm_context->sg_head != NULL) {
        hybm_gvm_err("sg_head has already been allocated.");
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
            sg_free_table(sg_head);
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
    hybm_gvm_debug("gvm_peer_mem_dma_map");
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
    hybm_gvm_debug("gvm_peer_mem_dma_unmap");
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
    hybm_gvm_debug("gvm_peer_mem_get_page_size");
    return page_size;
}

void gvm_peer_mem_put_pages(struct sg_table *sg_head, void *context)
{
    hybm_gvm_debug("gvm_peer_mem_put_pages");
    return;
}

void gvm_peer_mem_release(void *context)
{
    hybm_gvm_debug("gvm_peer_mem_release");
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
    return ib_register_peer_memory_client(&gvm_mem_client, &mem_invalidate_callback);
}

void hybm_gvm_ib_unregister_peer(void *priv)
{
    ib_unregister_peer_memory_client(priv);
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

    g_gvm_agent_info.get_mem_page_size_func = __symbol_get("devmm_get_mem_page_size");
    if (g_gvm_agent_info.get_mem_page_size_func == NULL) {
        hybm_gvm_err("get symbol devmm_get_mem_page_size fail.");
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
    if (g_gvm_agent_info.get_mem_page_size_func != NULL) {
        __symbol_put("devmm_get_mem_page_size");
        g_gvm_agent_info.get_mem_page_size_func = NULL;
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
        goto init_failed1;
    }

    ret = agentdrv_register_common_msg_client(&gvm_common_msg_client);
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
    agentdrv_unregister_common_msg_client(&gvm_common_msg_client);
init_failed1:
    agent_symbol_put();
    return ret;
}

static void __exit gvm_agent_exit(void)
{
    struct list_head *pos = NULL, *n = NULL;
    struct gvm_dev_mem_node *node;

    hybm_gvm_info("gvm agent exit.");
    if (!list_empty_careful(&g_gvm_agent_info.roce_head)) {
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
    agentdrv_unregister_common_msg_client(&gvm_common_msg_client);
    agent_symbol_put();
    g_gvm_agent_info.devpid = -1;
}

module_init(gvm_agent_init);
module_exit(gvm_agent_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("global virtual memory manager for Userland applications");
