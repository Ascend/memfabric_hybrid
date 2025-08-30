/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Create Date : 2025
 */
#ifndef PYTRANSFER_UITL_H
#define PYTRANSFER_UITL_H

#include <cstdint>

namespace ock {
namespace adapter {

#define STR(x) #x
#define STR2(x) STR(x)

uint16_t findAvailableTcpPort();

int32_t pytransfer_create_config_store(const char *storeUrl);

int32_t pytransfer_set_log_level(int level);

int32_t pytransfer_set_conf_store_tls(bool enable, std::string &tls_info);

}  // namespace adapter
}  // namespace ock
#endif // PYTRANSFER_UITL_H
