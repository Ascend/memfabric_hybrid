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

#ifndef SRC_SMEM_CSRC_CONFIG_STORE_BACKEND_SMEM_EXTERNAL_BACKEND_H_
#define SRC_SMEM_CSRC_CONFIG_STORE_BACKEND_SMEM_EXTERNAL_BACKEND_H_

#include <mutex>
#include <string>

#include "smem_config_store_backend.h"
#include "smem_def.h"

namespace ock {
namespace smem {

class SmemExternalBackend final : public ConfigStoreBackend {
public:
    explicit SmemExternalBackend(const std::string &instanceId = "") noexcept;
    ~SmemExternalBackend() noexcept override;

    SmemExternalBackend(const SmemExternalBackend &) = delete;
    SmemExternalBackend &operator=(const SmemExternalBackend &) = delete;
    SmemExternalBackend(SmemExternalBackend &&) = delete;
    SmemExternalBackend &operator=(SmemExternalBackend &&) = delete;

    [[nodiscard]] StoreErrorCode Initialize(const std::string &backendUrl, const std::string &userName,
                                            const std::string &password) override;
    void UnInitialize() override;

    [[nodiscard]] std::string BackendName() const noexcept override;
    [[nodiscard]] StoreErrorCode Get(const std::string &key, std::vector<uint8_t> &outValue) const noexcept override;
    [[nodiscard]] StoreErrorCode PrefixGet(const std::string &key, PrefixGetMap &outValue) const noexcept override;
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
    [[nodiscard]] StoreErrorCode GetLocked(const std::string &key, std::vector<uint8_t> &outValue) const noexcept;
    [[nodiscard]] StoreErrorCode MapCommonResult(int32_t result) const noexcept;

private:
    mutable std::mutex mutex_;
    bool initialized_ = false;
    smem_conf_store_backend_op_t backendOp_{};
    void *handle_ = nullptr;
    const std::string clusterRoot_;
};

using SmemExternalBackendPtr = SmRef<SmemExternalBackend>;

} // namespace smem
} // namespace ock

#endif // SRC_SMEM_CSRC_CONFIG_STORE_BACKEND_SMEM_EXTERNAL_BACKEND_H_
