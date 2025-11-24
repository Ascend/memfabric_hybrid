/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc_env.h"

#include <string>

#include "mmc_functions.h"

namespace ock {
namespace mmc {
std::string MMC_META_CONF_PATH = SafeGetEnv("MMC_META_CONFIG_PATH");
std::string MMC_LOCAL_CONF_PATH = SafeGetEnv("MMC_LOCAL_CONFIG_PATH");
std::string META_POD_NAME = SafeGetEnv("META_POD_NAME");
std::string META_NAMESPACE = SafeGetEnv("META_NAMESPACE");
std::string META_LEASE_NAME = SafeGetEnv("META_LEASE_NAME");
} // namespace mmc
} // namespace ock