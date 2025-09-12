/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_BM_DEF_H__
#define __MEMFABRIC_SMEM_BM_DEF_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *smem_bm_t;

#define ASYNC_COPY_FLAG (1UL << (0))
/**
* @brief Smem memory type
*/
typedef enum {
    SMEM_MEM_TYPE_LOCAL_DEVICE = 0,    /* memory on local device */
    SMEM_MEM_TYPE_LOCAL_HOST,          /* memory on local host */
    SMEM_MEM_TYPE_DEVICE,              /* memory on global device */
    SMEM_MEM_TYPE_HOST,                /* memory on global host */

    SMEM_MEM_TYPE_BUTT
} smem_bm_mem_type;
/**
 * @brief CPU initiated data operation type, currently only support SDMA
 */
typedef enum {
    SMEMB_DATA_OP_SDMA = 1U << 0,
    SMEMB_DATA_OP_HOST_RDMA = 1U << 1,
    SMEMB_DATA_OP_HOST_TCP = 1U << 2,
    SMEMB_DATA_OP_DEVICE_RDMA = 1U << 3,
    SMEMB_DATA_OP_BUTT
} smem_bm_data_op_type;

/**
* @brief Data copy direction
*/
typedef enum {
    SMEMB_COPY_L2G = 0,              /* copy data from local space to global space */
    SMEMB_COPY_G2L = 1,              /* copy data from global space to local space */
    SMEMB_COPY_G2H = 2,              /* copy data from global space to host memory */
    SMEMB_COPY_H2G = 3,              /* copy data from host memory to global space */
    SMEMB_COPY_G2G = 4,               /* copy data from global space to global space */
    /* add here */
    SMEMB_COPY_BUTT
} smem_bm_copy_type;

#define SMEM_BM_INIT_GVM_FLAG (1ULL << 1ULL) // Init the GVM module, enable to use Host DRAM

typedef struct {
    uint32_t initTimeout;             /* func smem_bm_init timeout, default 120 second */
    uint32_t createTimeout;           /* func smem_bm_create timeout, default 120 second */
    uint32_t controlOperationTimeout; /* control operation timeout, default 120 second */
    bool startConfigStore;            /* whether to start config store, default true */
    bool startConfigStoreOnly;        /* only start the config store */
    bool dynamicWorldSize;            /* member cannot join dynamically */
    bool unifiedAddressSpace;         /* unified address with SVM */
    bool autoRanking;                 /* automatically allocate rank IDs, default is false. */
    uint16_t rankId;                  /* user specified rank ID, valid for autoRanking is False */
    uint32_t flags;                   /* other flag, default 0 */
    char hcomUrl[64];
} smem_bm_config_t;

typedef struct {
    const void** sources;
    void** destinations;
    const uint32_t *dataSizes;
    uint32_t batchSize;
} smem_batch_copy_params;

#ifdef __cplusplus
}
#endif

#endif  //__MEMFABRIC_SMEM_BM_DEF_H__
