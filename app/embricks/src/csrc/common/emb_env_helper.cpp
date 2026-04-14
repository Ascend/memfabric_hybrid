/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Embricks is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "emb_env_helper.h"

namespace ock {
namespace emb {
bool EnvHelper::gHugeTableEnabled = false;
uint32_t EnvHelper::gHashmapOverflowBucketPoolStartAddrTB = 6; /* 6TB */
uint32_t EnvHelper::gHashmapOverflowBucketPoolSizeGB = 1;      /* 1GB */

void EnvHelper::Initialize() noexcept
{
    gHugeTableEnabled = (Func::GetEnv<uint32_t>(ENV_NAME_ENABLE_HUGE_TABLE, 0) == 1);
    gHashmapOverflowBucketPoolStartAddrTB = Func::GetEnv<uint32_t>(ENV_NAME_OVERFLOW_BUCKET_MEM_START_ADDRESS, 6);
    gHashmapOverflowBucketPoolSizeGB = Func::GetEnv<uint32_t>(ENV_NAME_OVERFLOW_BUCKET_MEM_SIZE, 1);
}

void EnvHelper::DumpEnv() noexcept
{
    EM_LOG_DEBUG(ENV_NAME_ENABLE_HUGE_TABLE << " = " << gHugeTableEnabled);
    EM_LOG_DEBUG(ENV_NAME_OVERFLOW_BUCKET_MEM_START_ADDRESS << " = " << gHashmapOverflowBucketPoolStartAddrTB);
    EM_LOG_DEBUG(ENV_NAME_OVERFLOW_BUCKET_MEM_SIZE << " = " << gHashmapOverflowBucketPoolSizeGB);
}
} // namespace emb
} // namespace ock