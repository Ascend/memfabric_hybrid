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

#ifndef SMEM_SMEM_HA_CONFIG_STORE_H
#define SMEM_SMEM_HA_CONFIG_STORE_H

#include <shared_mutex>

#include "smem_config_store.h"
#include "smem_tcp_config_store.h"
#include "smem_ref.h"

namespace ock {
namespace smem {

class HaConfigStore : public ConfigStoreManager {
public:
    explicit HaConfigStore(TcpConfigStorePtr clientDelegate) noexcept;
    ~HaConfigStore() noexcept override;

    Result Startup(const smem_tls_config &tlsConfig, int reconnectRetryTimes = -1) noexcept;
    Result ClientStart(const smem_tls_config &tlsConfig, int reconnectRetryTimes = -1) noexcept;
    Result ServerStart(const smem_tls_config &tlsConfig, int reconnectRetryTimes = -1) noexcept;
    void Shutdown(bool afterFork = false) noexcept;

    Result Set(const std::string &key, const std::vector<uint8_t> &value) noexcept override;
    Result Add(const std::string &key, int64_t increment, int64_t &value) noexcept override;
    Result Remove(const std::string &key, bool printKeyNotExist) noexcept override;
    Result Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept override;
    Result Cas(const std::string &key, const std::vector<uint8_t> &expect, const std::vector<uint8_t> &value,
               std::vector<uint8_t> &exists) noexcept override;
    Result Watch(const std::string &key,
                 const std::function<void(int result, const std::string &, const std::vector<uint8_t> &)> &notify,
                 uint32_t &wid) noexcept override;
    Result Watch(WatchRankType type, const std::function<void(WatchRankType, uint32_t)> &notify,
                 uint32_t &wid) noexcept override;
    Result Unwatch(uint32_t wid) noexcept override;
    Result Write(const std::string &key, const std::vector<uint8_t> &value, const uint32_t offset) noexcept override;

    std::string GetCompleteKey(const std::string &key) noexcept override;
    std::string GetCommonPrefix() noexcept override;
    StorePtr GetCoreStore() noexcept override;

    void RegisterReconnectHandler(ConfigStoreReconnectHandler callback) noexcept override;
    Result ReConnectAfterBroken(int reconnectRetryTimes) noexcept override;
    bool GetConnectStatus() noexcept override;
    void SetConnectStatus(bool status) noexcept override;
    void RegisterClientBrokenHandler(const ConfigStoreClientBrokenHandler &handler) noexcept override;
    void RegisterServerBrokenHandler(const ConfigStoreServerBrokenHandler &handler) noexcept override;
    void RegisterServerOpHandler(int16_t opCode, const ConfigStoreServerOpHandler &handler) noexcept override;
    void SetRankId(const int32_t &rankId) noexcept override;

protected:
    Result GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept override;

private:
    mutable std::shared_mutex delegateRwLock_;
    TcpConfigStorePtr clientDelegate_{nullptr};
    AccStoreServerPtr serverDelegate_{nullptr};
};

using HaConfigStorePtr = SmRef<HaConfigStore>;

} // namespace smem
} // namespace ock

#endif // SMEM_SMEM_HA_CONFIG_STORE_H