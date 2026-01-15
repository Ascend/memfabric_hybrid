/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#ifndef __MEMFABRIC_SMEM_BM_DEF_H__
#define __MEMFABRIC_SMEM_BM_DEF_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *smem_bm_t;
#define SMEM_BM_TIMEOUT_MAX                 UINT32_MAX /* all timeout must <= UINT32_MAX */
#define ASYNC_COPY_FLAG                     (1UL << (0))
#define SMEM_BM_PERFORMANCE_MODE_FLAG_INDEX 7
#define SMEM_BM_PERFORMANCE_MODE_FLAG_LEN   1
#define SMEM_BM_BIND_NUMA_FLAG_INDEX        0
#define SMEM_BM_BIND_NUMA_FLAG_LEN          7
#define SMEM_TLS_PATH_SIZE                  256
/**
* @brief Smem memory type
*/
typedef enum {
    SMEM_MEM_TYPE_LOCAL_DEVICE = 0, /* memory on local device */
    SMEM_MEM_TYPE_LOCAL_HOST,       /* memory on local host */
    SMEM_MEM_TYPE_DEVICE,           /* memory on global device */
    SMEM_MEM_TYPE_HOST,             /* memory on global host */

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
    SMEMB_DATA_OP_HOST_URMA = 1U << 4,
    SMEMB_DATA_OP_BUTT
} smem_bm_data_op_type;

/**
* @brief Data copy direction
*/
typedef enum {
    SMEMB_COPY_L2G = 0,  /* copy data from local hbm to global space */
    SMEMB_COPY_G2L = 1,  /* copy data from global space to local hbm */
    SMEMB_COPY_G2H = 2,  /* copy data from global space to local host dram */
    SMEMB_COPY_H2G = 3,  /* copy data from local host dram to global space */
    SMEMB_COPY_L2GH = 4, /* copy data from local space to global host space */
    SMEMB_COPY_GH2L = 5, /* copy data from global host space to local space */
    SMEMB_COPY_GH2H = 6, /* copy data from global host space to host memory */
    SMEMB_COPY_H2GH = 7, /* copy data from host memory to global host space */
    SMEMB_COPY_G2G = 8,  /* copy data from global space to global space */
    /* add here */
    SMEMB_COPY_BUTT
} smem_bm_copy_type;

typedef struct {
    bool tlsEnable;
    char caPath[SMEM_TLS_PATH_SIZE];
    char crlPath[SMEM_TLS_PATH_SIZE];
    char certPath[SMEM_TLS_PATH_SIZE];
    char keyPath[SMEM_TLS_PATH_SIZE];
    char keyPassPath[SMEM_TLS_PATH_SIZE];
    char packagePath[SMEM_TLS_PATH_SIZE];
    char decrypterLibPath[SMEM_TLS_PATH_SIZE];
} smem_tls_config;

typedef struct {
    uint32_t initTimeout;             /* func smem_bm_init timeout, default 120s (min=1, max=SMEM_BM_TIMEOUT_MAX) */
    uint32_t createTimeout;           /* func smem_bm_create timeout, default 120s (min=1, max=SMEM_BM_TIMEOUT_MAX) */
    uint32_t controlOperationTimeout; /* control operation timeout, default 120s (min=1, max=SMEM_BM_TIMEOUT_MAX) */
    bool startConfigStoreServer;      /* whether to start config store, default true */
    bool startConfigStoreOnly;        /* only start the config store */
    bool dynamicWorldSize;            /* member cannot join dynamically */
    bool unifiedAddressSpace;         /* unified address with SVM */
    bool autoRanking;                 /* automatically allocate rank IDs, default is false. */
    uint16_t rankId;                  /* user specified rank ID, valid for autoRanking is False */
    uint32_t flags;                   /* other flag, default 0 */
    char hcomUrl[64];
    smem_tls_config hcomTlsConfig;
    smem_tls_config storeTlsConfig;
} smem_bm_config_t;

typedef struct {
    const void *src;
    void *dest;
    size_t dataSize;
} smem_copy_params;

typedef struct {
    void **sources;
    void **destinations;
    const uint64_t *dataSizes;
    uint32_t batchSize;
} smem_batch_copy_params;

#ifdef __cplusplus
}
#endif

#endif //__MEMFABRIC_SMEM_BM_DEF_H__