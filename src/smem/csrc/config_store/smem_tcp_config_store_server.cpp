/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "smem_tcp_config_store_server.h"

#include <algorithm>
#include "acc_tcp_server.h"
#include "config_store_log.h"
#include "smem_message_packer.h"
#include "smem_config_store.h"
#include "smem_tcp_config_store_ssl_helper.h"
#include "mf_str_util.h"

namespace ock {
namespace smem {

std::atomic<uint64_t> StoreWaitContext::idGen_{1UL};
constexpr uint16_t MAX_U16_INDEX = 65535;

AccStoreServer::AccStoreServer(std::string ip, uint16_t port, uint32_t worldSize) noexcept
    : listenIp_{std::move(ip)}, listenPort_{port}, worldSize_{worldSize},
      requestHandlers_{{MessageType::SET, &AccStoreServer::SetHandler},
                       {MessageType::GET, &AccStoreServer::GetHandler},
                       {MessageType::ADD, &AccStoreServer::AddHandler},
                       {MessageType::REMOVE, &AccStoreServer::RemoveHandler},
                       {MessageType::APPEND, &AccStoreServer::AppendHandler},
                       {MessageType::CAS, &AccStoreServer::CasHandler},
                       {MessageType::WRITE, &AccStoreServer::WriteHandler},
                       {MessageType::WATCH_RANK_STATE, &AccStoreServer::WatchRankStateHandler}}
{}

Result AccStoreServer::Startup(const smem_tls_config& tlsConfig) noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
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

    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    running_ = true;
    lockGuard.unlock();

    timerThread_ = std::thread{[this]() { TimerThreadTask(); }};
    rankStateThread_ = std::thread{[this]() { RankStateTask(); }};
    STORE_LOG_DEBUG("startup acc tcp server on port: " << listenPort_);
    return SM_OK;
}

void AccStoreServer::Shutdown(bool afterFork) noexcept
{
    STORE_LOG_INFO("start to shutdown Acc Store Server");
    if (accTcpServer_ == nullptr) {
        return;
    }

    if (afterFork) {
        accTcpServer_->StopAfterFork();
        running_ = false;
        if (timerThread_.joinable()) {
            timerThread_.detach();
        }
    } else {
        accTcpServer_->Stop();
        std::unique_lock<std::mutex> lockGuard{storeMutex_};
        running_ = false;
        lockGuard.unlock();
        storeCond_.notify_one();

        if (timerThread_.joinable()) {
            try {
                timerThread_.join();
            } catch (const std::system_error& e) {
                STORE_LOG_ERROR("thread join failed: " << e.what());
            }
        }
    }

    if (rankStateThread_.joinable()) {
        rankStateTaskCondition_.notify_one();
        rankStateThread_.join();
    }
    accTcpServer_ = nullptr;
    STORE_LOG_INFO("finished shutdown Acc Store Server");
}

void AccStoreServer::RegisterOpHandler(int16_t opcode, const ConfigStoreServerOpHandler &handler) noexcept
{
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    externalOpHandlerMap_[opcode] = handler;
}


void  AccStoreServer::RegisterBrokenLinkCHandler(const ConfigStoreServerBrokenHandler &handler) noexcept
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
    STORE_LOG_INFO("new link connected, linkId: " << link->Id() << ", rank: " << std::hex << req.rankId);
    uint32_t worldSize = static_cast<uint32_t>(req.rankId >> 32);
    uint32_t rankId = static_cast<uint32_t>(req.rankId & 0xFFFFFFFF);
    if (worldSize_ == std::numeric_limits<uint32_t>::max()) {
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
    kvStore_[autoRankingStr] = std::vector<uint8_t>(trans.data, trans.data + sizeof(trans.data));
    aliveRankSet_.insert(rankId);

    return SM_OK;
}

Result AccStoreServer::LinkBrokenHandler(const ock::acc::AccTcpLinkComplexPtr &link) noexcept
{
    STORE_LOG_INFO("link broken, linkId: " << link->Id());
    uint32_t rankId = std::numeric_limits<uint32_t>::max();
    uint32_t linkId = link->Id();

    union Transfer {
        uint32_t rankId;
        uint8_t data[sizeof(uint32_t)];
    } trans{};
    std::string autoRankingStr = autoRankingStr_ + std::to_string(linkId);
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    auto pos = kvStore_.find(autoRankingStr);
    if (pos != kvStore_.end()) {
        std::copy(pos->second.begin(), pos->second.end(), trans.data);
        rankId = trans.rankId;
        aliveRankSet_.erase(rankId);
        kvStore_.erase(pos);
        STORE_LOG_INFO("link broken, linkId: " << linkId << " remove rankId: " << rankId);
    }
    if (aliveRankSet_.empty()) {
        STORE_LOG_INFO("all client link broken, will clear data");
        kvStore_.clear();
        waitCtx_.clear();
        keyWaiters_.clear();
        timedWaiters_.clear();
        return SM_OK;
    }
    lockGuard.unlock();

    if (rankId == std::numeric_limits<uint32_t>::max()) {
        STORE_LOG_ERROR("broken link id: " << linkId << ", cannot find rank id.");
        return SM_ERROR;
    }

    std::unique_lock<std::mutex> rankStateLock{rankStateMutex_};
    rankStateWaiters_.erase(linkId);
    rankStateTaskQueue_.push(rankId);
    rankStateTaskCondition_.notify_one();
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

    if (ExcuteHandle(MessageType::SET, context.Link()->Id(), key, value) != SM_OK) {
        lockGuard.unlock();
        STORE_LOG_DEBUG("SET REQUEST(" << context.SeqNo() << ") for key(" << key << "), excute handle failed.");
        ReplyWithMessage(context, StoreErrorCode::ERROR, "failed");
        return StoreErrorCode::ERROR;
    }
    auto pos = kvStore_.find(key);
    if (pos == kvStore_.end()) {
        auto wPos = keyWaiters_.find(key);
        if (wPos != keyWaiters_.end()) {
            wakeupWaiters = GetOutWaitersInLock(wPos->second);
            reqVal = value;
            keyWaiters_.erase(wPos);
        }
        kvStore_.emplace(key, std::move(value));
    } else {
        pos->second = std::move(value);
    }
    lockGuard.unlock();

    ReplyWithMessage(context, StoreErrorCode::SUCCESS, "success");
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
    STORE_LOG_INFO("GET rankingKey(" << rankingKey << ") success.");
    SmemMessage responseMessage{request.mt};
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    auto pos = kvStore_.find(rankingKey);
    if (pos != kvStore_.end()) {
        responseMessage.values.emplace_back(pos->second);
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
    kvStore_.emplace(rankingKey, std::move(data));
    lockGuard.unlock();

    responseMessage.values.emplace_back(trans.date, trans.date + sizeof(trans.date));
    STORE_LOG_INFO("GET REQUEST(" << context.SeqNo() << ") for key(" << rankingKey << ") rankId:" << trans.rankId
                               << " rankId_:" << rankIndex_);
    auto response = SmemMessagePacker::Pack(responseMessage);
    ReplyWithMessage(context, StoreErrorCode::SUCCESS, response);
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
        return FindOrInsertRank(context, request);
    }

    STORE_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");
    SmemMessage responseMessage{request.mt};
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    auto pos = kvStore_.find(key);
    if (pos != kvStore_.end()) {
        responseMessage.values.push_back(pos->second);
        lockGuard.unlock();

        STORE_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") success.");
        auto response = SmemMessagePacker::Pack(responseMessage);
        ReplyWithMessage(context, StoreErrorCode::SUCCESS, response);
        return SM_OK;
    }

    std::vector<uint8_t> outValue;
    if (ExcuteHandle(MessageType::GET, context.Link()->Id(), key, outValue) == SM_GET_OBJIECT) {
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
    lockGuard.unlock();

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
    if (valueNum > 0 && ExcuteHandle(MessageType::ADD, context.Link()->Id(), key, value) != SM_OK) {
        lockGuard.unlock();
        STORE_LOG_DEBUG("ADD REQUEST(" << context.SeqNo() << ") for key(" << key << "), excute handle failed.");
        ReplyWithMessage(context, StoreErrorCode::ERROR, "failed");
        return StoreErrorCode::ERROR;
    }
    auto pos = kvStore_.find(key);
    if (pos == kvStore_.end()) {
        auto wPos = keyWaiters_.find(key);
        if (wPos != keyWaiters_.end()) {
            wakeupWaiters = GetOutWaitersInLock(wPos->second);
            reqVal = value;
            keyWaiters_.erase(wPos);
        }
        kvStore_.emplace(key, std::move(value));
    } else {
        std::string oldValueStr{pos->second.begin(), pos->second.end()};
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
        pos->second = std::vector<uint8_t>(storedValueStr.begin(), storedValueStr.end());
        responseValue = storedValueNum;
    }
    lockGuard.unlock();
    STORE_LOG_DEBUG("ADD REQUEST(" << context.SeqNo() << ") for key(" << key
        << ") value(" << responseValue << ") end.");
    ReplyWithMessage(context, StoreErrorCode::SUCCESS, std::to_string(responseValue));
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
    auto pos = kvStore_.find(key);
    if (pos != kvStore_.end()) {
        kvStore_.erase(pos);
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
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    auto pos = kvStore_.find(key);
    if (pos != kvStore_.end()) {
        pos->second.insert(pos->second.end(), value.begin(), value.end());
        newSize = pos->second.size();
    } else {
        newSize = value.size();
        auto wPos = keyWaiters_.find(key);
        if (wPos != keyWaiters_.end()) {
            wakeupWaiters = GetOutWaitersInLock(wPos->second);
            reqVal = value;
            keyWaiters_.erase(wPos);
        }
        kvStore_.emplace(key, std::move(value));
    }
    if (ExcuteHandle(MessageType::APPEND, context.Link()->Id(), key, value) != SM_OK) {
        lockGuard.unlock();
        STORE_LOG_DEBUG("APPEND REQUEST(" << context.SeqNo() << ") for key(" << key << ") excute handle failed.");
        ReplyWithMessage(context, StoreErrorCode::ERROR, "failed");
        return StoreErrorCode::ERROR;
    }
    lockGuard.unlock();
    ReplyWithMessage(context, StoreErrorCode::SUCCESS, std::to_string(newSize));
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
    STORE_VALIDATE_RETURN(offset <= MAX_U16_INDEX * realValSize,
                          "offset too large, offset:" << offset, StoreErrorCode::INVALID_KEY);
    
    STORE_LOG_INFO("WRITE REQUEST(" << context.SeqNo() << ") for key(" << key
                << ") offset(" << offset << ") value size(" << realValSize << ")");
    std::unique_lock<std::mutex> lockGuard{storeMutex_};

    if (kvStore_.find(key) == kvStore_.end()) {
        kvStore_.emplace(key, std::vector<uint8_t>(offset + realValSize, 0));
        STORE_LOG_INFO("write: not find key:" << key << ", new alloc mem: " << offset + realValSize);
    }
    auto& curValue = kvStore_.find(key)->second;
    if (offset + realValSize > curValue.size()) {
        curValue.resize(offset + realValSize, 0);
        STORE_LOG_INFO("write: not enough kvStore room, expansion size: " << (offset + realValSize));
    }
    std::copy_n(value.data() + sizeof(uint32_t), realValSize, curValue.data() + offset);
    if (ExcuteHandle(MessageType::WRITE, context.Link()->Id(), key, value) != SM_OK) {
        lockGuard.unlock();
        STORE_LOG_INFO("WRITE REQUEST(" << context.SeqNo() << ") for key(" << key << ") excute handle failed.");
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

    std::vector<uint8_t> exists;
    SmemMessage responseMessage{request.mt};
    std::list<ock::acc::AccTcpRequestContext> wakeupWaiters;
    STORE_LOG_DEBUG("CAS REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");

    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    auto pos = kvStore_.find(key);
    if (pos != kvStore_.end()) {
        if (expected == pos->second) {
            exists = std::move(pos->second);
            pos->second = std::move(exchange);
        } else {
            exists = pos->second;
        }
    } else {
        if (expected.empty()) {
            kvStore_.emplace(key, std::move(exchange));
            auto wPos = keyWaiters_.find(key);
            if (wPos != keyWaiters_.end()) {
                wakeupWaiters = GetOutWaitersInLock(wPos->second);
                keyWaiters_.erase(wPos);
            }
        }
    }
    lockGuard.unlock();
    STORE_LOG_DEBUG("CAS REQUEST(" << context.SeqNo() << ") for key(" << key << ") finished.");

    responseMessage.values.push_back(exists);
    auto response = SmemMessagePacker::Pack(responseMessage);
    ReplyWithMessage(context, StoreErrorCode::SUCCESS, response);
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
    std::unique_lock<std::mutex> uniqueLock{rankStateMutex_};
    auto pair = rankStateWaiters_.emplace(linkId, waitContext);
    if (!pair.second) {
        uniqueLock.unlock();
        STORE_LOG_ERROR("link id : " << linkId << ", already watched for rank state.");
        return SM_REPEAT_CALL;
    }
    STORE_LOG_DEBUG("WATCH REQUEST(" << context.SeqNo() << ") for key(" << WATCH_RANK_DOWN_KEY << ") finished.");
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
        std::unique_lock<std::mutex> lock(rankStateMutex_);
        rankStateTaskCondition_.wait(lock, [this] { return !rankStateTaskQueue_.empty() || !running_; });
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
            STORE_LOG_DEBUG("rankId: " << rankId << " down notify to linkId: " << it->first);
            ReplyWithMessage(it->second.ReqCtx(), StoreErrorCode::SUCCESS, response);
        }
    }
}

Result AccStoreServer::ExcuteHandle(int16_t opCode, uint32_t linkId, std::string &key,
                                    std::vector<uint8_t> &value) noexcept
{
    auto it = externalOpHandlerMap_.find(opCode);
    if (it == externalOpHandlerMap_.end()) {
        STORE_LOG_DEBUG("excute handle map not find opCode:" << opCode);
        return SM_OK;
    }
    return it->second(linkId, key, value, kvStore_);
}
}  // namespace smem
}  // namespace ock