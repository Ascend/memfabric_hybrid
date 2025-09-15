/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBRID_BIG_MEM_DL_H
#define MEM_FABRIC_HYBRID_HYBRID_BIG_MEM_DL_H

#include <stdint.h>

#include "mf_tls_def.h"

#ifndef __cplusplus
extern "C" {
#endif

typedef void *hybm_entity_t;
typedef void *hybm_mem_slice_t;

#define HYBM_FREE_SINGLE_SLICE 0x00
#define HYBM_FREE_ALL_SLICE 0x01

#define HYBM_EXPORT_PARTIAL_SLICE 0x00
#define HYBM_EXPORT_ALL_SLICE 0x01

typedef enum { HYBM_TYPE_AI_CORE_INITIATE = 0, HYBM_TYPE_HOST_INITIATE, HYBM_TYPE_BUTT } hybm_type;

typedef enum {
    HYBM_DOP_TYPE_MTE = 1 << 0,
    HYBM_DOP_TYPE_SDMA = 1 << 1,
    HYBM_DOP_TYPE_DEVICE_RDMA = 1 << 2,
    HYBM_DOP_TYPE_HOST_RDMA = 1 << 3,
    HYBM_DOP_TYPE_HOST_TCP = 1 << 4,

    HYBM_DOP_TYPE_BUTT
} hybm_data_op_type;

typedef enum {
    HYBM_SCOPE_IN_NODE = 0,
    HYBM_SCOPE_CROSS_NODE,

    HYBM_SCOPE_BUTT
} hybm_scope;

typedef enum {
    HYBM_MEM_TYPE_DEVICE = 1 << 0,
    HYBM_MEM_TYPE_HOST = 1 << 1,

    HYBM_MEM_TYPE_BUTT
} hybm_mem_type;

typedef enum {
    HYBM_ROLE_PEER = 0,  // peer to peer connect
    HYBM_ROLE_SENDER,
    HYBM_ROLE_RECEIVER,
    HYBM_ROLE_BUTT
} hybm_role_type;

typedef struct {
    uint8_t desc[512L];
    uint32_t descLen;
} hybm_exchange_info;

typedef struct {
    hybm_type bmType;
    hybm_mem_type memType;
    hybm_data_op_type bmDataOpType;
    hybm_scope bmScope;
    uint16_t rankCount;
    uint16_t rankId;
    uint16_t devId;
    uint64_t singleRankVASpace;
    uint64_t deviceVASpace;
    uint64_t hostVASpace;
    uint64_t preferredGVA;
    hybm_role_type role;
    char nic[64];
    tls_config tlsOption;
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

typedef struct {
    const void** sources;
    void** destinations;
    const uint32_t *dataSizes;
    uint32_t batchSize;
} hybm_batch_copy_params;

#ifndef __cplusplus
}
#endif

#endif  // MEM_FABRIC_HYBRID_HYBRID_BIG_MEM_DL_H
