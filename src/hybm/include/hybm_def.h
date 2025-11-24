/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MEM_FABRIC_HYBRID_HYBRID_BIG_MEM_DL_H
#define MEM_FABRIC_HYBRID_HYBRID_BIG_MEM_DL_H

#include <stdint.h>
#include <stdbool.h>

#ifndef __cplusplus
extern "C" {
#endif

typedef void *hybm_entity_t;
typedef void *hybm_mem_slice_t;

#define HYBM_FREE_SINGLE_SLICE 0x00
#define HYBM_FREE_ALL_SLICE    0x01

#define HYBM_EXPORT_PARTIAL_SLICE 0x00
#define HYBM_EXPORT_ALL_SLICE     0x01

#define HYBM_IMPORT_WITH_ADDRESS 0x01U

#define HYBM_TLS_PATH_SIZE 256

/**
 * @brief Determine whether the IO initiator is on the host or the device.
 */
typedef enum {
    HYBM_TYPE_AI_CORE_INITIATE = 0, // AI core
    HYBM_TYPE_HOST_INITIATE,        // Host
    HYBM_TYPE_BUTT
} hybm_type;

/**
 * @brief data copy type
 */
typedef enum {
    HYBM_DOP_TYPE_DEFAULT = 0U,
    HYBM_DOP_TYPE_MTE = 1U << 0,
    HYBM_DOP_TYPE_SDMA = 1U << 1,
    HYBM_DOP_TYPE_DEVICE_RDMA = 1U << 2,
    HYBM_DOP_TYPE_HOST_RDMA = 1U << 3,
    HYBM_DOP_TYPE_HOST_TCP = 1U << 4,

    HYBM_DOP_TYPE_BUTT
} hybm_data_op_type;

typedef enum {
    HYBM_SCOPE_IN_NODE = 0U,
    HYBM_SCOPE_CROSS_NODE,

    HYBM_SCOPE_BUTT
} hybm_scope;

typedef enum {
    HYBM_MEM_TYPE_DEVICE = 1U << 0,
    HYBM_MEM_TYPE_HOST = 1U << 1,

    HYBM_MEM_TYPE_BUTT
} hybm_mem_type;

typedef enum {
    HYBM_ROLE_PEER = 0, // peer to peer connect
    HYBM_ROLE_SENDER,
    HYBM_ROLE_RECEIVER,
    HYBM_ROLE_BUTT
} hybm_role_type;

typedef struct {
    uint8_t desc[512L];
    uint32_t descLen;
} hybm_exchange_info;

typedef struct {
    bool tlsEnable;
    char caPath[HYBM_TLS_PATH_SIZE];
    char crlPath[HYBM_TLS_PATH_SIZE];
    char certPath[HYBM_TLS_PATH_SIZE];
    char keyPath[HYBM_TLS_PATH_SIZE];
    char keyPassPath[HYBM_TLS_PATH_SIZE];
    char packagePath[HYBM_TLS_PATH_SIZE];
    char decrypterLibPath[HYBM_TLS_PATH_SIZE];
} hybm_tls_config;

typedef struct {
    hybm_type bmType;
    hybm_mem_type memType;
    hybm_data_op_type bmDataOpType;
    hybm_scope bmScope;
    uint16_t rankCount;
    uint16_t rankId;
    uint16_t devId;
    uint64_t deviceVASpace;
    uint64_t hostVASpace;
    uint64_t preferredGVA;
    uint64_t singleRankVASpace;
    bool globalUniqueAddress; // 是否使用全局统一内存地址
    hybm_role_type role;
    char nic[64];
    hybm_tls_config tlsOption;
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
    void *src;
    uint64_t spitch;
    void *dest;
    uint64_t dpitch;
    uint64_t width;
    uint64_t height;
} hybm_copy_2d_params;

typedef struct {
    void *src;
    void *dest;
    uint64_t dataSize;
} hybm_copy_params;

typedef struct {
    void** sources;
    void** destinations;
    const uint64_t *dataSizes;
    uint32_t batchSize;
} hybm_batch_copy_params;

#ifndef __cplusplus
}
#endif

#endif // MEM_FABRIC_HYBRID_HYBRID_BIG_MEM_DL_H
