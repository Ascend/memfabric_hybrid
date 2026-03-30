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

#ifndef SMEM_ETCD_CONFIG_STORE_BACKEND_H
#define SMEM_ETCD_CONFIG_STORE_BACKEND_H

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <atomic>

#include "smem_config_store_backend.h"

namespace ock {
namespace smem {

// Local memory implementation of ConfigStoreBackend.
class SmemEtcdStoreBackend final : public ConfigStoreBackend {
public:
    explicit SmemEtcdStoreBackend(std::string instanceId = "") noexcept;
    ~SmemEtcdStoreBackend() noexcept override;

    SmemEtcdStoreBackend(const SmemEtcdStoreBackend &) = delete;
    SmemEtcdStoreBackend &operator=(const SmemEtcdStoreBackend &) = delete;
    SmemEtcdStoreBackend(SmemEtcdStoreBackend &&) = delete;
    SmemEtcdStoreBackend &operator=(SmemEtcdStoreBackend &&) = delete;

    [[nodiscard]] StoreErrorCode Initialize(const std::string &backendUrl, const std::string &userName,
                                            const std::string &password) override;
    void UnInitialize() override;

    [[nodiscard]] std::string BackendName() const noexcept override;

    [[nodiscard]] StoreErrorCode PrefixGet(const std::string &key, PrefixGetMap &outValue) const noexcept override;

    [[nodiscard]] StoreErrorCode Get(const std::string &key, std::vector<uint8_t> &outValue) const noexcept override;

    [[nodiscard]] StoreErrorCode Put(const std::string &key, const std::vector<uint8_t> &value,
                                     int64_t ttlSeconds) noexcept override;

    [[nodiscard]] StoreErrorCode Delete(const std::string &key) noexcept override;

    [[nodiscard]] StoreErrorCode Exist(const std::string &key) const noexcept override;

    void Clear() noexcept override;

    [[nodiscard]] bool IsDistributed() const noexcept override;

    [[nodiscard]] bool SupportsTTL() const noexcept override;

    [[nodiscard]] StoreErrorCode AcquireDistributedLock(const std::string &name) noexcept override;

    [[nodiscard]] StoreErrorCode ReleaseDistributedLock(const std::string &name) noexcept override;

    [[nodiscard]] StoreErrorCode TryAcquireDistributedLock(const std::string &name,
                                                           int64_t timeoutMs) noexcept override;

private:
    mutable std::mutex mutex_;
    std::atomic_bool initialized_{false};
    const std::string clusterId_;
    const std::string clusterRoot_;
};
using SmemEtcdStoreBackendPtr = SmRef<SmemEtcdStoreBackend>;
} // namespace smem
} // namespace ock
#endif
