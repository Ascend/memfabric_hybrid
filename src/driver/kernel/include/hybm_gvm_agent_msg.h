/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_GVM_AGENT_MSG_H
#define MF_HYBRID_HYBM_GVM_AGENT_MSG_H

#include <linux/types.h>

#define DEVDRV_COMMON_MSG_PROFILE 4

#define HYBM_GVM_S2A_MSG_SEND_MAGIC 0x6789
#define HYBM_GVM_S2A_MSG_RCV_MAGIC  0x9876

enum HYBM_GVM_AGENT_MSG_TYPE {
    HYBM_GVM_AGENT_MSG_INIT = 0,
    HYBM_GVM_AGENT_MSG_MAP,
    HYBM_GVM_AGENT_MSG_UNMAP,
    HYBM_GVM_AGENT_MSG_FETCH,
    HYBM_GVM_AGENT_MSG_REGISTER,
    HYBM_GVM_AGENT_MSG_MAX
};

struct hybm_gvm_agent_init_msg {
    int hostpid;
    u32 pasid;
    u32 svspid;
};

struct hybm_gvm_agent_mmap_msg {
    u64 va;
    u64 size;
    u32 pasid;
    u32 page_num;
    u64 pa_list[];
};

struct hybm_gvm_agent_unmap_msg {
    u64 va;
    u64 size;
    u64 page_size;
    u32 pasid;
};

struct hybm_gvm_agent_fetch_msg {
    u64 va;
    u64 size;
    u32 page_size;
    int hostpid;
    u32 pasid;
    u32 pa_num;
    u64 pa_list[];
};

struct hybm_gvm_agent_register_msg {
    u64 va;
    u64 size;
    u64 new_va;
    u32 page_size;
    int hostpid;
    u32 pasid;
};

struct hybm_gvm_agent_msg {
    int type;
    u16 valid;
    s16 result;
    u64 body[];
};

#endif // MF_HYBRID_HYBM_GVM_AGENT_MSG_H
