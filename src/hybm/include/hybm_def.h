/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBRID_BIG_MEM_DL_H
#define MEM_FABRIC_HYBRID_HYBRID_BIG_MEM_DL_H

#include <stdint.h>

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
    HYBM_TYPE_HBM_AI_CORE_INITIATE = 0,
    HYBM_TYPE_HBM_HOST_INITIATE,
    HYBM_TYPE_HBM_DRAM_HOST_INITIATE,

    HYBM_TYPE_BUTT
} hybm_type;

typedef enum {
    HYBM_DOP_TYPE_MTE = 0,
    HYBM_DOP_TYPE_ROCE,
    HYBM_DOP_TYPE_SDMA,

    HYBM_DOP_TYPE_BUTT
} hybm_data_op_type;

typedef enum {
    HYBM_SCOPE_IN_NODE = 0,
    HYBM_SCOPE_CROSS_NODE,

    HYBM_SCOPE_BUTT
} hybm_scope;

typedef enum {
    HYBM_RANK_TYPE_STATIC = 0,

    HYBM_RANK_TYPE_BUTT
} hybm_rank_type;

typedef enum {
    HYBM_MEM_TYPE_DEVICE = 0,
    HYBM_MEM_TYPE_HOST,

    HYBM_MEM_TYPE_BUTT
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
    uint64_t singleRankVASpace;
    uint64_t preferredGVA;
} hybm_options;

typedef enum {
    HYBM_LOCAL_HOST_TO_GLOBAL_HOST = 0,
    HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE = 1,

    HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST = 2,
    HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE = 3,

    HYBM_GLOBAL_HOST_TO_GLOBAL_HOST = 4,
    HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE = 5,
    HYBM_GLOBAL_HOST_TO_LOCAL_HOST = 6,
    HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE = 7,

    HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST = 8,
    HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE = 9,
    HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST = 10,
    HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE = 11,

    HYBM_DATA_COPY_DIRECTION_BUTT
} hybm_data_copy_direction;

#ifndef __cplusplus
}
#endif

#endif // MEM_FABRIC_HYBRID_HYBRID_BIG_MEM_DL_H
