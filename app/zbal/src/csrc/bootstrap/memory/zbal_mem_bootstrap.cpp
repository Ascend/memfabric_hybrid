/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "zbal_mem_bootstrap.h"
#include "zbal_mem_mf_bootstrap.h"

namespace zbal {
namespace bootstrap {

MemBootstrapPtr MemBootstrap::Create(const MemBootstrapOptions &options)
{
    if (options.boostrapType == MemBoostrapType::MBT_MEMFABRIC) {
        auto bootstrap = ZMakeRef<MemFabricBoostrap>(options);
        if (bootstrap == nullptr) {
            ZBAL_LOG_ERROR("Create MemFabric bootstrap failed, probably out of memory");
            return nullptr;
        }
        return bootstrap.Get();
    }

    ZBAL_LOG_ERROR("Un-reachable path, probably the mem bootstrap type is not invalid");

    return nullptr;
}

ZResult MemBootstrap::VerifyOptions()
{
    ZBAL_VALIDATE_RETURN(options_.rankCount > 0, "invalid options, rankCount should > 0", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(options_.rankId < options_.rankCount, "invalid options, rankId should < rankCount",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(options_.deviceId < DEVICE_COUNT_MAX_LIMIT, "invalid options, deviceId should be less than 32",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(options_.totalMemSize < MEMORY_SIZE_CAP, "invalid options, memory size is too large",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(!options_.ipPort.empty(), "invalid options, ipPort is empty", Z_INVALID_PARAM);

    ZBAL_LOG_INFO("verified options, " << options_);

    return Z_OK;
}
} // namespace bootstrap
} // namespace zbal