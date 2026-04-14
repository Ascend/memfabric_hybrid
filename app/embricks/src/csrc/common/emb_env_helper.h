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
#ifndef MEMFABRIC_HYBRID_EMB_ENV_HELPER_H
#define MEMFABRIC_HYBRID_EMB_ENV_HELPER_H

#include "emb_common_includes.h"

namespace ock {
namespace emb {

/* env variable for perf */
#define ENV_NAME_ENABLE_HUGE_TABLE                 "EMB_ENABLE_HUGE_PAGE"
#define ENV_NAME_OVERFLOW_BUCKET_MEM_START_ADDRESS "EMB_HASHMAP_OVERFLOW_BUCKET_MEM_POOL_ADDRESS_TB"
#define ENV_NAME_OVERFLOW_BUCKET_MEM_SIZE          "EMB_HASHMAP_OVERFLOW_BUCKET_MEM_POOL_SIZE_GB"

class EnvHelper {
public:
    static void Initialize() noexcept;
    static void DumpEnv() noexcept;

public:
    static bool gHugeTableEnabled;
    static uint32_t gHashmapOverflowBucketPoolStartAddrTB;
    static uint32_t gHashmapOverflowBucketPoolSizeGB;
};
} // namespace emb
} // namespace ock

#endif // MEMFABRIC_HYBRID_EMB_ENV_HELPER_H
