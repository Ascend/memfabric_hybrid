/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 */

#ifndef MF_HYBM_OPS_HYBM_KERNEL_HYBM_BATCH_TRANSFER_H
#define MF_HYBM_OPS_HYBM_KERNEL_HYBM_BATCH_TRANSFER_H

#include <cstdint>
#include "dl_hcomm_api.h"

struct HybmOneSideOpParam {
    ock::mf::ThreadHandle thread;
    ock::mf::ChannelHandle channel;
    uint32_t list_num;
    void **dst_buf_addr_list;
    void **src_buf_addr_list;
    uint64_t *len_list;
    uint64_t remote_flag_addr;
    uint64_t local_flag_addr;
    uint32_t flag_size;
};

extern "C" {
uint32_t HybmBatchWrite(HybmOneSideOpParam *param);
uint32_t HybmBatchRead(HybmOneSideOpParam *param);
}

#endif // MF_HYBM_OPS_HYBM_KERNEL_HYBM_BATCH_TRANSFER_H
