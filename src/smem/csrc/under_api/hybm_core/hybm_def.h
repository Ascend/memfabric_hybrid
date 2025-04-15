/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBRID_BIG_MEM_DL_H
#define MEM_FABRIC_HYBRID_HYBRID_BIG_MEM_DL_H

#include <cstdint>

#ifndef __cplusplus
extern "C" {
#endif

typedef void *hybm_entity_t;
typedef void *hybm_mem_slice_t;

#define HYBM_FREE_SINGLE_SLICE 0x00
#define HYBM_FREE_ALL_SLICE 0x01

#define HYBM_EXPORT_PARTIAL_SLICE 0x00
#define HYBM_EXPORT_ALL_SLICE 0x01

typedef enum {
    HyBM_TYPE_HBM_AI_CORE_INITIATE = 0,
    HyBM_TYPE_HBM_HOST_INITIATE,
    HyBM_TYPE_HBM_DRAM_HOST_INITIATE,

    HyBM_TYPE_BUTT
} hybm_type;

typedef enum {
    HyBM_DOP_TYPE_MTE = 0,
    HyBM_DOP_TYPE_ROCE,
    HyBM_DOP_TYPE_SDMA,

    HyBM_DOP_TYPE_BUTT
} hybm_data_op_type;

typedef enum {
    HyBM_SCOPE_IN_NODE = 0,
    HyBM_SCOPE_CROSS_NODE,

    HyBM_SCOPE_BUTT
} hybm_scope;

typedef enum {
    HyBM_RANK_TYPE_STATIC = 0,

    HyBM_RANK_TYPE_BUTT
} hybm_rank_type;

typedef enum {
    HyBM_MEM_TYPE_DEVICE = 0,
    HyBM_MEM_TYPE_HOST,

    HyBM_MEM_TYPE_BUTT
} hybm_mem_type;

typedef struct {
    uint8_t desc[512L];
    uint32_t descLen;
} hybm_exchange_info;

typedef struct {
    hybm_type bmType;
    hybm_data_op_type bmDataOpType;
    hybm_scope bmScope;
    hybm_rank_type bmRankType;
    uint16_t rankCount;
    uint16_t rankId;
    uint16_t devId;
    uint64_t singleRankVASpace;
    uint64_t preferredGVA;
} hybm_options;

typedef enum {
    HyBM_LOCAL_TO_SHARED = 0,
    HyBM_SHARED_TO_LOCAL,
    HyBM_SHARED_TO_SHARED,

    HyBM_DATA_COPY_DIRECTION_BUTT
} hybm_data_copy_direction;

#ifndef __cplusplus
}
#endif

namespace ock {
namespace mf {}
}

#endif // MEM_FABRIC_HYBRID_HYBRID_BIG_MEM_DL_H
