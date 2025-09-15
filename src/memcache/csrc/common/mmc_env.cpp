/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc_env.h"
#include <cstdlib>

namespace ock {
namespace mmc {
const char* MMC_META_CONF_PATH = std::getenv("MMC_META_CONFIG_PATH");
const char* MMC_LOCAL_CONF_PATH = std::getenv("MMC_LOCAL_CONFIG_PATH");
const char* META_POD_NAME = std::getenv("META_POD_NAME");
const char* META_NAMESPACE = std::getenv("META_NAMESPACE");
const char* META_LEASE_NAME = std::getenv("META_LEASE_NAME");
}
}
