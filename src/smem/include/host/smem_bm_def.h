/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_BM_DEF_H__
#define __MEMFABRIC_SMEM_BM_DEF_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *smem_bm_t;

/**
 * @brief CPU initiated data operation type, currently only support MTE
 */
typedef enum {
    SMEMB_DATA_OP_SDMA = 1U << 0,
    SMEMB_DATA_OP_ROCE = 1U << 1,
} smem_bm_data_op_type;

/**
 * @brief Memory type
 */
typedef enum {
    SMEMB_MEM_TYPE_HBM = 1U << 0,
    SMEMB_MEM_TYPE_DRAM = 1U << 1,
} smem_bm_mem_type;

/**
* @brief Data copy direction
*/
typedef enum {
    SMEMB_COPY_L2G = 0,
    SMEMB_COPY_G2L = 1,
    SMEMB_COPY_G2H = 2,
    SMEMB_COPY_H2G = 3,
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
    bool unifiedAddressSpace;         /* unified adderss with SVM */
    uint32_t flags;                   /* other flag, default 0 */
} smem_bm_config_t;

#ifdef __cplusplus
}
#endif

#endif  //__MEMFABRIC_SMEM_BM_DEF_H__
