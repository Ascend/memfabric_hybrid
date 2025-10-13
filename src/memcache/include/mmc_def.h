/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __MEMFABRIC_MMC_DEF_H__
#define __MEMFABRIC_MMC_DEF_H__

#include <stdint.h>
#include <string>
#include <vector>

#include "mf_tls_def.h"

#define DISCOVERY_URL_SIZE 1024
#define PATH_MAX_SIZE 1024
#define MAX_BATCH_OP_COUNT 16384

#ifdef __cplusplus
extern "C" {
#endif

typedef void *mmc_meta_service_t;
typedef void *mmc_local_service_t;
typedef void *mmc_client_t;
#ifndef MMC_OUT_LOGGER
typedef void (*ExternalLog)(int level, const char* msg);
#endif

typedef struct {
    char discoveryURL[DISCOVERY_URL_SIZE]; /* composed by schema and url, e.g. tcp:// or etcd:// or zk:// */
    char configStoreURL[DISCOVERY_URL_SIZE]; /* composed by schema and url, e.g. tcp:// or etcd:// or zk:// */
    bool haEnable;
    int32_t logLevel;
    char logPath[PATH_MAX_SIZE];
    int32_t logRotationFileSize;
    int32_t logRotationFileCount;
    uint16_t evictThresholdHigh;
    uint16_t evictThresholdLow;
    tls_config accTlsConfig;
    tls_config configStoreTlsConfig;
} mmc_meta_service_config_t;

typedef struct {
    char discoveryURL[DISCOVERY_URL_SIZE];
    uint32_t deviceId;
    uint32_t rankId;  // bmRankId: BM全局统一编号
    uint32_t worldSize;
    std::string bmIpPort;
    std::string bmHcomUrl;
    uint32_t createId;
    std::string dataOpType;
    uint64_t localDRAMSize;
    uint64_t localHBMSize;
    uint32_t flags;
    tls_config accTlsConfig;
    int32_t logLevel;
    ExternalLog logFunc;
    tls_config hcomTlsConfig;
    tls_config configStoreTlsConfig;
} mmc_local_service_config_t;

typedef struct {
    char discoveryURL[DISCOVERY_URL_SIZE];
    uint32_t rankId;
    uint32_t rpcRetryTimeOut;
    uint32_t timeOut;
    int32_t logLevel;
    ExternalLog logFunc;
    tls_config tlsConfig;
} mmc_client_config_t;

typedef struct {
    uint64_t addr;
    uint32_t type; // 0 dram, 1 hbm
    uint32_t dimType; // 0 oneDim, 1 twoDim
    union {
        struct {
            uint64_t dpitch : 48;
            uint64_t layerOffset : 16;
            uint32_t width;  // max 4GB
            uint16_t layerNum;
            uint16_t layerCount;
        } twoDim;
        struct {
            uint64_t offset;
            uint64_t len;
        } oneDim;
    };
} mmc_buffer;

enum affinity_policy : int {
    NATIVE_AFFINITY = 0,
};

#define MAX_BLOB_COPIES 8
typedef struct {
    uint16_t mediaType;
    affinity_policy policy;
    uint16_t replicaNum; // Less than or equal to MAX_BLOB_COPIES=8, Currently only supports a value of 1
    int32_t preferredLocalServiceIDs[MAX_BLOB_COPIES];
} mmc_put_options;

typedef struct {
    int32_t xx;
} mmc_location_t;

typedef struct {
    uint64_t size;
    uint16_t prot;
    uint8_t numBlobs;
    bool valid;
    uint32_t ranks[MAX_BLOB_COPIES];
    uint16_t types[MAX_BLOB_COPIES];
} mmc_data_info;

#ifdef __cplusplus
}
#endif

#endif  //__MEMFABRIC_MMC_DEF_H__