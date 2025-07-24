/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_BM_DEF_H__
#define __MEMFABRIC_SMEM_BM_DEF_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *smem_bm_t;

/**
 * @brief CPU initiated data operation type, currently only support SDMA
 */
typedef enum {
    SMEMB_DATA_OP_SDMA = 1U << 0,
    SMEMB_DATA_OP_ROCE = 1U << 1,
} smem_bm_data_op_type;

/**
* @brief Data copy direction
*/
typedef enum {
    SMEMB_COPY_L2G = 0,              /* copy data from local space to global space */
    SMEMB_COPY_G2L = 1,              /* copy data from global space to local space */
    SMEMB_COPY_G2H = 2,              /* copy data from global space to host memory */
    SMEMB_COPY_H2G = 3,              /* copy data from host memory to global space */

    SMEMB_COPY_L2GH = 4,              /* copy data from local space to global host space */
    SMEMB_COPY_GH2L = 5,              /* copy data from global host space to local space */
    SMEMB_COPY_GH2H = 6,              /* copy data from global host space to host memory */
    SMEMB_COPY_H2GH = 7,              /* copy data from host memory to global host space */
    /* add here */
    SMEMB_COPY_BUTT
} smem_bm_copy_type;

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

#ifdef __cplusplus
}
#endif

#endif  //__MEMFABRIC_SMEM_BM_DEF_H__
