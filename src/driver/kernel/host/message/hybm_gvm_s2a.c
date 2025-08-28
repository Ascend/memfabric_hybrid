/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <linux/slab.h>
#include "hybm_gvm_agent_msg.h"
#include "hybm_gvm_log.h"
#include "hybm_gvm_symbol_get.h"
#include "hybm_gvm_s2a.h"

static int hybm_gvm_agent_msg_send(u32 devid, struct hybm_gvm_agent_msg *msg, u32 in_data_len, u32 out_data_len,
                                   u32 *real_out_len)
{
    hybm_gvm_debug("hybm_gvm_agent_msg_send, type:%d", msg->type);
    // max len is 0x10000
    return devdrv_common_msg_send(devid, (void *)msg, in_data_len, out_data_len, real_out_len,
                                  DEVDRV_COMMON_MSG_PROFILE);
}

int hybm_gvm_to_agent_init(u32 devid, u32 *pasid, u32 *svspid)
{
    struct hybm_gvm_agent_msg *msg;
    struct hybm_gvm_agent_init_msg *init_body;
    u32 in_len, out_len;
    int ret;

    in_len = sizeof(struct hybm_gvm_agent_msg) + sizeof(struct hybm_gvm_agent_init_msg);
    msg = kzalloc(in_len, GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        hybm_gvm_err("kzalloc msg fail.");
        return -ENOMEM;
    }

    msg->type = HYBM_GVM_AGENT_MSG_INIT;
    msg->valid = HYBM_GVM_S2A_MSG_SEND_MAGIC;
    msg->result = 0;

    init_body = (struct hybm_gvm_agent_init_msg *)msg->body;
    init_body->hostpid = (current->tgid);
    init_body->pasid = 0;
    init_body->svspid = 0;

    ret = hybm_gvm_agent_msg_send(devid, msg, in_len, in_len, &out_len);
    if (ret != 0 || out_len != in_len || msg->valid != HYBM_GVM_S2A_MSG_RCV_MAGIC || msg->result != 0) {
        hybm_gvm_err("init msg send fail. (ret=%d; result=%d; valid=0x%x; devid=%u; size=%u)", ret, msg->result,
                     msg->valid, devid, out_len);
        kfree(msg);
        return -EBUSY;
    }

    *pasid = init_body->pasid;
    *svspid = init_body->svspid;
    kfree(msg);
    return 0;
}

int hybm_gvm_to_agent_mmap(u32 devid, u32 pasid, u64 va, u64 size, u64 *pa_list, u32 num)
{
    struct hybm_gvm_agent_msg *msg;
    struct hybm_gvm_agent_mmap_msg *map_body;
    u32 in_len, out_len, i;
    int ret;

    in_len = sizeof(struct hybm_gvm_agent_msg) + sizeof(struct hybm_gvm_agent_mmap_msg) + sizeof(u64) * num;
    msg = kzalloc(in_len, GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        hybm_gvm_err("kzalloc msg fail.");
        return -ENOMEM;
    }

    msg->type = HYBM_GVM_AGENT_MSG_MAP;
    msg->valid = HYBM_GVM_S2A_MSG_SEND_MAGIC;
    msg->result = 0;

    map_body = (struct hybm_gvm_agent_mmap_msg *)msg->body;
    map_body->va = va;
    map_body->size = size;
    map_body->pasid = pasid;
    map_body->page_num = num;
    for (i = 0; i < num; i++) {
        map_body->pa_list[i] = pa_list[i];
    }

    ret = hybm_gvm_agent_msg_send(devid, msg, in_len, in_len, &out_len);
    if (ret != 0 || out_len != in_len || msg->valid != HYBM_GVM_S2A_MSG_RCV_MAGIC || msg->result != 0) {
        hybm_gvm_err("map msg send fail. (ret=%d; result=%d; valid=0x%x; devid=%u; size=%u)", ret, msg->result,
                     msg->valid, devid, out_len);
        kfree(msg);
        return -EBUSY;
    }

    kfree(msg);
    return 0;
}

int hybm_gvm_to_agent_unmap(u32 devid, u32 pasid, u64 va, u64 size, u64 page_size)
{
    struct hybm_gvm_agent_msg *msg;
    struct hybm_gvm_agent_unmap_msg *unmap_body;
    u32 in_len, out_len;
    int ret;

    in_len = sizeof(struct hybm_gvm_agent_msg) + sizeof(struct hybm_gvm_agent_unmap_msg);
    msg = kzalloc(in_len, GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        hybm_gvm_err("kzalloc msg fail.");
        return -ENOMEM;
    }

    msg->type = HYBM_GVM_AGENT_MSG_UNMAP;
    msg->valid = HYBM_GVM_S2A_MSG_SEND_MAGIC;
    msg->result = 0;

    unmap_body = (struct hybm_gvm_agent_unmap_msg *)msg->body;
    unmap_body->va = va;
    unmap_body->pasid = pasid;
    unmap_body->size = size;
    unmap_body->page_size = page_size;

    ret = hybm_gvm_agent_msg_send(devid, msg, in_len, in_len, &out_len);
    if (ret != 0 || out_len != in_len || msg->valid != HYBM_GVM_S2A_MSG_RCV_MAGIC || msg->result != 0) {
        hybm_gvm_err("unmap msg send fail. (ret=%d; result=%d; valid=0x%x; devid=%u; size=%u)", ret, msg->result,
                     msg->valid, devid, out_len);
        kfree(msg);
        return -EBUSY;
    }

    kfree(msg);
    return 0;
}

int hybm_gvm_to_agent_fetch(u32 devid, u32 pasid, u64 va, u64 size, u64 *pa_list, u32 num)
{
    struct hybm_gvm_agent_msg *msg;
    struct hybm_gvm_agent_fetch_msg *fetch_body;
    u32 in_len, out_len, i;
    int ret;

    in_len = sizeof(struct hybm_gvm_agent_msg) + sizeof(struct hybm_gvm_agent_fetch_msg) + sizeof(u64) * num;
    msg = kzalloc(in_len, GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        hybm_gvm_err("kzalloc msg fail.");
        return -ENOMEM;
    }

    msg->type = HYBM_GVM_AGENT_MSG_FETCH;
    msg->valid = HYBM_GVM_S2A_MSG_SEND_MAGIC;
    msg->result = 0;

    fetch_body = (struct hybm_gvm_agent_fetch_msg *)msg->body;
    fetch_body->va = va;
    fetch_body->size = size;
    fetch_body->hostpid = (current->tgid);
    fetch_body->pasid = pasid;

    ret = hybm_gvm_agent_msg_send(devid, msg, in_len, in_len, &out_len);
    if (ret != 0 || out_len != in_len || msg->valid != HYBM_GVM_S2A_MSG_RCV_MAGIC || msg->result != 0) {
        hybm_gvm_err("fetch msg send fail. (ret=%d; result=%d; valid=0x%x; devid=%u; size=%u)", ret, msg->result,
                     msg->valid, devid, out_len);
        kfree(msg);
        return -EBUSY;
    }

    for (i = 0; i < num; i++) {
        pa_list[i] = fetch_body->pa_list[i];
    }
    kfree(msg);
    return 0;
}
