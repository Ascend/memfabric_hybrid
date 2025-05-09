/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef SMEM_SMEM_TCP_CONFIG_STORE_H
#define SMEM_SMEM_TCP_CONFIG_STORE_H

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

#include "smem_config_store.h"
#include "smem_tcp_config_store_server.h"

namespace ock {
namespace smem {

class ClientWaitContext {
public:
    ClientWaitContext(std::mutex &mtx, std::condition_variable &cond) noexcept;
    std::shared_ptr<ock::acc::AccTcpRequestContext> WaitFinished() noexcept;
    void SetFinished(const ock::acc::AccTcpRequestContext &response) noexcept;

private:
    std::mutex &waitMutex_;
    std::condition_variable &waitCond_;
    bool finished_;
    std::shared_ptr<ock::acc::AccTcpRequestContext> responseInfo_;
};

class TcpConfigStore : public ConfigStore {
public:
    TcpConfigStore(std::string ip, uint16_t port, bool isServer, int32_t rankId = 0) noexcept;
    ~TcpConfigStore() noexcept override;

    Result Startup(int reconnectRetryTimes = -1) noexcept;
    void Shutdown() noexcept;

    Result Set(const std::string &key, const std::vector<uint8_t> &value) noexcept override;
    Result Add(const std::string &key, int64_t increment, int64_t &value) noexcept override;
    Result Remove(const std::string &key) noexcept override;
    Result Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept override;
    std::string GetCompleteKey(std::string &key) noexcept override
    {
        return key;
    }

protected:
    Result GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept override;

private:
    std::shared_ptr<ock::acc::AccTcpRequestContext> SendMessageBlocked(const std::vector<uint8_t> &reqBody) noexcept;
    Result LinkBrokenHandler(const ock::acc::AccTcpLinkComplexPtr &link) noexcept;
    Result ReceiveResponseHandler(const ock::acc::AccTcpRequestContext &context) noexcept;
    static uint8_t *DuplicateMessage(const std::vector<uint8_t> &message) noexcept;

private:
    AccStoreServerPtr accServer_;
    ock::acc::AccTcpServerPtr accClient_;
    ock::acc::AccTcpLinkComplexPtr accClientLink_;

    std::mutex msgCtxMutex_;
    std::unordered_map<uint32_t, std::shared_ptr<ClientWaitContext>> msgWaitContext_;
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
