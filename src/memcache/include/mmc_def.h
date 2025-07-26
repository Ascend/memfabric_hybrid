/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_MMC_DEF_H__
#define __MEMFABRIC_MMC_DEF_H__

#include <stdint.h>
#include <string>
#include <vector>

#define DISCOVERY_URL_SIZE 1024
#define PATH_MAX_SIZE 1024

#ifdef __cplusplus
extern "C" {
#endif

typedef void *mmc_meta_service_t;
typedef void *mmc_local_service_t;
typedef void *mmc_client_t;

typedef struct {
    bool tlsEnable;
    char tlsTopPath[PATH_MAX_SIZE];
    char tlsCaPath[PATH_MAX_SIZE];
    char tlsCertPath[PATH_MAX_SIZE];
    char tlsKeyPath[PATH_MAX_SIZE];
    char packagePath[PATH_MAX_SIZE];
} mmc_tls_config;

typedef struct {
    char discoveryURL[DISCOVERY_URL_SIZE]; /* composed by schema and url, e.g. tcp:// or etcd:// or zk:// */
    mmc_tls_config tlsConfig;
} mmc_meta_service_config_t;

typedef struct {
    char discoveryURL[DISCOVERY_URL_SIZE];
    uint32_t deviceId;
    uint32_t rankId;  // bmRankId: BM全局统一编号
    uint32_t worldSize;
    std::string bmIpPort;
    std::string bmHcomUrl;
    int autoRanking;
    uint32_t createId;
    std::string dataOpType;
    uint64_t localDRAMSize;
    uint64_t localHBMSize;
    uint32_t flags;
    mmc_tls_config tlsConfig;
} mmc_local_service_config_t;

typedef struct {
    char discoveryURL[DISCOVERY_URL_SIZE];
    uint32_t rankId;
    uint32_t timeOut;
    int autoRanking;
    mmc_tls_config tlsConfig;
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

typedef struct {
    uint16_t mediaType;
    affinity_policy policy;
} mmc_put_options;

typedef struct {
    int32_t xx;  // TODO
} mmc_location_t;

typedef struct {
    uint32_t size;
    uint16_t prot;
    uint8_t numBlobs;
    bool valid;
} mmc_data_info;

#ifdef __cplusplus
}
#endif

#endif  //__MEMFABRIC_MMC_DEF_H__
