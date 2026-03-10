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
#include "smem_tcp_config_store_server.h"

#include <algorithm>
#include <cstring>
#include <utility>
#include "acc_tcp_server.h"
#include "smem_config_store_logger.h"
#include "smem_message_packer.h"
#include "smem_config_store.h"
#include "smem_tcp_config_store_ssl_helper.h"
#include "mf_str_util.h"
#include "smem_logger.h"

namespace ock {
namespace smem {

std::atomic<uint64_t> StoreWaitContext::idGen_{1UL};
constexpr uint16_t MAX_U16_INDEX = 65535;
constexpr uint32_t HEARTBEAT_TIMEOUT = 3;
constexpr int32_t EPHEMERAL_KEY_TTL_SEC = 5;
constexpr int32_t PERSISTENT_KEY_TTL_SEC = 0;

AccStoreServer::AccStoreServer(std::string ip, uint16_t port, uint32_t worldSize, StoreBackendPtr backend) noexcept
    : requestHandlers_{{MessageType::SET, &AccStoreServer::SetHandler},
                       {MessageType::GET, &AccStoreServer::GetHandler},
                       {MessageType::ADD, &AccStoreServer::AddHandler},
                       {MessageType::REMOVE, &AccStoreServer::RemoveHandler},
                       {MessageType::APPEND, &AccStoreServer::AppendHandler},
                       {MessageType::CAS, &AccStoreServer::CasHandler},
                       {MessageType::WRITE, &AccStoreServer::WriteHandler},
                       {MessageType::WATCH_RANK_STATE, &AccStoreServer::WatchRankStateHandler},
                       {MessageType::HEARTBEAT, &AccStoreServer::HeartbeatHandler}},
      backend_(std::move(backend)), listenIp_{std::move(ip)}, listenPort_{port}, worldSize_{worldSize}
{}

Result AccStoreServer::Startup(const smem_tls_config &tlsConfig) noexcept
{
    std::unique_lock<std::mutex> guard(storeMutex_);
    if (accTcpServer_ != nullptr) {
        STORE_LOG_WARN("tcp store server already startup");
        return SM_OK;
    }

    accTcpServer_ = acc::AccTcpServer::Create();
    if (accTcpServer_ == nullptr) {
        STORE_LOG_ERROR("create acc tcp server failed");
        return SM_NEW_OBJECT_FAILED;
    }

    accTcpServer_->RegisterNewRequestHandler(
        0, [this](const ock::acc::AccTcpRequestContext &context) { return ReceiveMessageHandler(context); });
    accTcpServer_->RegisterNewLinkHandler(
        [this](const ock::acc::AccConnReq &req, const ock::acc::AccTcpLinkComplexPtr &link) {
            return LinkConnectedHandler(req, link);
        });
    accTcpServer_->RegisterLinkBrokenHandler(
        [this](const ock::acc::AccTcpLinkComplexPtr &link) { return LinkBrokenHandler(link); });

    acc::AccTcpServerOptions options{};
    options.listenIp = listenIp_;
    options.listenPort = listenPort_;
    options.enableListener = true;
    options.linkSendQueueSize = ock::acc::UNO_48;
    acc::AccTlsOption tlsOption = GetAccTlsOption(tlsConfig);
    if (tlsOption.enableTls) {
        if (PrepareTlsForAccTcpServer(accTcpServer_, tlsConfig) != SM_OK) {
            STORE_LOG_ERROR("Failed to prepare TLS for AccTcpServer");
            return SM_ERROR;
        }
    }

    auto result = accTcpServer_->Start(options, tlsOption);
    if (result == ock::acc::ACC_LINK_ADDRESS_IN_USE) {
        STORE_LOG_INFO("startup acc tcp server on port: " << listenPort_ << " already in use.");
        return SM_RESOURCE_IN_USE;
    }
    if (result != SM_OK) {
        STORE_LOG_ERROR("startup acc tcp server on port: " << listenPort_ << " failed: " << result);
        return SM_ERROR;
    }

    running_ = true;

    timerThread_ = std::thread{[this]() { TimerThreadTask(); }};
    rankStateThread_ = std::thread{[this]() { RankStateTask(); }};
    checkerThread_ = std::thread{[this]() { CheckerThreadTask(); }};
    STORE_LOG_DEBUG("startup acc tcp server on port: " << listenPort_);
    if (!backend_->IsDistributed()) {
        return SM_OK;
    }
    if (LaunchCleanupThread() != SM_OK) {
        SM_LOG_ERROR("LaunchCleanupThread failed");
        guard.unlock();
        Shutdown(false);
        return SM_ERROR;
    }
    return SM_OK;
}

void AccStoreServer::Shutdown(bool afterFork) noexcept
{
    STORE_LOG_INFO("start to shutdown Acc Store Server");
    {
        if (accTcpServer_ == nullptr) {
            return;
        }
        if (afterFork) {
            accTcpServer_->StopAfterFork();
            if (timerThread_.joinable()) {
                timerThread_.detach();
            }
        } else {
            accTcpServer_->Stop();
        }
        std::unique_lock<std::mutex> lockGuard{storeMutex_};
        running_ = false;
        shouldStop_.store(true);
        storeCond_.notify_all();
        accTcpServer_ = nullptr;
    }

    if (timerThread_.joinable() && !afterFork) {
        try {
            timerThread_.join();
        } catch (const std::system_error &e) {
            STORE_LOG_ERROR("thread join failed: " << e.what());
        }
    }
    if (rankStateThread_.joinable()) {
        rankStateThread_.join();
    }
    if (checkerThread_.joinable()) {
        checkerThread_.join();
    }
    shouldStop_.store(true);
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
    STORE_LOG_INFO("finished shutdown Acc Store Server");
}

void AccStoreServer::RegisterOpHandler(int16_t opcode, const ConfigStoreServerOpHandler &handler) noexcept
{
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    externalOpHandlerMap_[opcode] = handler;
}

void AccStoreServer::RegisterBrokenLinkCHandler(const ConfigStoreServerBrokenHandler &handler) noexcept
{
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    externalBrokenHandler_ = handler;
}

Result AccStoreServer::ReceiveMessageHandler(const ock::acc::AccTcpRequestContext &context) noexcept
{
    auto data = reinterpret_cast<const uint8_t *>(context.DataPtr());
    if (data == nullptr) {
        STORE_LOG_ERROR("request(" << context.SeqNo() << ") handle get null request body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "request no body");
        return SM_INVALID_PARAM;
    }

    SmemMessage requestMessage;
    auto size = SmemMessagePacker::Unpack(data, context.DataLen(), requestMessage);
    if (size < 0) {
        STORE_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request");
        return SM_ERROR;
    }

    auto pos = requestHandlers_.find(requestMessage.mt);
    if (pos == requestHandlers_.end()) {
        STORE_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid message type: " << requestMessage.mt);
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request message type");
        return SM_ERROR;
    }

    return (this->*(pos->second))(context, requestMessage);
}

Result AccStoreServer::LinkConnectedHandler(const ock::acc::AccConnReq &req,
                                            const ock::acc::AccTcpLinkComplexPtr &link) noexcept
{
    uint32_t worldSize = static_cast<uint32_t>(req.rankId >> 32);
    uint32_t rankId = static_cast<uint32_t>(req.rankId & 0xFFFFFFFF);
    STORE_LOG_INFO("New link connected, linkId: " << link->Id() << ", worldSize: " << worldSize
                                                  << ", rankId: " << rankId);
    if (worldSize_ == std::numeric_limits<uint32_t>::max()) {
        SM_ASSERT_RETURN(PersistWorldSize(worldSize) == SUCCESS, SM_ERROR);
        worldSize_ = worldSize;
        STORE_LOG_INFO("Success to fix world size:" << worldSize_);
    }
    if (rankId >= std::numeric_limits<uint32_t>::max()) {
        return SM_OK;
    }

    std::string autoRankingStr = autoRankingStr_ + std::to_string(link->Id());
    union Transfer {
        uint32_t rankId;
        uint8_t data[4];
    } trans{};
    trans.rankId = rankId;

    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    auto ret = backend_->Put(autoRankingStr, std::vector<uint8_t>(trans.data, trans.data + sizeof(trans.data)),
                             EPHEMERAL_KEY_TTL_SEC);
    STORE_ASSERT_RETURN(ret == SUCCESS, ret);
    aliveRankSet_.insert(rankId);
    SM_ASSERT_RETURN(PersistAliveRankIds(aliveRankSet_) == SUCCESS, SM_ERROR);
    return SM_OK;
}

Result AccStoreServer::LinkBrokenHandler(const ock::acc::AccTcpLinkComplexPtr &link) noexcept
{
    STORE_LOG_DEBUG("link broken, linkId: " << link->Id());
    uint32_t rankId = std::numeric_limits<uint32_t>::max();
    uint32_t linkId = link->Id();

    union Transfer {
        uint32_t rankId;
        uint8_t data[sizeof(uint32_t)];
    } trans{};
    std::string autoRankingStr = autoRankingStr_ + std::to_string(linkId);
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    std::vector<uint8_t> outValue;
    auto ret = backend_->Get(autoRankingStr, outValue);
    if (ret == SUCCESS) {
        std::copy(outValue.begin(), outValue.end(), trans.data);
        rankId = trans.rankId;
        aliveRankSet_.erase(rankId);
        STORE_ASSERT_RETURN(backend_->Delete(autoRankingStr) == SUCCESS, SM_ERROR);
        PersistAliveRankIds(aliveRankSet_);
        STORE_LOG_INFO("link broken, linkId: " << linkId << " remove rankId: " << rankId);
    }
    heartBeatMap_.erase(link->Id());
    if (externalBrokenHandler_ != nullptr) {
        externalBrokenHandler_(link->Id(), backend_);
    } else if (aliveRankSet_.empty()) {
        STORE_LOG_INFO("all client link broken, will clear data");
        rankIndex_ = 0;
        backend_->Clear();
        waitCtx_.clear();
        keyWaiters_.clear();
        timedWaiters_.clear();
        rankStateWaiters_.clear();
        rankStateTaskQueue_ = {};
        (void)backend_->Delete(KEY_ALIVE_RANK_LIST);
    }
    rankStateWaiters_.erase(linkId);
    if (rankId == std::numeric_limits<uint32_t>::max()) {
        STORE_LOG_WARN("broken link id: " << linkId << ", cannot find rank id.");
        return SM_OK;
    }
    rankStateTaskQueue_.push(rankId);
    storeCond_.notify_all();
    return SM_OK;
}

Result AccStoreServer::LinkBrokenHandler(const uint32_t linkId) noexcept
{
    STORE_LOG_INFO("inner detect link broken, linkId: " << linkId);
    if (externalBrokenHandler_ != nullptr) {
        externalBrokenHandler_(linkId, backend_);
    }
    heartBeatMap_.erase(linkId);
    return SM_OK;
}

Result AccStoreServer::SetHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || request.values.size() != 1) {
        STORE_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: key value should be one");
        return SM_INVALID_PARAM;
    }

    auto &key = request.keys[0];
    auto &value = request.values[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        STORE_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }

    STORE_LOG_DEBUG("SET REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");
    std::list<ock::acc::AccTcpRequestContext> wakeupWaiters;
    std::vector<uint8_t> reqVal;
    std::unique_lock<std::mutex> lockGuard{storeMutex_};

    if (ExecuteHandle(MessageType::SET, context.Link()->Id(), key, value) != SM_OK) {
        lockGuard.unlock();
        STORE_LOG_ERROR("SET REQUEST(" << context.SeqNo() << ") for key(" << key << "), excute handle failed.");
        ReplyWithMessage(context, StoreErrorCode::ERROR, "failed");
        return StoreErrorCode::ERROR;
    }
    auto ret = backend_->Exist(key);
    if (ret != SUCCESS) {
        auto wPos = keyWaiters_.find(key);
        if (wPos != keyWaiters_.end()) {
            wakeupWaiters = GetOutWaitersInLock(wPos->second);
            reqVal = value;
            keyWaiters_.erase(wPos);
        }
        ret = backend_->Put(key, std::move(value), EPHEMERAL_KEY_TTL_SEC);
    } else {
        ret = backend_->Put(key, std::move(value), EPHEMERAL_KEY_TTL_SEC);
    }
    lockGuard.unlock();

    ReplyWithMessage(context, ret, ret == SUCCESS ? "success" : "error");
    if (!wakeupWaiters.empty()) {
        WakeupWaiters(wakeupWaiters, reqVal);
    }

    return SM_OK;
}

Result AccStoreServer::FindOrInsertRank(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    auto &key = request.keys[0];
    STORE_ASSERT_RETURN(context.Link() != nullptr, SM_INVALID_PARAM);
    auto linkId = context.Link()->Id();
    auto rankingKey = key + std::to_string(linkId);
    STORE_LOG_DEBUG("GET rankingKey(" << rankingKey << ") start.");
    SmemMessage responseMessage{request.mt};
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    std::vector<uint8_t> oldValue;
    auto ret = backend_->Get(rankingKey, oldValue);
    if (ret == SUCCESS) {
        responseMessage.values.emplace_back(oldValue);
        lockGuard.unlock();
        STORE_LOG_INFO("GET REQUEST(" << context.SeqNo() << ") for key(" << rankingKey << ") success.");
        auto response = SmemMessagePacker::Pack(responseMessage);
        ReplyWithMessage(context, StoreErrorCode::SUCCESS, response);
        return SM_OK;
    }
    if (aliveRankSet_.size() >= worldSize_) {
        lockGuard.unlock();
        STORE_LOG_ERROR("Failed to insert rank, rank count:" << aliveRankSet_.size()
                                                             << " equal worldSize: " << worldSize_);
        ReplyWithMessage(context, StoreErrorCode::ERROR, "error: worldSize rankSize bigger than worldSize.");
        return SM_ERROR;
    }
    while (true) {
        rankIndex_ %= worldSize_;
        if (aliveRankSet_.find(rankIndex_) == aliveRankSet_.end()) {
            aliveRankSet_.insert(rankIndex_);
            if (PersistAliveRankIds(aliveRankSet_) != SUCCESS) {
                aliveRankSet_.erase(rankIndex_);
                return SM_ERROR;
            }
            break;
        }
        rankIndex_++;
    }
    union Transfer {
        uint32_t rankId;
        uint8_t date[4];
    } trans{};
    trans.rankId = rankIndex_;
    std::vector<uint8_t> data{trans.date, trans.date + sizeof(trans.date)};
    ret = backend_->Put(rankingKey, std::move(data), EPHEMERAL_KEY_TTL_SEC);
    lockGuard.unlock();

    responseMessage.values.emplace_back(trans.date, trans.date + sizeof(trans.date));
    STORE_LOG_INFO("GET REQUEST(" << context.SeqNo() << ") for key(" << rankingKey << ") rankId:" << trans.rankId
                                  << " worldSize:" << worldSize_);
    auto response = SmemMessagePacker::Pack(responseMessage);
    ReplyWithMessage(context, ret, response);
    return 0;
}

Result AccStoreServer::GetHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || !request.values.empty()) {
        STORE_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: key should be one and no values.");
        return SM_INVALID_PARAM;
    }

    auto &key = request.keys[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        STORE_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }

    if (key.compare(0, autoRankingStr_.size(), autoRankingStr_) == 0) {
        if (!GetStatus()) {
            ReplyWithMessage(context, StoreErrorCode::ERROR, "leader status inactive");
            return SM_ERROR;
        }
        return FindOrInsertRank(context, request);
    }

    STORE_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");
    SmemMessage responseMessage{request.mt};
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    std::vector<uint8_t> oldValue;
    auto ret = backend_->Get(key, oldValue);
    if (ret == SUCCESS) {
        responseMessage.values.push_back(oldValue);
        lockGuard.unlock();

        STORE_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") success.");
        auto response = SmemMessagePacker::Pack(responseMessage);
        ReplyWithMessage(context, StoreErrorCode::SUCCESS, response);
        return SM_OK;
    }

    std::vector<uint8_t> outValue;
    if (ExecuteHandle(MessageType::GET, context.Link()->Id(), key, outValue) == SM_GET_OBJIECT) {
        responseMessage.values.push_back(std::move(outValue));
        lockGuard.unlock();
        STORE_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") from falut info success.");
        auto response = SmemMessagePacker::Pack(responseMessage);
        ReplyWithMessage(context, StoreErrorCode::RESTORE, response);
        return SM_OK;
    }
    if (request.userDef == 0) {
        lockGuard.unlock();
        STORE_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") not exist.");
        ReplyWithMessage(context, StoreErrorCode::NOT_EXIST, "<not exist>");
        return SM_ERROR;
    }

    STORE_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key
                                   << ") waiting timeout=" << request.userDef);
    auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(request.userDef);
    auto timeoutMs = std::chrono::duration_cast<std::chrono::milliseconds>(timeout.time_since_epoch()).count();
    STORE_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") waiting timeout=" << timeoutMs);
    StoreWaitContext waitContext{timeoutMs, key, context};
    auto pair = waitCtx_.emplace(waitContext.Id(), std::move(waitContext));
    auto wPos = keyWaiters_.find(key);
    if (wPos != keyWaiters_.end()) {
        wPos->second.emplace(pair.first->first);
    } else {
        keyWaiters_.emplace(key, std::unordered_set<uint64_t>{pair.first->first});
    }

    if (request.userDef > 0) {
        auto timerPos = timedWaiters_.find(timeoutMs);
        if (timerPos == timedWaiters_.end()) {
            timedWaiters_.emplace(timeoutMs, std::unordered_set<uint64_t>{pair.first->first});
        } else {
            timerPos->second.emplace(pair.first->first);
        }
    }
    return SM_OK;
}

Result AccStoreServer::AddHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || request.values.size() != 1) {
        STORE_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: key value should be one.");
        return SM_INVALID_PARAM;
    }

    auto &key = request.keys[0];
    auto &value = request.values[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        STORE_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }

    std::string valueStr{value.begin(), value.end()};
    STORE_LOG_DEBUG("ADD REQUEST(" << context.SeqNo() << ") for key(" << key << ") value(" << valueStr << ") start.");

    long valueNum;
    STORE_VALIDATE_RETURN(mf::StrUtil::String2Int<long>(valueStr, valueNum), "convert string to long failed.",
                          SM_ERROR);
    if (valueStr != std::to_string(valueNum)) {
        STORE_LOG_ERROR("request(" << context.SeqNo() << ") add for key(" << key << ") value is not a number");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: value should be a number.");
        return SM_ERROR;
    }

    auto responseValue = valueNum;
    std::list<ock::acc::AccTcpRequestContext> wakeupWaiters;
    std::vector<uint8_t> reqVal;
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    if (valueNum > 0 && ExecuteHandle(MessageType::ADD, context.Link()->Id(), key, value) != SM_OK) {
        lockGuard.unlock();
        STORE_LOG_ERROR("ADD REQUEST(" << context.SeqNo() << ") for key(" << key << "), excute handle failed.");
        ReplyWithMessage(context, StoreErrorCode::ERROR, "failed");
        return StoreErrorCode::ERROR;
    }
    std::vector<uint8_t> oldValue;
    auto ret = backend_->Get(key, oldValue);
    if (ret != SUCCESS) {
        auto wPos = keyWaiters_.find(key);
        if (wPos != keyWaiters_.end()) {
            wakeupWaiters = GetOutWaitersInLock(wPos->second);
            reqVal = value;
            keyWaiters_.erase(wPos);
        }
        ret = backend_->Put(key, std::move(value), EPHEMERAL_KEY_TTL_SEC);
    } else {
        std::string oldValueStr{oldValue.begin(), oldValue.end()};
        long storedValueNum = 0;
        auto ret = mf::StrUtil::String2Int<long>(oldValueStr, storedValueNum);
        if ((storedValueNum == 0 && oldValueStr != "0") || !ret) {
            lockGuard.unlock();
            STORE_LOG_ERROR("oldValueStr is " << oldValueStr);
            ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "oldValueStr should be a number.");
            return SM_ERROR;
        }

        storedValueNum += valueNum;
        auto storedValueStr = std::to_string(storedValueNum);
        ret = backend_->Put(key, std::vector<uint8_t>(storedValueStr.begin(), storedValueStr.end()),
                            EPHEMERAL_KEY_TTL_SEC);
        responseValue = storedValueNum;
    }
    lockGuard.unlock();
    STORE_LOG_DEBUG("ADD REQUEST(" << context.SeqNo() << ") for key(" << key << ") value(" << responseValue
                                   << ") end.");
    ReplyWithMessage(context, ret, std::to_string(responseValue));
    if (!wakeupWaiters.empty()) {
        WakeupWaiters(wakeupWaiters, reqVal);
    }
    return SM_OK;
}

Result AccStoreServer::RemoveHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || !request.values.empty()) {
        STORE_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: key should be one and no values.");
        return SM_INVALID_PARAM;
    }

    auto &key = request.keys[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        STORE_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }

    STORE_LOG_DEBUG("REMOVE REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");
    bool removed = false;
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    auto ret = backend_->Exist(key);
    if (ret == SUCCESS) {
        (void)backend_->Delete(key);
        removed = true;
    }
    lockGuard.unlock();
    ReplyWithMessage(context, removed ? StoreErrorCode::SUCCESS : StoreErrorCode::NOT_EXIST,
                     removed ? "success" : "not exist");

    return SM_OK;
}

Result AccStoreServer::AppendHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || request.values.size() != 1) {
        STORE_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: key & value should be one.");
        return SM_INVALID_PARAM;
    }

    auto &key = request.keys[0];
    auto &value = request.values[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        STORE_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }

    STORE_LOG_DEBUG("APPEND REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");
    uint64_t newSize;
    std::list<ock::acc::AccTcpRequestContext> wakeupWaiters;
    std::vector<uint8_t> reqVal;
    std::vector<uint8_t> appendValue = value;
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    std::vector<uint8_t> oldValue;
    auto ret = backend_->Get(key, oldValue);
    if (ret == SUCCESS) {
        oldValue.insert(oldValue.end(), value.begin(), value.end());
        newSize = oldValue.size();
        ret = backend_->Put(key, oldValue, EPHEMERAL_KEY_TTL_SEC);
    } else {
        newSize = value.size();
        auto wPos = keyWaiters_.find(key);
        if (wPos != keyWaiters_.end()) {
            wakeupWaiters = GetOutWaitersInLock(wPos->second);
            reqVal = value;
            keyWaiters_.erase(wPos);
        }
        ret = backend_->Put(key, std::move(value), EPHEMERAL_KEY_TTL_SEC);
    }
    if (ExecuteHandle(MessageType::APPEND, context.Link()->Id(), key, appendValue) != SM_OK) {
        lockGuard.unlock();
        STORE_LOG_ERROR("APPEND REQUEST(" << context.SeqNo() << ") for key(" << key << ") excute handle failed.");
        ReplyWithMessage(context, StoreErrorCode::ERROR, "failed");
        return StoreErrorCode::ERROR;
    }
    lockGuard.unlock();
    ReplyWithMessage(context, ret, std::to_string(newSize));
    if (!wakeupWaiters.empty()) {
        WakeupWaiters(wakeupWaiters, value);
    }

    return SM_OK;
}

Result AccStoreServer::WriteHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || request.values.size() != 1) {
        STORE_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: key & value should be one.");
        return SM_INVALID_PARAM;
    }
    auto &key = request.keys[0];
    auto &value = request.values[0];

    if (key.length() > MAX_KEY_LEN_SERVER) {
        STORE_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }
    STORE_LOG_INFO("WRITE REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");
    uint32_t offset = *(reinterpret_cast<uint32_t *>(value.data()));
    size_t realValSize = value.size() - sizeof(uint32_t);
    STORE_VALIDATE_RETURN(offset <= MAX_U16_INDEX * realValSize, "offset too large, offset:" << offset,
                          StoreErrorCode::INVALID_KEY);

    STORE_LOG_INFO("WRITE REQUEST(" << context.SeqNo() << ") for key(" << key << ") offset(" << offset
                                    << ") value size(" << realValSize << ")");
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    std::vector<uint8_t> oldValue;
    auto ret = backend_->Get(key, oldValue);
    if (ret != SUCCESS) {
        STORE_LOG_INFO("write: not find key:" << key << ", new alloc mem: " << offset + realValSize);
        ret = backend_->Put(key, std::vector<uint8_t>(offset + realValSize, 0), EPHEMERAL_KEY_TTL_SEC);
        if (ret != SUCCESS) {
            STORE_LOG_ERROR("put failed, key=" << key << ", ret: " << ret);
            ReplyWithMessage(context, StoreErrorCode::ERROR, "failed");
            return StoreErrorCode::ERROR;
        }
    }
    ret = backend_->Get(key, oldValue);
    auto &curValue = oldValue;
    if (offset + realValSize > curValue.size()) {
        curValue.resize(offset + realValSize, 0);
        STORE_LOG_INFO("write: not enough kvStore room, expansion size: " << (offset + realValSize));
    }
    std::copy_n(value.data() + sizeof(uint32_t), realValSize, curValue.data() + offset);
    if (ExecuteHandle(MessageType::WRITE, context.Link()->Id(), key, value) != SM_OK) {
        lockGuard.unlock();
        STORE_LOG_ERROR("WRITE REQUEST(" << context.SeqNo() << ") for key(" << key << ") excute handle failed.");
        ReplyWithMessage(context, StoreErrorCode::ERROR, "failed");
        return StoreErrorCode::ERROR;
    }
    ret = backend_->Put(key, curValue, EPHEMERAL_KEY_TTL_SEC);
    if (ret != SUCCESS) {
        lockGuard.unlock();
        STORE_LOG_ERROR("WRITE REQUEST(" << context.SeqNo() << ") for key(" << key
                                         << ") persist update failed, ret:" << ret);
        ReplyWithMessage(context, StoreErrorCode::ERROR, "failed");
        return StoreErrorCode::ERROR;
    }
    lockGuard.unlock();
    ReplyWithMessage(context, StoreErrorCode::SUCCESS, "success");
    return SM_OK;
}

Result AccStoreServer::CasHandler(const ock::acc::AccTcpRequestContext &context,
                                  ock::smem::SmemMessage &request) noexcept
{
    const size_t EXPECTDE_KEY = 1;
    const size_t EXPECTED_VAL = 2;

    if (request.keys.size() != EXPECTDE_KEY || request.values.size() != EXPECTED_VAL) {
        STORE_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: count(key)=1 & count(value)=2");
        return SM_INVALID_PARAM;
    }
    auto &key = request.keys[0];
    auto &expected = request.values[0];
    auto &exchange = request.values[1];
    auto newValue = exchange;
    if (key.length() > MAX_KEY_LEN_SERVER) {
        STORE_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }
    std::string newValueStr = std::string{newValue.begin(), newValue.end()};
    std::vector<uint8_t> exists;
    SmemMessage responseMessage{request.mt};
    std::list<ock::acc::AccTcpRequestContext> wakeupWaiters;
    STORE_LOG_DEBUG("CAS REQUEST(" << context.SeqNo() << ") for key(" << key
                                   << ") start, newValueStr: " << newValueStr);
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    std::vector<uint8_t> oldValue;
    auto ret = backend_->Get(key, oldValue);
    if (ret == SUCCESS) {
        if (expected == oldValue) {
            exists = std::move(oldValue);
            ret = backend_->Put(key, exchange, EPHEMERAL_KEY_TTL_SEC);
        } else {
            exists = oldValue;
        }
    } else {
        ret = SUCCESS;
        if (expected.empty()) {
            ret = backend_->Put(key, exchange, EPHEMERAL_KEY_TTL_SEC);
            auto wPos = keyWaiters_.find(key);
            if (wPos != keyWaiters_.end()) {
                wakeupWaiters = GetOutWaitersInLock(wPos->second);
                keyWaiters_.erase(wPos);
            }
        }
    }
    lockGuard.unlock();
    std::string existsStr = std::string{exists.begin(), exists.end()};
    STORE_LOG_DEBUG("CAS REQUEST(" << context.SeqNo() << ") for key(" << key << ") finished, existsStr: " << existsStr);

    responseMessage.values.push_back(exists);
    auto response = SmemMessagePacker::Pack(responseMessage);
    ReplyWithMessage(context, ret, response);
    if (!wakeupWaiters.empty()) {
        WakeupWaiters(wakeupWaiters, newValue);
    }
    return SM_OK;
}

Result AccStoreServer::WatchRankStateHandler(const acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || request.keys[0] != WATCH_RANK_DOWN_KEY) {
        STORE_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: key should be");
        return SM_INVALID_PARAM;
    }
    STORE_ASSERT_RETURN(context.Link() != nullptr, SM_INVALID_PARAM);
    auto linkId = context.Link()->Id();
    StoreWaitContext waitContext{-1L, WATCH_RANK_DOWN_KEY, context};
    std::unique_lock<std::mutex> uniqueLock{storeMutex_};
    auto pair = rankStateWaiters_.emplace(linkId, waitContext);
    if (!pair.second) {
        uniqueLock.unlock();
        STORE_LOG_ERROR("link id : " << linkId << ", already watched for rank state.");
        return SM_REPEAT_CALL;
    }
    STORE_LOG_DEBUG("WATCH REQUEST(" << context.SeqNo() << ") for key(" << WATCH_RANK_DOWN_KEY
                                     << ") finished, linkId: " << linkId);
    return SM_OK;
}

Result AccStoreServer::HeartbeatHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 0 || request.values.size() != 0) {
        STORE_LOG_ERROR("heart beat request(" << context.SeqNo() << ") handle invalid body");
        return SM_INVALID_PARAM;
    }
    STORE_ASSERT_RETURN(context.Link() != nullptr, SM_INVALID_PARAM);
    uint32_t linkId = context.Link()->Id();
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    heartBeatMap_[linkId] = mf::StrUtil::GetNowTime();
    return SM_OK;
}

std::list<ock::acc::AccTcpRequestContext>
AccStoreServer::GetOutWaitersInLock(const std::unordered_set<uint64_t> &ids) noexcept
{
    std::list<ock::acc::AccTcpRequestContext> reqCtx;
    for (auto id : ids) {
        auto it = waitCtx_.find(id);
        if (it != waitCtx_.end()) {
            reqCtx.emplace_back(std::move(it->second.ReqCtx()));
            auto wit = timedWaiters_.find(it->second.TimeoutMs());
            if (wit != timedWaiters_.end()) {
                wit->second.erase(it->second.Id());
                if (wit->second.empty()) {
                    timedWaiters_.erase(wit);
                }
            }
            waitCtx_.erase(it);
        }
    }
    return std::move(reqCtx);
}

void AccStoreServer::WakeupWaiters(const std::list<ock::acc::AccTcpRequestContext> &waiters,
                                   const std::vector<uint8_t> &value) noexcept
{
    SmemMessage responseMessage{MessageType::GET};
    responseMessage.values.push_back(value);
    auto response = SmemMessagePacker::Pack(responseMessage);
    for (auto &context : waiters) {
        STORE_LOG_DEBUG("WAKEUP REQUEST(" << context.SeqNo() << ").");
        if (!context.Link()->Established()) {
            continue;
        }
        ReplyWithMessage(context, StoreErrorCode::SUCCESS, response);
    }
}

void AccStoreServer::ReplyWithMessage(const ock::acc::AccTcpRequestContext &ctx, int16_t code,
                                      const std::string &message) noexcept
{
    auto response = ock::acc::AccDataBuffer::Create(message.c_str(), message.size());
    if (response == nullptr) {
        STORE_LOG_ERROR("create response message failed");
        return;
    }

    ctx.Reply(code, response);
}

void AccStoreServer::ReplyWithMessage(const ock::acc::AccTcpRequestContext &ctx, int16_t code,
                                      const std::vector<uint8_t> &message) noexcept
{
    auto response = ock::acc::AccDataBuffer::Create(message.data(), message.size());
    if (response == nullptr) {
        STORE_LOG_ERROR("create response message failed");
        return;
    }

    ctx.Reply(code, response);
}

void AccStoreServer::TimerThreadTask() noexcept
{
    std::unordered_set<uint64_t> timeoutIds;
    std::unique_lock<std::mutex> lockerGuard{storeMutex_};
    while (running_) {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        while (!timedWaiters_.empty()) {
            auto it = timedWaiters_.begin();
            if (it->first > timestamp) {
                break;
            }
            timeoutIds.insert(it->second.begin(), it->second.end());
            timedWaiters_.erase(it);
        }

        auto timeoutContexts = GetOutWaitersInLock(timeoutIds);
        lockerGuard.unlock();

        timeoutIds.clear();
        for (auto &ctx : timeoutContexts) {
            if (!ctx.Link()->Established()) {
                STORE_LOG_WARN("Link is not Established, reply timeout response for : " << ctx.SeqNo());
                continue;
            }
            STORE_LOG_DEBUG("reply timeout response for : " << ctx.SeqNo());
            ReplyWithMessage(ctx, StoreErrorCode::TIMEOUT, "<timeout>");
        }

        lockerGuard.lock();
        storeCond_.wait_for(lockerGuard, std::chrono::milliseconds(1), [this]() { return !running_; });
    }
}

void AccStoreServer::RankStateTask() noexcept
{
    while (running_) {
        std::unique_lock<std::mutex> lock(storeMutex_);
        storeCond_.wait(lock, [this] { return !rankStateTaskQueue_.empty() || !running_; });
        if (!running_) {
            return;
        }

        union Transfer {
            uint32_t rankId;
            uint8_t data[sizeof(uint32_t)];
        } trans{};

        auto rankId = std::move(rankStateTaskQueue_.front());
        rankStateTaskQueue_.pop();
        trans.rankId = rankId;
        SmemMessage responseMessage{MessageType::WATCH_RANK_STATE};
        std::vector<uint8_t> value(trans.data, trans.data + sizeof(trans.data));
        responseMessage.values.push_back(value);
        auto response = SmemMessagePacker::Pack(responseMessage);
        for (auto it = rankStateWaiters_.begin(); it != rankStateWaiters_.end(); ++it) {
            if (!it->second.ReqCtx().Link()->Established()) {
                STORE_LOG_WARN("rankId: " << rankId << " down notify to linkId: " << it->first
                                          << ", id: " << it->second.ReqCtx().Link()->Id());
                continue;
            }
            STORE_LOG_DEBUG("rankId: " << rankId << " down notify to linkId: " << it->first);
            ReplyWithMessage(it->second.ReqCtx(), StoreErrorCode::SUCCESS, response);
        }
    }
}

void AccStoreServer::CheckerThreadTask() noexcept
{
    std::unordered_set<uint32_t> brokenLinks;
    std::unique_lock<std::mutex> lockerGuard{storeMutex_};
    while (running_) {
        auto curTime = mf::StrUtil::GetNowTime();
        for (auto it = heartBeatMap_.begin(); it != heartBeatMap_.end();) {
            if ((curTime - it->second) / HEARTBEAT_INTERVAL > HEARTBEAT_TIMEOUT) {
                STORE_LOG_INFO("link(" << it->first << ") broken");
                brokenLinks.insert(it->first);
                it = heartBeatMap_.erase(it);
            } else {
                ++it;
            }
        }
        for (auto linkId : brokenLinks) {
            LinkBrokenHandler(linkId); // private func
        }
        brokenLinks.clear();
        storeCond_.wait_for(lockerGuard, std::chrono::milliseconds(HEARTBEAT_INTERVAL), [this]() { return !running_; });
    }
    STORE_LOG_INFO("checker thread exit");
}

Result AccStoreServer::ExecuteHandle(int16_t opCode, uint32_t linkId, std::string &key,
                                     std::vector<uint8_t> &value) noexcept
{
    auto it = externalOpHandlerMap_.find(opCode);
    if (it == externalOpHandlerMap_.end()) {
        STORE_LOG_DEBUG("execute handle map not find opCode:" << opCode);
        return SM_OK;
    }
    return it->second(linkId, key, value, backend_);
}

Result AccStoreServer::RestoreFromBackend() noexcept
{
    if (!backend_->IsDistributed()) {
        return SM_OK;
    }
    SM_LOG_INFO("Starting restore from backend...");

    if (auto ret = RecoverAliveRankIds(aliveRankFromBackend_); ret != StoreErrorCode::SUCCESS) {
        SM_LOG_WARN("Failed to recover alive rank IDs from backend");
        return SM_OK;
    }

    SM_LOG_INFO("Restore from backend completed, alive ranks: " << aliveRankFromBackend_.size());
    return SM_OK;
}

bool AccStoreServer::GetStatus() noexcept
{
    if (!backend_->IsDistributed()) {
        return true;
    }
    static constexpr int retries = 5;
    static constexpr int retryIntervalSec = 2;
    for (int attempt = 0; attempt <= retries; ++attempt) {
        std::string status;
        auto ret = backend_->Get(KEY_LEADER_STATUS, status);
        if (ret != 0) {
            SM_LOG_WARN("Failed to get leader status from backend, error code: "
                        << ret << ", attempt: " << (attempt + 1) << "/" << (retries + 1));
        } else if (status == "true") {
            SM_LOG_DEBUG("Leader status: active");
            return true;
        } else {
            SM_LOG_DEBUG("Leader status: inactive, attempt: " << (attempt + 1) << "/" << (retries + 1));
        }
        if (attempt < retries) {
            std::this_thread::sleep_for(std::chrono::seconds(retryIntervalSec));
        }
    }
    SM_LOG_WARN("Leader status check failed after " << (retries + 1) << " attempts");
    return false;
}

Result AccStoreServer::UpdateStatus(bool status) noexcept
{
    if (!backend_->IsDistributed()) {
        return SM_OK;
    }
    Result ret;
    if (status) {
        ret = backend_->Put(KEY_LEADER_STATUS, "true", EPHEMERAL_KEY_TTL_SEC);
        if (ret != SM_OK) {
            SM_LOG_ERROR("Failed to set leader status to active");
        } else {
            SM_LOG_INFO("Leader status set to active");
        }
    } else {
        ret = backend_->Delete(KEY_LEADER_STATUS);
        if (ret != SM_OK) {
            SM_LOG_ERROR("Failed to remove leader status");
        } else {
            SM_LOG_INFO("Leader status removed");
        }
    }

    return ret;
}

StoreErrorCode AccStoreServer::PersistWorldSize(uint32_t size) noexcept
{
    if (!backend_->IsDistributed()) {
        return StoreErrorCode::SUCCESS;
    }
    const std::string str = std::to_string(size);
    const std::vector<uint8_t> data(str.begin(), str.end());
    auto ret = backend_->Put(KEY_WORLD_SIZE, data, EPHEMERAL_KEY_TTL_SEC);
    if (ret != SUCCESS) {
        SM_LOG_ERROR("Failed to persist world size: " << size);
    } else {
        SM_LOG_INFO("World size persisted: " << size);
    }
    return ret;
}

StoreErrorCode AccStoreServer::PersistAliveRankIds(const std::unordered_set<uint32_t> &ranks) noexcept
{
    if (!backend_->IsDistributed()) {
        return SUCCESS;
    }
    if (ranks.empty()) {
        auto ret = backend_->Delete(KEY_ALIVE_RANK_LIST);
        if (ret != SUCCESS) {
            SM_LOG_ERROR("Failed to remove alive ranks key from backend");
            return ret;
        }
        SM_LOG_INFO("Alive ranks cleared in backend");
        return SUCCESS;
    }
    std::stringstream ss;
    auto it = ranks.begin();
    ss << *it;

    for (++it; it != ranks.end(); ++it) {
        ss << "," << *it;
    }
    const std::string str = ss.str();
    const std::vector<uint8_t> data(str.begin(), str.end());
    auto ret = backend_->Put(KEY_ALIVE_RANK_LIST, data, 0);
    if (ret != SUCCESS) {
        SM_LOG_ERROR("Failed to persist alive ranks, count: " << ranks.size());
    } else {
        SM_LOG_INFO("Alive ranks persisted, count: " << ranks.size());
    }
    return ret;
}

StoreErrorCode AccStoreServer::RecoverAliveRankIds(std::unordered_set<uint32_t> &outRanks) noexcept
{
    if (!backend_->IsDistributed()) {
        return SUCCESS;
    }
    outRanks.clear();
    std::string rankStr;
    auto ret = backend_->Get(KEY_ALIVE_RANK_LIST, rankStr);
    if (ret != 0) {
        SM_LOG_WARN("Failed to get alive ranks from backend, error code: " << ret);
        return SUCCESS;
    }
    if (rankStr.empty()) {
        SM_LOG_INFO("No alive ranks found in backend");
        return SUCCESS;
    }

    auto list = mf::StrUtil::Split(rankStr, ',');
    for (const auto &idStr : list) {
        if (idStr.empty()) {
            continue;
        }
        uint32_t val;
        if (!mf::StrUtil::String2Uint(idStr, val)) {
            SM_LOG_ERROR("Rank ID check failed: " << idStr);
            outRanks.clear();
            return ERROR;
        }
        outRanks.insert(static_cast<uint32_t>(val));
    }

    // Remove from backend only after successful parsing
    ret = backend_->Delete(KEY_ALIVE_RANK_LIST);
    if (ret != SUCCESS) {
        SM_LOG_WARN("Failed to remove alive ranks key after recovery");
    }

    SM_LOG_INFO("Recovered alive ranks from backend, count: " << outRanks.size());
    return SUCCESS;
}

Result AccStoreServer::LaunchCleanupThread()
{
    const bool isFirstUpdate = aliveRankFromBackend_.empty();
    // First status update
    if (UpdateStatus(isFirstUpdate) != SM_OK) {
        SM_LOG_ERROR("backend update status failed.");
        return SM_ERROR;
    }

    // Launch cleanup thread only for non-first update
    if (!isFirstUpdate) {
        if (cleanupThread_.joinable()) {
            cleanupThread_.join();
        }

        cleanupThread_ = std::thread([this]() { CleanupStaleRanks(); });
    }
    SM_LOG_INFO("Started cleanup thread successful, old rank size: " << aliveRankFromBackend_.size());
    return SM_OK;
}

void AccStoreServer::CleanupStaleRanks() noexcept
{
    // Wait 5 seconds before cleanup
    {
        std::unique_lock<std::mutex> lock{storeMutex_};
        if (storeCond_.wait_for(lock, std::chrono::seconds(STORE_WAIT_TIMEOUT_SEC),
                                [this] { return shouldStop_.load(std::memory_order_acquire); })) {
            return;
        }
    }

    std::unordered_set<uint32_t> ranksToRemove;
    {
        std::lock_guard<std::mutex> lock{storeMutex_};
        for (const uint32_t rank : aliveRankSet_) {
            aliveRankFromBackend_.erase(rank);
        }
        ranksToRemove = aliveRankFromBackend_;
    }

    // Process removals
    for (uint32_t rankId : ranksToRemove) {
        {
            std::lock_guard<std::mutex> lock{storeMutex_};
            rankStateTaskQueue_.push(rankId);
        }
        STORE_LOG_INFO("Remove old rankId: " << rankId);
    }

    // Notify and wait again
    storeCond_.notify_all();
    {
        std::unique_lock<std::mutex> lock{storeMutex_};
        if (storeCond_.wait_for(lock, std::chrono::seconds(STORE_WAIT_TIMEOUT_SEC),
                                [this] { return shouldStop_.load(std::memory_order_acquire); })) {
            return;
        }
    }

    // Final status update
    if (UpdateStatus(true) != SM_OK) {
        SM_LOG_ERROR("backend final update status failed in cleanup thread.");
    }
    SM_LOG_INFO("backend final update status successful in cleanup thread.");
}
} // namespace smem
} // namespace ock
