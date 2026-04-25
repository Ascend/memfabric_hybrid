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

#ifndef MEMFABRIC_HYBRID_SMEM_ETCD_BACKEND_UTILS_H
#define MEMFABRIC_HYBRID_SMEM_ETCD_BACKEND_UTILS_H
#include "smem_config_store_logger.h"
#include "smem_config_store_backend.h"
namespace ock {
namespace smem {
namespace etcd_utils {

inline bool PrefixGetFill(const char *key, const void *value, uint64_t size, void *context)
{
    if (key == nullptr) {
        STORE_LOG_ERROR("[ETCD] failed to do prefix get, key is null");
        return false;
    }
    if (value == nullptr) {
        STORE_LOG_ERROR("[ETCD] failed to do prefix get, value is null");
        return false;
    }
    if (size == 0) {
        STORE_LOG_ERROR("[ETCD] failed to do prefix get, size is 0");
        return false;
    }

    auto *prefixGetMap = static_cast<PrefixGetMap *>(context);
    if (prefixGetMap == nullptr) {
        return false;
    }
    std::vector<uint8_t> outValue(static_cast<const uint8_t *>(value), static_cast<const uint8_t *>(value) + size);
    prefixGetMap->emplace(key, outValue);
    return true;
}

} // namespace etcd_utils
} // namespace smem
} // namespace ock
#endif // MEMFABRIC_HYBRID_SMEM_ETCD_BACKEND_UTILS_H
