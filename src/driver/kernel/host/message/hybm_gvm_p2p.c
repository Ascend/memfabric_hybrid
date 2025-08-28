/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_gvm_p2p.h"

#include <linux/slab.h>

#include "hybm_gvm_log.h"
#include "hybm_gvm_proc.h"
#include "hybm_gvm_proc_info.h"
#include "hybm_gvm_symbol_get.h"

struct hybm_gvm_p2p_msg_head {
    u32 src_sdid;
    u32 cmdtype;
    u16 valid;  /* validity judgement, 0x5A5A is valide */
    s16 result; /* process result from rp, zero for succ, non zero for fail */
    u32 pad;
    u64 msg_body[0];
};

struct hybm_gvm_p2p_node_info_msg {
    u64 key;
    u64 va;
    u64 size;
    u32 dst_sdid;
};

struct hybm_gvm_p2p_set_wl_reply {
    void *rproc;
    u32 dst_sdid;
};

struct hybm_gvm_p2p_unset_msg {
    u64 key;
    void *rproc;
    u32 dst_sdid;
};

struct hybm_gvm_p2p_fetch_msg {
    u64 va;
    u32 dst_sdid;
    u64 pa_list[HYBM_GVM_PAGE_NUM];
};

#define HYBM_GVM_P2P_MSG_SEND_MAGIC 0x5A5A
#define HYBM_GVM_P2P_MSG_RCV_MAGIC  0xA5A5

enum hybm_gvm_p2p_msg_cmd {
    HYBM_GVM_P2P_MSG_SET_WL,
    HYBM_GVM_P2P_MSG_GET_ADDR,
    HYBM_GVM_P2P_MSG_DESTROY,
    HYBM_GVM_P2P_MSG_FETCH,
    HYBM_GVM_P2P_MSG_MAX
};

typedef int (*hybm_gvm_p2p_msg_rcv_func_t)(u32 sdid, struct hybm_gvm_p2p_msg_head *msg, u32 len);

static bool hybm_gvm_is_local_pod(u32 src_sdid, u32 dst_sdid)
{
    struct sdid_parse_info parse1 = {0};
    struct sdid_parse_info parse2 = {0};
    int ret;

    ret = dbl_parse_sdid(src_sdid, &parse1);
    if (ret != 0) {
        hybm_gvm_err("Parse sdid fail. (ret=%d; src_sdid=%u)", ret, src_sdid);
        return true;
    }

    ret = dbl_parse_sdid(dst_sdid, &parse2);
    if (ret != 0) {
        hybm_gvm_err("Parse sdid fail. (ret=%d; dst_sdid=%u)", ret, dst_sdid);
        return true;
    }
    /* server id only have 10 bits, mask: 0x3FF */
    return ((parse1.server_id & 0x3FF) == (parse2.server_id & 0x3FF));
}

static int hybm_gvm_p2p_set_wl_recv(u32 send_sdid, struct hybm_gvm_p2p_msg_head *msg, u32 len)
{
    struct hybm_gvm_p2p_node_info_msg *nmsg = (struct hybm_gvm_p2p_node_info_msg *)msg->msg_body;
    u32 expect_len = sizeof(struct hybm_gvm_p2p_msg_head) + sizeof(struct hybm_gvm_p2p_node_info_msg);
    struct hybm_gvm_p2p_set_wl_reply *reply = (struct hybm_gvm_p2p_set_wl_reply *)msg->msg_body;
    struct hybm_gvm_process *proc = NULL;
    int ret;
    if (len < expect_len) {
        hybm_gvm_err("set_wl msg size is error. expect(%u)real(%u)", expect_len, len);
        return -EINVAL;
    }

    ret = hybm_gvm_insert_remote(nmsg->dst_sdid, nmsg->key, nmsg->va, nmsg->size, &proc);
    reply->rproc = (void *)proc;
    return ret;
}

static int hybm_gvm_p2p_get_addr_recv(u32 send_sdid, struct hybm_gvm_p2p_msg_head *msg, u32 len)
{
    struct hybm_gvm_p2p_node_info_msg *nmsg = (struct hybm_gvm_p2p_node_info_msg *)msg->msg_body;
    u32 expect_len = sizeof(struct hybm_gvm_p2p_msg_head) + sizeof(struct hybm_gvm_p2p_node_info_msg);
    if (len < expect_len) {
        hybm_gvm_err("get_addr msg size is error. expect(%u)real(%u)", expect_len, len);
        return -EINVAL;
    }
    return hybm_gvm_proc_get_pa(nmsg->dst_sdid, nmsg->va, nmsg->size, msg->msg_body,
                                (len - sizeof(struct hybm_gvm_p2p_msg_head)) / sizeof(u64));
}

static int hybm_gvm_p2p_unset_recv(u32 send_sdid, struct hybm_gvm_p2p_msg_head *msg, u32 len)
{
    struct hybm_gvm_p2p_unset_msg *nmsg = (struct hybm_gvm_p2p_unset_msg *)msg->msg_body;
    u32 expect_len = sizeof(struct hybm_gvm_p2p_msg_head) + sizeof(struct hybm_gvm_p2p_unset_msg);
    if (len < expect_len) {
        hybm_gvm_err("get_addr msg size is error. expect(%u)real(%u)", expect_len, len);
        return -EINVAL;
    }
    return hybm_gvm_unset_remote(nmsg->dst_sdid, nmsg->key, nmsg->rproc);
}

static int hybm_gvm_p2p_fetch_recv(u32 send_sdid, struct hybm_gvm_p2p_msg_head *msg, u32 len)
{
    struct hybm_gvm_p2p_fetch_msg *nmsg = (struct hybm_gvm_p2p_fetch_msg *)msg->msg_body;
    u32 expect_len = sizeof(struct hybm_gvm_p2p_msg_head) + sizeof(struct hybm_gvm_p2p_fetch_msg);
    if (len < expect_len) {
        hybm_gvm_err("get_addr msg size is error. expect(%u)real(%u)", expect_len, len);
        return -EINVAL;
    }
    return hybm_gvm_fetch_remote(nmsg->dst_sdid, nmsg->va, nmsg->pa_list);
}

static const hybm_gvm_p2p_msg_rcv_func_t rcv_ops[HYBM_GVM_P2P_MSG_MAX] = {
    [HYBM_GVM_P2P_MSG_SET_WL] = hybm_gvm_p2p_set_wl_recv,
    [HYBM_GVM_P2P_MSG_GET_ADDR] = hybm_gvm_p2p_get_addr_recv,
    [HYBM_GVM_P2P_MSG_DESTROY] = hybm_gvm_p2p_unset_recv,
    [HYBM_GVM_P2P_MSG_FETCH] = hybm_gvm_p2p_fetch_recv,
};

static int hybm_gvm_p2p_msg_check(u32 devid, u32 sdid, struct data_input_info *data)
{
    struct hybm_gvm_p2p_msg_head *msg = NULL;

    if (devid >= HYBM_MAX_DEVICE_NUM) {
        hybm_gvm_err("Invalid devid. (devid=%u)", devid);
        return -EINVAL;
    }

    if ((data == NULL) || (data->data == NULL)) {
        hybm_gvm_err("Invalid data. (devid=%u; sdid=%u)", devid, sdid);
        return -EINVAL;
    }

    if (data->in_len < sizeof(struct hybm_gvm_p2p_msg_head)) {
        hybm_gvm_err("Invalid data len. (in_len=%u; data_size=%ld; devid=%u; sdid=%u)", data->in_len,
                     sizeof(struct hybm_gvm_p2p_msg_head), devid, sdid);
        return -EINVAL;
    }
    msg = (struct hybm_gvm_p2p_msg_head *)data->data;
    if (msg->valid != HYBM_GVM_P2P_MSG_SEND_MAGIC) {
        hybm_gvm_err("Invalid magic. (magic=0x%x; devid=%u; sdid=%u; cmdtype=%d)", msg->valid, devid, sdid,
                     msg->cmdtype);
        return -EINVAL;
    }

    if (msg->cmdtype >= (u32)HYBM_GVM_P2P_MSG_MAX) {
        hybm_gvm_err("Invalid cmd. (cmd=%u; devid=%u; sdid=%u)", msg->cmdtype, devid, sdid);
        return -EINVAL;
    }
    return 0;
}

static int hybm_gvm_p2p_msg_recv(u32 devid, u32 sdid, struct data_input_info *data)
{
    struct hybm_gvm_p2p_msg_head *msg = NULL;
    int ret;

    ret = hybm_gvm_p2p_msg_check(devid, sdid, data);
    if (ret != 0) {
        hybm_gvm_err("p2p msg check fail. (ret=%d; devid=%u; sdid=%u)", ret, devid, sdid);
        return ret;
    }

    msg = data->data;
    hybm_gvm_debug("gvm_p2p_msg_recv, type:%u src_dev:%u dst_sdid:%u", msg->cmdtype, devid, sdid);
    if (rcv_ops[msg->cmdtype] != NULL) {
        ret = rcv_ops[msg->cmdtype](sdid, msg, data->in_len);
        data->out_len = data->in_len;
        msg->valid = HYBM_GVM_P2P_MSG_RCV_MAGIC;
        msg->result = ret;
    } else {
        hybm_gvm_warn("not found msg handle (type:%u)", msg->cmdtype);
    }
    return 0;
}

// src_devid dst_sdid
static int hybm_gvm_p2p_msg_sync_send(u32 src_devid, u32 dst_sdid, struct hybm_gvm_p2p_msg_head *msg, size_t size)
{
    struct data_input_info data = {
        .data = (void *)msg, .data_len = size, .in_len = size, .out_len = 0, .msg_mode = DEVDRV_S2S_SYNC_MODE};
    int ret;

    if (hybm_gvm_is_local_pod(msg->src_sdid, dst_sdid)) { // local msg
        return hybm_gvm_p2p_msg_recv(src_devid, dst_sdid, &data);
    }

    ret = devdrv_s2s_msg_send(src_devid, dst_sdid, DEVDRV_S2S_MSG_TEST, DEVDRV_S2S_TO_HOST, &data);
    if (ret != 0) {
        hybm_gvm_err("p2p sync send fail. (ret=%d; src_devid=%u; dst_sdid=%u)", ret, src_devid, dst_sdid);
        return ret;
    }
    return 0;
}

int hybm_gvm_p2p_set_wl(u32 src_sdid, u32 src_devid, u32 dst_sdid, struct gvm_node *node, void **remote)
{
    u64 msg_len = sizeof(struct hybm_gvm_p2p_msg_head) + sizeof(struct hybm_gvm_p2p_node_info_msg);
    struct hybm_gvm_p2p_msg_head *msg = NULL;
    struct hybm_gvm_p2p_node_info_msg *nmsg = NULL;
    struct hybm_gvm_p2p_set_wl_reply *reply;
    int ret;
    *remote = NULL;

    msg = kzalloc(msg_len, GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        hybm_gvm_err("kzalloc hybm_gvm_p2p_msg_head fail.");
        return -ENOMEM;
    }

    msg->src_sdid = src_sdid;
    msg->cmdtype = HYBM_GVM_P2P_MSG_SET_WL;
    msg->result = 0;
    msg->valid = HYBM_GVM_P2P_MSG_SEND_MAGIC;
    nmsg = (struct hybm_gvm_p2p_node_info_msg *)msg->msg_body;

    nmsg->key = node->shm_key;
    nmsg->va = node->va;
    nmsg->size = node->size;
    nmsg->dst_sdid = dst_sdid;
    ret = hybm_gvm_p2p_msg_sync_send(src_devid, dst_sdid, msg, msg_len);
    if ((ret != 0) || (msg->result < 0) || (msg->valid != HYBM_GVM_P2P_MSG_RCV_MAGIC)) {
        hybm_gvm_err("msg send fail. (ret=%d; result=%d; valid=0x%x; devid=%u; sdid=%u)", ret, msg->result, msg->valid,
                     src_devid, dst_sdid);
        ret = -EFAULT;
        goto free_and_return;
    }

    reply = (struct hybm_gvm_p2p_set_wl_reply *)msg->msg_body;
    *remote = reply->rproc;
    ret = msg->result;
free_and_return:
    kfree(msg);
    return ret;
}

void hybm_gvm_p2p_unset_wl(u32 src_sdid, u32 src_devid, u32 dst_sdid, u64 key, void *remote)
{
    u64 msg_len = sizeof(struct hybm_gvm_p2p_msg_head) + sizeof(struct hybm_gvm_p2p_unset_msg);
    struct hybm_gvm_p2p_msg_head *msg = NULL;
    struct hybm_gvm_p2p_unset_msg *nmsg = NULL;
    int ret;

    msg = kzalloc(msg_len, GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        hybm_gvm_err("kzalloc hybm_gvm_p2p_msg_head fail.");
        return;
    }

    msg->src_sdid = src_sdid;
    msg->cmdtype = HYBM_GVM_P2P_MSG_DESTROY;
    msg->result = 0;
    msg->valid = HYBM_GVM_P2P_MSG_SEND_MAGIC;
    nmsg = (struct hybm_gvm_p2p_unset_msg *)msg->msg_body;

    nmsg->key = key;
    nmsg->rproc = remote;
    nmsg->dst_sdid = dst_sdid;
    ret = hybm_gvm_p2p_msg_sync_send(src_devid, dst_sdid, msg, msg_len);
    if ((ret != 0) || (msg->result < 0) || (msg->valid != HYBM_GVM_P2P_MSG_RCV_MAGIC)) {
        hybm_gvm_err("msg send fail. (ret=%d; result=%d; valid=0x%x; devid=%u; sdid=%u)", ret, msg->result, msg->valid,
                     src_devid, dst_sdid);
    }

    kfree(msg);
}

int hybm_gvm_p2p_get_addr(struct hybm_gvm_process *proc, u32 src_sdid, u32 src_devid, u32 dst_sdid,
                          struct gvm_node *node)
{
    u64 num = node->size / HYBM_GVM_PAGE_SIZE;
    u64 msg_len =
        sizeof(struct hybm_gvm_p2p_msg_head) + MAX(sizeof(u64) * num, sizeof(struct hybm_gvm_p2p_node_info_msg));
    struct hybm_gvm_p2p_msg_head *msg = NULL;
    struct hybm_gvm_p2p_node_info_msg *nmsg = NULL;
    int ret;

    msg = kzalloc(msg_len, GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        hybm_gvm_err("kzalloc hybm_gvm_p2p_msg_head fail.");
        return -ENOMEM;
    }

    msg->src_sdid = src_sdid;
    msg->cmdtype = HYBM_GVM_P2P_MSG_GET_ADDR;
    msg->result = 0;
    msg->valid = HYBM_GVM_P2P_MSG_SEND_MAGIC;
    nmsg = (struct hybm_gvm_p2p_node_info_msg *)msg->msg_body;
    nmsg->va = node->va;
    nmsg->size = node->size;
    nmsg->dst_sdid = dst_sdid;
    ret = hybm_gvm_p2p_msg_sync_send(src_devid, dst_sdid, msg, msg_len);
    if ((ret != 0) || (msg->result != 0) || (msg->valid != HYBM_GVM_P2P_MSG_RCV_MAGIC)) {
        hybm_gvm_err("msg send fail. (ret=%d; result=%d; valid=0x%x; devid=%u; sdid=%u)", ret, msg->result, msg->valid,
                     proc->devid, dst_sdid);
        ret = -EFAULT;
        goto result;
    }

    ret = hybm_gvm_proc_set_pa(proc, dst_sdid, node, msg->msg_body, num);
    if (ret != 0) {
        hybm_gvm_err("set local pa failed. ret(%d)", ret);
        goto result;
    }

result:
    kfree(msg);
    return ret;
}

int hybm_gvm_p2p_fetch(u32 src_sdid, u32 src_devid, u32 dst_sdid, u64 va, u64 *pa_list)
{
    u64 msg_len = sizeof(struct hybm_gvm_p2p_msg_head) + sizeof(struct hybm_gvm_p2p_fetch_msg);
    struct hybm_gvm_p2p_msg_head *msg = NULL;
    struct hybm_gvm_p2p_fetch_msg *nmsg = NULL;
    u32 i;
    int ret;

    msg = kzalloc(msg_len, GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        hybm_gvm_err("kzalloc hybm_gvm_p2p_msg_head fail.");
        return -ENOMEM;
    }

    msg->src_sdid = src_sdid;
    msg->cmdtype = HYBM_GVM_P2P_MSG_FETCH;
    msg->result = 0;
    msg->valid = HYBM_GVM_P2P_MSG_SEND_MAGIC;
    nmsg = (struct hybm_gvm_p2p_fetch_msg *)msg->msg_body;

    nmsg->va = va;
    nmsg->dst_sdid = dst_sdid;
    ret = hybm_gvm_p2p_msg_sync_send(src_devid, dst_sdid, msg, msg_len);
    if ((ret != 0) || (msg->result < 0) || (msg->valid != HYBM_GVM_P2P_MSG_RCV_MAGIC)) {
        hybm_gvm_err("msg send fail. (ret=%d; result=%d; valid=0x%x; devid=%u; sdid=%u)", ret, msg->result, msg->valid,
                     src_devid, dst_sdid);
        ret = -EFAULT;
        goto free_and_return;
    }

    for (i = 0; i < HYBM_GVM_PAGE_NUM; i++) {
        pa_list[i] = nmsg->pa_list[i];
    }
    ret = msg->result;
free_and_return:
    kfree(msg);
    return ret;
}

int hybm_gvm_p2p_msg_register(void)
{
    int ret;
    // TODO: 需要添加专门的消息枚举,当前暂时使用TEST
    ret = devdrv_register_s2s_msg_proc_func(DEVDRV_S2S_MSG_TEST, hybm_gvm_p2p_msg_recv);
    if (ret != 0) {
        hybm_gvm_err("register p2p msg func fail. (ret=%d)", ret);
        return ret;
    }
    return 0;
}

void hybm_gvm_p2p_msg_unregister(void)
{
    (void)devdrv_unregister_s2s_msg_proc_func(DEVDRV_S2S_MSG_TEST);
    hybm_gvm_info("hybm_gvm_p2p_msg_unregister");
}
