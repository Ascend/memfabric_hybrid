/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
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
};

class TcpConfigStore : public ConfigStore {
public:
    TcpConfigStore(std::string ip, uint16_t port, bool isServer, int32_t rankId = 0) noexcept;
    ~TcpConfigStore() noexcept override;

    Result Startup(const AcclinkTlsOption &tlsOption, int reconnectRetryTimes = -1) noexcept;
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
    Result Unwatch(uint32_t wid) noexcept override;
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

protected:
    Result GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept override;

private:
    std::shared_ptr<ock::acc::AccTcpRequestContext> SendMessageBlocked(const std::vector<uint8_t> &reqBody) noexcept;
    Result LinkBrokenHandler(const ock::acc::AccTcpLinkComplexPtr &link) noexcept;
    Result ReceiveResponseHandler(const ock::acc::AccTcpRequestContext &context) noexcept;
    Result SendWatchRequest(const std::vector<uint8_t> &reqBody,
                            const std::function<void(int result, const std::vector<uint8_t> &)> &notify,
                            uint32_t &id) noexcept;

    Result AccClientStart(const AcclinkTlsOption &tlsOption) noexcept;

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
    const int32_t rankId_;
};
using TcpConfigStorePtr = SmRef<TcpConfigStore>;
}  // namespace smem
}  // namespace ock

#endif  // SMEM_SMEM_TCP_CONFIG_STORE_H
