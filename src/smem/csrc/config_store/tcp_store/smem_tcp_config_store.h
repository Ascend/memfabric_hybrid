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

#ifndef SMEM_SMEM_TCP_CONFIG_STORE_H
#define SMEM_SMEM_TCP_CONFIG_STORE_H

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <functional>

#include "smem_config_store.h"
#include "smem_tcp_config_store_server.h"

namespace ock {
namespace smem {

class ClientCommonContext {
public:
    virtual ~ClientCommonContext() = default;
    virtual std::shared_ptr<ock::acc::AccTcpRequestContext> WaitFinished() noexcept = 0;
    virtual void SetFinished(const ock::acc::AccTcpRequestContext &response) noexcept = 0;
    virtual void SetFailedFinish() noexcept = 0;
    virtual bool Blocking() const noexcept = 0;
    virtual bool OnlyOneTime() const noexcept
    {
        return true;
    }
};

class TcpConfigStore : public ConfigStoreManager {
public:
    TcpConfigStore(StoreBackendPtr storeBackend, std::string ip, uint16_t port, bool isServer, uint32_t worldSize = 0,
                   int32_t rankId = -1) noexcept;
    ~TcpConfigStore() noexcept override;

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
    std::string GetCompleteKey(const std::string &key) noexcept override
    {
        return key;
    }

    std::string GetCommonPrefix() noexcept override
    {
        return "";
    }

    StorePtr GetCoreStore() noexcept override
    {
        return this;
    }

    void RegisterReconnectHandler(ConfigStoreReconnectHandler callback) noexcept override
    {
        reconnectHandler = callback;
    }

    void SetRankId(const int32_t &rankId) noexcept override
    {
        rankId_ = rankId;
    }

    Result ReConnectAfterBroken(int reconnectRetryTimes) noexcept override;
    bool GetConnectStatus() noexcept override;
    void SetConnectStatus(bool status) noexcept override;
    void RegisterClientBrokenHandler(const ConfigStoreClientBrokenHandler &handler) noexcept override;

    void RegisterServerBrokenHandler(const ConfigStoreServerBrokenHandler &handler) noexcept override;

    void RegisterServerOpHandler(int16_t opCode, const ConfigStoreServerOpHandler &handler) noexcept override;

protected:
    Result GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept override;

private:
    std::shared_ptr<ock::acc::AccTcpRequestContext> SendMessageBlocked(const std::vector<uint8_t> &reqBody) noexcept;
    Result LinkBrokenHandler(const ock::acc::AccTcpLinkComplexPtr &link) noexcept;
    Result ReceiveResponseHandler(const ock::acc::AccTcpRequestContext &context) noexcept;
    Result SendWatchRequest(const std::vector<uint8_t> &reqBody,
                            const std::function<void(int result, const std::vector<uint8_t> &)> &notify,
                            uint32_t &id) noexcept;
    void HeartBeat() noexcept;

    inline int32_t LocalNonBlockSend(int16_t msgType, uint32_t seqNo, const acc::AccDataBufferPtr &d,
                                     const acc::AccDataBufferPtr &cbCtx)
    {
        auto ret = accClientLink_->NonBlockSend(msgType, seqNo, d, cbCtx);
        if (ret == acc::ACC_LINK_ERROR) {
            ReConnectAfterBroken(1UL);
            ret = accClientLink_->NonBlockSend(msgType, seqNo, d, cbCtx);
        }
        return ret;
    }

private:
    AccStoreServerPtr accServer_;
    ock::acc::AccTcpServerPtr accClient_;
    ock::acc::AccTcpLinkComplexPtr accClientLink_;

    std::mutex msgCtxMutex_;
    std::unordered_map<uint32_t, std::shared_ptr<ClientCommonContext>> msgClientContext_;
    static std::atomic<uint32_t> reqSeqGen_;

    std::mutex mutex_;
    const std::string serverIp_;
    const uint16_t serverPort_;
    const bool isServer_;
    int32_t rankId_;
    const uint32_t worldSize_;
    std::atomic<bool> isConnect_{false};
    ConfigStoreReconnectHandler reconnectHandler{nullptr};
    ConfigStoreClientBrokenHandler brokenHandler_ = nullptr;
    std::atomic<bool> isRunning_{false};
    std::thread heartBeatThread_;
    StoreBackendPtr backend_;
};
using TcpConfigStorePtr = SmRef<TcpConfigStore>;
} // namespace smem
} // namespace ock

#endif // SMEM_SMEM_TCP_CONFIG_STORE_H