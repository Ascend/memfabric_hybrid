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
#ifndef ZBAL_DEF_H_
#define ZBAL_DEF_H_

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZBAL_MAX_RANKS 1024
#define ZBAL_COMM_NAME_MAX 128
#define ZBAL_MAX_IPPORT_LEN 64

typedef void *zbal_comm_t;

typedef void *aclrtStream;

typedef enum {
    ZBAL_DATA_TYPE_INT8 = 0,   /**< int8 */
    ZBAL_DATA_TYPE_INT16 = 1,  /**< int16 */
    ZBAL_DATA_TYPE_INT32 = 2,  /**< int32 */
    ZBAL_DATA_TYPE_FP16 = 3,   /**< fp16 */
    ZBAL_DATA_TYPE_FP32 = 4,   /**< fp32 */
    ZBAL_DATA_TYPE_INT64 = 5,  /**< int64 */
    ZBAL_DATA_TYPE_UINT64 = 6, /**< uint64 */
    ZBAL_DATA_TYPE_UINT8 = 7,  /**< uint8 */
    ZBAL_DATA_TYPE_UINT16 = 8, /**< uint16 */
    ZBAL_DATA_TYPE_UINT32 = 9, /**< uint32 */
    ZBAL_DATA_TYPE_FP64 = 10,  /**< fp64 */
    ZBAL_DATA_TYPE_BFP16 = 11, /**< bfp16 */

    ZBAL_DATA_TYPE_BUTT /* reserved */
} zbal_datatype_t;      /* reference to HcclDataType */

typedef enum {
    ZBAL_REDUCE_SUM = 0,  /**< sum */
    ZBAL_REDUCE_PROD = 1, /**< prod */
    ZBAL_REDUCE_MAX = 2,  /**< max */
    ZBAL_REDUCE_MIN = 3,  /**< min */

    ZBAL_REDUCE_BUTT /*  reserved */
} zbal_reduce_op_t;  /* reference to HcclReduceOp */

typedef enum {
    ZBAL_ASCEND_NPU = 0,

    ZBAL_BACK_BUTT
} zbal_backend_t;

typedef enum {
    BOOT_BY_MEMFABRIC = 0,

    BOOT_BY_BUTT
} zbal_bootstrap_type_t;

typedef struct {
    uint32_t flags;                    /* optional, flags */
    zbal_bootstrap_type_t btType;      /* bootstrap type */
    char ipPort[ZBAL_MAX_IPPORT_LEN];  /* tcp://127.0.0.1:9897 */
    uint16_t worldSize;                /* how many rank in total */
    uint16_t rankId;                   /* my rank id in the world */
    uint16_t deviceId;                 /* device id */
    uint16_t startConfigServer;        /* optional, if start config store server, 1 means start, 0 means not start */
    uint64_t deviceMemorySize;         /* memory size can be allocated */
    uint32_t dataOperationType;        /* optional, data operation type */
    uint16_t commMetaSpaceSize;        /* optional, in KB, default 1MB, min: 512KB, max: 4MB */
    uint16_t commGroupCap;             /* optional, max count of comm Group, default 128, min: 1, max: 256 */
} zbal_bootstrap_options_t;

typedef struct {
    void *deviceGva;                       /* gva of the world */
    uint64_t createdDeviceMemorySpaceSize; /* space size for symmetric memory */
    uint64_t allocatedDeviceMemorySize;    /* actually allocated memory size */
    void *myDeviceGva;                     /* gva of this rank */
    void *myCommMetaDeviceGva;             /* gva of comm meta of this rank */
    uint64_t metaSizeOfDevice;             /* size of device memory for SMA */
    void *mySMAGva;                        /* gva of sma of this rank */
    uint64_t smaSizeOfDevice;              /* size of device memory for SMA */
} zbal_bootstrap_output_t;

typedef struct {
    void *gva;     /* gva of the world */
    void *myGva;   /* gva of this rank */
    uint64_t size; /* device memory size */
} zbal_allocator_options_t;

typedef struct {
    char *name;                  /* name of the comm object */
    zbal_backend_t backendType;  /* backend type */
    uint32_t flags;              /* optional flags */
    uint16_t isWorldGroup;       /* if this is the world group, 1 means true, 0 means false */
    uint16_t groupSize;          /* how many rank in total */
    uint16_t groupRankId;        /* my rank id in the group */
    uint16_t symmetricMetaGva;   /* use symmetric memory for meta */
} zbal_comm_options_t;

typedef struct {
    char name[ZBAL_COMM_NAME_MAX];         /* name of the comm object */
    zbal_backend_t backendType;            /* backend type */
    uint32_t flags;                        /* optional flags */
    uint16_t isWorldGroup;                 /* if this is the world group, 1 means true, 0 means false */
    uint16_t groupSize;                    /* how many rank in total */
    uint16_t groupRankId;                  /* my rank id in the world */
    uint16_t symmetricMetaGva;             /* use symmetric memory for meta */
    void *myGVA;                           /* gva of this rank in world */
    void *myMetaGVA;                       /* gva of this rank in group */
    void *myMetaGVAForOpParam;             /* gva for operation param on device in meta area */
    void *myMetaGVAForOpExchange;          /* gva for address exchange in meta area */
    void *hostMemoryForProfiling;          /* host mem of this rank for profiling data */
    void *devMemoryForProfiling;           /* mapped device mem of this rank for profiling data */
    uint64_t sizeOfMetaArea;               /* device memory size of meta area */
    uint64_t sizeOfMetaForOpParam;         /* device memory size of operation param in meta area */
    uint64_t sizeOfMetaForAddressExchange; /* device memory size of address exchange area in meta */
    uint64_t tracePointPerCore;            /* trace point number per core */
    uint64_t localDeviceMemSize;           /* device memory size of this rank */
    uint32_t groupIndex;                   /* group index */
} zbal_comm_property_t;

/**
 * Make sure the size of this struct is 64 bytes, which fit to one cacheline to cpu
 */
typedef struct {
    void *data;                /* base pointer of tensor data, default value is null */
    zbal_datatype_t dataType;  /* data type of tensor */
    uint16_t dim;              /* dimension of the shape, default value is 0 */
    uint16_t shape[25];        /* shape, default value is 0 */
} zbal_tensor_info_t;

typedef enum {
    NO_QUANT = 0,

    QUANT_BF16_2_INT8 = 1, /* from bf16 to int8 */

    QUANT_BUTT
} zbal_quant_mode_t;

#ifdef __cplusplus
}
#endif

#endif  // ZBAL_DEF_H_
