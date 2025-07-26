/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_CONFIG_CONST_H
#define MEM_FABRIC_MMC_CONFIG_CONST_H

#include <utility>

namespace ock {
namespace mmc {
namespace ConfConstant {
// add configuration here with default values
constexpr auto OCK_MMC_META_SERVICE_URL = std::make_pair("ock.mmc.meta_service_url", "tcp://127.0.0.1:5000");

constexpr auto OCK_MMC_TLS_ENABLE = std::make_pair("ock.mmc.tls.enable", true);
constexpr auto OCK_MMC_TLS_TOP_PATH = std::make_pair("ock.mmc.tls.top.path", "");
constexpr auto OCK_MMC_TLS_CA_PATH = std::make_pair("ock.mmc.tls.ca.path", "");
constexpr auto OCK_MMC_TLS_CERT_PATH = std::make_pair("ock.mmc.tls.cert.path", "");
constexpr auto OCK_MMC_TLS_KEY_PATH = std::make_pair("ock.mmc.tls.key.path", "");
constexpr auto OCK_MMC_TLS_PACKAGE_PATH = std::make_pair("ock.mmc.tls.package.path", "");

constexpr auto OKC_MMC_LOCAL_SERVICE_DEVICE_ID = std::make_pair("ock.mmc.local_service.device_id", 0);
constexpr auto OKC_MMC_LOCAL_SERVICE_RANK_ID = std::make_pair("ock.mmc.local_service.bm_rank_id", 0);
constexpr auto OKC_MMC_LOCAL_SERVICE_WORLD_SIZE = std::make_pair("ock.mmc.local_service.world_size", 1);
constexpr auto OKC_MMC_LOCAL_SERVICE_BM_IP_PORT = std::make_pair("ock.mmc.local_service.config_store_url", "tcp://127.0.0.1:6000");
constexpr auto OKC_MMC_LOCAL_SERVICE_BM_HCOM_URL = std::make_pair("ock.mmc.local_service.hcom_url", "tcp://127.0.0.1:7000");
constexpr auto OKC_MMC_LOCAL_SERVICE_AUTO_RANKING = std::make_pair("ock.mmc.local_service.auto_ranking", 0);
constexpr auto OKC_MMC_LOCAL_SERVICE_BM_ID = std::make_pair("ock.mmc.local_service.bm_id", 0);
constexpr auto OKC_MMC_LOCAL_SERVICE_PROTOCOL = std::make_pair("ock.mmc.local_service.protocol", "sdma");
constexpr auto OKC_MMC_LOCAL_SERVICE_DRAM_SIZE = std::make_pair("ock.mmc.local_service.dram.size", 2097152ULL);
constexpr auto OKC_MMC_LOCAL_SERVICE_HBM_SIZE = std::make_pair("ock.mmc.local_service.hbm.size", 2097152ULL);

constexpr auto OCK_MMC_CLIENT_TIMEOUT_SECONDS = std::make_pair("ock.mmc.client.timeout.seconds", 60);

}
}  // namespace mmc
}  // namespace ock

#endif