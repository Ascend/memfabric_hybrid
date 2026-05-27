/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef ZBAL_COMM_STRUCT_H
#define ZBAL_COMM_STRUCT_H

#include <string>
#include <vector>

#define ZBAL_FLAG_SIZE 8

#define ZBAL_MAX_AIV_SIZE_PER_NPU 48

#define ZBAL_PROFILING_DEVICE_IDX_OFF   8
#define ZBAL_PROFILING_DEVICE_TRACE_OFF 16
#define ZBAL_PROFILING_FRAME_SHIFT      56
#define ZBAL_PROFILING_BE_SHIFT         62

enum zbal_profiling_name_t : uint16_t {
    ZBAL_PROF_UNKNOWN = 0,
    ZBAL_PROF_LINENO,
    ZBAL_PROF_BARRIER,

    ZBAL_PROF_ALLGATHER_KERNEL_ALL,
    ZBAL_PROF_ALLGATHER_COPY,
    ZBAL_PROF_ALLGATHER_LOCAL_COPY,
    ZBAL_PROF_ALLGATHER_PREPARE_PTR,

    ZBAL_PROF_REDUCESCATTER_KERNEL_ALL,
    ZBAL_PROF_REDUCESCATTER_LOCAL_COPY,
    ZBAL_PROF_REDUCESCATTER_COPY,

    ZBAL_PROF_ALLTOALL_KERNEL_ALL,
    ZBAL_PROF_ALLTOALL_COPY,
    ZBAL_PROF_ALLTOALL_PREPARE_PTR,
    ZBAL_PROF_ALLTOALL_CORE_RANGE,
    ZBAL_PROF_ALLTOALL_INIT_STAT,
    ZBAL_PROF_ALLTOALL_ATOMIC_INC,
    ZBAL_PROF_ALLTOALL_WAIT_LOCAL_STAT,

    ZBAL_PROF_ALLREDUCE_KERNEL_ALL,
    ZBAL_PROF_ALLREDUCE_SCATTER_REDUCE,
    ZBAL_PROF_ALLREDUCE_ALLGATHER,

    ZBAL_PROF_BROADCAST_KERNEL_ALL,
    ZBAL_PROF_BROADCAST_SCATTER,
    ZBAL_PROF_BROADCAST_ALLGATHER,

    ZBAL_PROF_SCATTER_KERNEL_ALL,

    ZBAL_PROF_SEND_KERNEL_ALL,
    ZBAL_PROF_RECV_KERNEL_ALL,

    ZBAL_PROF_EXCHANGE_ADDR,
    ZBAL_PROF_WAIT_FLAG,
    ZBAL_PROF_WRITE_STAT,
    ZBAL_PROF_WAIT_STAT,
    ZBAL_PROF_GM_2_UB,
    ZBAL_PROF_UB_2_GM,
    ZBAL_PROF_BUTT,
};

const std::vector<std::pair<std::string, bool>> g_profName = {
    {"UNKNOWN", false},
    {"LINE_NO", true},
    {"BARRIER", false},

    {"AG_KERNEL_ALL", true},
    {"AG_COPY", true},
    {"AG_LOCAL_COPY", true},
    {"AG_PREPARE_PTR", false},

    {"RS_KERNEL_ALL", false},
    {"RS_LOCAL_COPY", false},
    {"RS_COPY", false},

    {"A2A_KERNEL_ALL", true},
    {"A2A_COPY", true},
    {"A2A_PREPARE_PTR", true},
    {"A2A_CORE_RANGE", true},
    {"A2A_INIT_STAT", true},
    {"A2A_ATOMIC_INC", true},
    {"A2A_WAIT_LOCAL_STAT", true},

    {"AR_KERNEL_ALL", false},
    {"AR_REDUCE", false},
    {"AR_GATHER", false},

    {"BR_KERNEL_ALL", false},
    {"BR_SCATER_PART", false},
    {"BR_GATHER_PART", false},

    {"SC_KERNEL_ALL", false},

    {"SEND_KERNEL_ALL",           false},
    {"RECV_KERNEL_ALL",           false},

    {"EXCHANGE_ADDR", true},
    {"WAIT_FLAG", true},
    {"WRITE_STAT", true},
    {"WAIT_STAT", true},
    {"GM_2_UB", false},
    {"UB_2_GM", false},
};

struct RankCoreMapping {
    uint16_t groupSize;
    uint32_t blockDim;
    uint64_t start;
    uint64_t end;
};

/**
 * @brief group info of this communicator, this struct will be copy to device, keep it simple
 */
struct CommGroupInfo {
    uint16_t groupSize = 0;                                /* the ranks in the group */
    uint16_t myGroupRank = 0;                              /* rank id in the group */
    uintptr_t myMetaGva = 0;                               /* gva of mine */
    uintptr_t myParamDataGva = 0;                          /* gva of for param exchange of operation */
    uintptr_t myAddressExchangeGva = 0;                    /* gva of for address exchange of operation */
    uint64_t sizeForCommGroupInfo = 0;                     /* max memory size of passing param from host to device */
    uint64_t sizeForParam = 0;                             /* max memory size of exchange param */
    uint64_t sizeForExchangeAddress = 0;                   /* max memory size for exchange operation data addresses */
    uint64_t fftsConfig;                                   /* copy from CommGroupOptions.fftsConfig */
    uint16_t peerGroupRank2WorldRank[ZBAL_MAX_RANKS] = {}; /* rank id in group to world rank id relationship */
    uint64_t localDeviceMemSize;                           /* copy from CommGroupOptions.localDeviceMemSize */
    uint64_t waitSymbol;                                   /* wait symbol for op cross ranks */
    uintptr_t hostMemoryForProfiling;                      /* memory for perf at host side */
    uintptr_t devMemoryForProfiling;                       /* memory for perf at host side */
    uint64_t tracePointPerCore;                            /* tracing points per core */
    uint16_t groupIndex;                                   /* index of this group, start from 0 */
    uint32_t dataOpType;                                   /* data operation type, see zbal_data_op_type_t */
};

#endif // ZBAL_COMM_STRUCT_H
