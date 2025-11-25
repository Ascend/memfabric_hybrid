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

#ifndef SMEM_SMEM_TCP_CONFIG_STORE_SERVER_H
#define SMEM_SMEM_TCP_CONFIG_STORE_SERVER_H

#include <list>
#include <mutex>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>

#include "acc_tcp_server.h"
#include "smem_bm_def.h"
#include "smem_config_store.h"
#include "smem_message_packer.h"
#include "smem_ref.h"

namespace ock {
namespace smem {
class StoreWaitContext {
public:
    StoreWaitContext(int64_t tmMs, std::string key, const ock::acc::AccTcpRequestContext &reqCtx) noexcept
        : id_{idGen_.fetch_add(1UL)},
          timeoutMs_{tmMs},
          key_{std::move(key)},
          reqCtx_{reqCtx}
    {
    }

    uint64_t Id() const noexcept
    {
        return id_;
    }

    int64_t TimeoutMs() const noexcept
    {
        return timeoutMs_;
    }

    const std::string &Key() const noexcept
    {
        return key_;
    }

    const ock::acc::AccTcpRequestContext &ReqCtx() const noexcept
    {
        return reqCtx_;
    }

    ock::acc::AccTcpRequestContext &ReqCtx() noexcept
    {
        return reqCtx_;
    }

private:
    const uint64_t id_;
    const int64_t timeoutMs_;
    const std::string key_;
    ock::acc::AccTcpRequestContext reqCtx_;
    static std::atomic<uint64_t> idGen_;
};

class AccStoreServer : public SmReferable {
public:
    AccStoreServer(std::string ip, uint16_t port, uint32_t worldSize) noexcept;
    ~AccStoreServer() override = default;

    Result Startup(const smem_tls_config& tlsConfig) noexcept;
    void Shutdown(bool afterFork = false) noexcept;
    void RegisterBrokenLinkCHandler(const ConfigStoreServerBrokenHandler &handler) noexcept;

    void RegisterOpHandler(int16_t opCode, const ConfigStoreServerOpHandler &handler) noexcept;

private:
    Result ReceiveMessageHandler(const ock::acc::AccTcpRequestContext &context) noexcept;
    Result LinkConnectedHandler(const ock::acc::AccConnReq &req, const ock::acc::AccTcpLinkComplexPtr &link) noexcept;
    Result LinkBrokenHandler(const ock::acc::AccTcpLinkComplexPtr &link) noexcept;

    /* business handler */
    Result SetHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept;
    Result GetHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept;
    Result AddHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept;
    Result RemoveHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept;
    Result AppendHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept;
    Result CasHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept;
    Result WatchRankStateHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept;
    Result WriteHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept;

    std::list<ock::acc::AccTcpRequestContext> GetOutWaitersInLock(const std::unordered_set<uint64_t> &ids) noexcept;
    void WakeupWaiters(const std::list<ock::acc::AccTcpRequestContext> &waiters,
                       const std::vector<uint8_t> &value) noexcept;
    void ReplyWithMessage(const ock::acc::AccTcpRequestContext &ctx, int16_t code, const std::string &message) noexcept;
    void ReplyWithMessage(const ock::acc::AccTcpRequestContext &ctx, int16_t code,
                          const std::vector<uint8_t> &message) noexcept;
    void TimerThreadTask() noexcept;
    void RankStateTask() noexcept;
    Result FindOrInsertRank(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept;
    Result ExcuteHandle(int16_t opCode, uint32_t linkId, std::string &key, std::vector<uint8_t> &value) noexcept;

private:
    static constexpr uint32_t MAX_KEY_LEN_SERVER = 2048U;
    // prevent access broken global value during global static destructor
    const std::string autoRankingStr_ = AutoRankingStr;

    using MessageHandle = int32_t (AccStoreServer::*)(const ock::acc::AccTcpRequestContext &, SmemMessage &);
    const std::unordered_map<MessageType, MessageHandle> requestHandlers_;

    std::mutex storeMutex_;
    std::condition_variable storeCond_;
    std::unordered_map<std::string, std::vector<uint8_t>> kvStore_;
    std::unordered_map<uint64_t, StoreWaitContext> waitCtx_;
    std::unordered_map<std::string, std::unordered_set<uint64_t>> keyWaiters_;
    ock::acc::AccTcpServerPtr accTcpServer_;
    std::unordered_map<int64_t, std::unordered_set<uint64_t>> timedWaiters_;
    std::thread timerThread_;
    bool running_{false};

    std::mutex rankStateMutex_;
    std::condition_variable rankStateTaskCondition_;
    std::unordered_map<uint32_t, StoreWaitContext> rankStateWaiters_;
    std::queue<uint32_t> rankStateTaskQueue_;
    std::thread rankStateThread_;

    const std::string listenIp_;
    const uint16_t listenPort_;
    std::mutex mutex_;
    uint32_t worldSize_;
    uint32_t rankIndex_{0};
    std::unordered_set<uint32_t> aliveRankSet_;
    std::unordered_map<int16_t, ConfigStoreServerOpHandler> externalOpHandlerMap_;
    ConfigStoreServerBrokenHandler externalBrokenHandler_{nullptr};
};
using AccStoreServerPtr = SmRef<AccStoreServer>;
}  // namespace smem
}  // namespace ock

#endif  // SMEM_SMEM_TCP_CONFIG_STORE_SERVER_H
