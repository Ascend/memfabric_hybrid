/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <algorithm>
#include "smem_logger.h"
#include "smem_message_packer.h"
#include "smem_config_store.h"
#include "smem_tcp_config_store_server.h"

namespace ock {
namespace smem {
std::atomic<uint64_t> StoreWaitContext::idGen_{1UL};
AccStoreServer::AccStoreServer(std::string ip, uint16_t port, uint32_t worldSize) noexcept
    : listenIp_{std::move(ip)},
      listenPort_{port},
      worldSize_{worldSize},
      requestHandlers_{
          {MessageType::SET, &AccStoreServer::SetHandler},       {MessageType::GET, &AccStoreServer::GetHandler},
          {MessageType::ADD, &AccStoreServer::AddHandler},       {MessageType::REMOVE, &AccStoreServer::RemoveHandler},
          {MessageType::APPEND, &AccStoreServer::AppendHandler}, {MessageType::CAS, &AccStoreServer::CasHandler}}
{
}

Result AccStoreServer::Startup() noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (accTcpServer_ != nullptr) {
        SM_LOG_WARN("tcp store server already startup");
        return SM_OK;
    }

    auto tmpAccTcpServer = ock::acc::AccTcpServer::Create();
    if (tmpAccTcpServer == nullptr) {
        SM_LOG_ERROR("create acc tcp server failed");
        return SM_NEW_OBJECT_FAILED;
    }

    tmpAccTcpServer->RegisterNewRequestHandler(
        0, [this](const ock::acc::AccTcpRequestContext &context) { return ReceiveMessageHandler(context); });
    tmpAccTcpServer->RegisterNewLinkHandler(
        [this](const ock::acc::AccConnReq &req, const ock::acc::AccTcpLinkComplexPtr &link) {
            return LinkConnectedHandler(req, link);
        });
    tmpAccTcpServer->RegisterLinkBrokenHandler(
        [this](const ock::acc::AccTcpLinkComplexPtr &link) { return LinkBrokenHandler(link); });

    ock::acc::AccTcpServerOptions options;
    options.listenIp = listenIp_;
    options.listenPort = listenPort_;
    options.enableListener = true;
    options.linkSendQueueSize = ock::acc::UNO_48;
    auto result = tmpAccTcpServer->Start(options);
    if (result == ock::acc::ACC_LINK_ADDRESS_IN_USE) {
        SM_LOG_INFO("startup acc tcp server on port: " << listenPort_ << " already in use.");
        return SM_RESOURCE_IN_USE;
    }
    if (result != SM_OK) {
        SM_LOG_ERROR("startup acc tcp server on port: " << listenPort_ << " failed: " << result);
        return SM_ERROR;
    }

    accTcpServer_ = tmpAccTcpServer;

    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    running_ = true;
    lockGuard.unlock();

    timerThread_ = std::thread{[this]() {
        TimerThreadTask();
    }};

    return SM_OK;
}

void AccStoreServer::Shutdown() noexcept
{
    SM_LOG_INFO("start to shutdown Acc Store Server");
    if (accTcpServer_ == nullptr) {
        return;
    }

    accTcpServer_->Stop();
    accTcpServer_ = nullptr;

    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    running_ = false;
    lockGuard.unlock();
    storeCond_.notify_one();
    timerThread_.join();
    SM_LOG_INFO("finished shutdown Acc Store Server");
}

Result AccStoreServer::ReceiveMessageHandler(const ock::acc::AccTcpRequestContext &context) noexcept
{
    auto data = reinterpret_cast<const uint8_t *>(context.DataPtr());
    if (data == nullptr) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle get null request body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "request no body");
        return SM_INVALID_PARAM;
    }

    std::vector<uint8_t> body(data, data + context.DataLen());
    SmemMessage requestMessage;
    auto size = SmemMessagePacker::Unpack(body, requestMessage);
    if (size < 0) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request");
        return SM_ERROR;
    }

    auto pos = requestHandlers_.find(requestMessage.mt);
    if (pos == requestHandlers_.end()) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid message type: " << requestMessage.mt);
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request message type");
        return SM_ERROR;
    }

    return (this->*(pos->second))(context, requestMessage);
}

Result AccStoreServer::LinkConnectedHandler(const ock::acc::AccConnReq &req,
                                            const ock::acc::AccTcpLinkComplexPtr &link) noexcept
{
    SM_LOG_INFO("new link connected, linkId: " << link->Id() << ", rank: " << req.rankId);
    return SM_OK;
}

Result AccStoreServer::LinkBrokenHandler(const ock::acc::AccTcpLinkComplexPtr &link) noexcept
{
    SM_LOG_INFO("link broken, linkId: " << link->Id());
    std::string autoRankingStr = AutoRankingStr + std::to_string(link->Id());
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    auto pos = kvStore_.find(autoRankingStr);
    if (pos != kvStore_.end()) {
        union Transfer {
            uint32_t rankId;
            uint8_t date[4];
        } trans{};
        std::copy(pos->second.begin(), pos->second.end(), trans.date);
        uint32_t rankId = trans.rankId;
        aliveRankSet.erase(rankId);
        kvStore_.erase(pos);
        SM_LOG_INFO("link broken, linkId: " << link->Id() << " remove rankId: " << rankId);
    }
    return SM_OK;
}

Result AccStoreServer::SetHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || request.values.size() != 1) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: key value should be one");
        return SM_INVALID_PARAM;
    }

    auto &key = request.keys[0];
    auto &value = request.values[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        SM_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }

    SM_LOG_DEBUG("SET REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");
    std::list<ock::acc::AccTcpRequestContext> wakeupWaiters;
    std::vector<uint8_t> reqVal;
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
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
    auto linkId = context.Link()->Id();
    auto rankingKey = key + std::to_string(linkId);
    SM_LOG_DEBUG("GET rankingKey(" << rankingKey << ") success.");
    SmemMessage responseMessage{request.mt};
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    auto pos = kvStore_.find(rankingKey);
    if (pos != kvStore_.end()) {
        responseMessage.values.emplace_back(pos->second);
        lockGuard.unlock();
        SM_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << rankingKey << ") success.");
        auto response = SmemMessagePacker::Pack(responseMessage);
        ReplyWithMessage(context, StoreErrorCode::SUCCESS, response);
        return SM_OK;
    }
    if (aliveRankSet.size() >= worldSize_) {
        SM_LOG_ERROR("Failed to insert rank, rank count:" << aliveRankSet.size() << " equal worldSize: " << worldSize_);
        ReplyWithMessage(context, StoreErrorCode::ERROR, "error: worldSize rankSize bigger than worldSize.");
        return SM_ERROR;
    }
    while (true) {
        rankIndex_ %= worldSize_;
        if (aliveRankSet.find(rankIndex_) == aliveRankSet.end()) {
            aliveRankSet.insert(rankIndex_);
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
    SM_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << rankingKey << ") success.");
    auto response = SmemMessagePacker::Pack(responseMessage);
    ReplyWithMessage(context, StoreErrorCode::SUCCESS, response);
    return 0;
}

Result AccStoreServer::GetHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || !request.values.empty()) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: key should be one and no values.");
        return SM_INVALID_PARAM;
    }

    auto &key = request.keys[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        SM_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }

    if (key.compare(0, AutoRankingStr.size(), AutoRankingStr) == 0) {
        return FindOrInsertRank(context, request);
    }

    SM_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");
    SmemMessage responseMessage{request.mt};
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    auto pos = kvStore_.find(key);
    if (pos != kvStore_.end()) {
        responseMessage.values.push_back(pos->second);
        lockGuard.unlock();

        SM_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") success.");
        auto response = SmemMessagePacker::Pack(responseMessage);
        ReplyWithMessage(context, StoreErrorCode::SUCCESS, response);
        return SM_OK;
    }

    if (request.userDef == 0) {
        lockGuard.unlock();

        SM_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") not exist.");
        ReplyWithMessage(context, StoreErrorCode::NOT_EXIST, "<not exist>");
        return SM_ERROR;
    }

    SM_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") waiting timeout=" << request.userDef);
    auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(request.userDef);
    auto timeoutMs = std::chrono::duration_cast<std::chrono::milliseconds>(timeout.time_since_epoch()).count();
    SM_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") waiting timeout=" << timeoutMs);
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
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: key value should be one.");
        return SM_INVALID_PARAM;
    }

    auto &key = request.keys[0];
    auto &value = request.values[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        SM_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }

    std::string valueStr{value.begin(), value.end()};
    SM_LOG_DEBUG("ADD REQUEST(" << context.SeqNo() << ") for key(" << key << ") value(" << valueStr << ") start.");
    auto valueNum = strtol(valueStr.c_str(), nullptr, 10);
    if (valueStr != std::to_string(valueNum)) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") add for key(" << key << ") value is not a number");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: value should be a number.");
        return SM_ERROR;
    }

    auto responseValue = valueNum;
    std::list<ock::acc::AccTcpRequestContext> wakeupWaiters;
    std::vector<uint8_t> reqVal;
    std::unique_lock<std::mutex> lockGuard{storeMutex_};
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
        auto storedValueNum = strtol(oldValueStr.c_str(), nullptr, 10);
        storedValueNum += valueNum;
        auto storedValueStr = std::to_string(storedValueNum);
        pos->second = std::vector<uint8_t>(storedValueStr.begin(), storedValueStr.end());
        responseValue = storedValueNum;
    }
    lockGuard.unlock();
    SM_LOG_DEBUG("ADD REQUEST(" << context.SeqNo() << ") for key(" << key << ") value(" << responseValue << ") end.");
    ReplyWithMessage(context, StoreErrorCode::SUCCESS, std::to_string(responseValue));
    if (!wakeupWaiters.empty()) {
        WakeupWaiters(wakeupWaiters, reqVal);
    }
    return SM_OK;
}

Result AccStoreServer::RemoveHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || !request.values.empty()) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: key should be one and no values.");
        return SM_INVALID_PARAM;
    }

    auto &key = request.keys[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        SM_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }

    SM_LOG_DEBUG("REMOVE REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");
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
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: key & value should be one.");
        return SM_INVALID_PARAM;
    }

    auto &key = request.keys[0];
    auto &value = request.values[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        SM_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }

    SM_LOG_DEBUG("APPEND REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");
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
    lockGuard.unlock();
    ReplyWithMessage(context, StoreErrorCode::SUCCESS, std::to_string(newSize));
    if (!wakeupWaiters.empty()) {
        WakeupWaiters(wakeupWaiters, value);
    }

    return SM_OK;
}

Result AccStoreServer::CasHandler(const ock::acc::AccTcpRequestContext &context,
                                  ock::smem::SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || request.values.size() != 2) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body");
        ReplyWithMessage(context, StoreErrorCode::INVALID_MESSAGE, "invalid request: count(key)=1 & count(value)=2");
        return SM_INVALID_PARAM;
    }

    auto &key = request.keys[0];
    auto &expected = request.values[0];
    auto &exchange = request.values[1];
    auto newValue = exchange;
    if (key.length() > MAX_KEY_LEN_SERVER) {
        SM_LOG_ERROR("key length too large, length: " << key.length());
        return StoreErrorCode::INVALID_KEY;
    }

    std::vector<uint8_t> exists;
    SmemMessage responseMessage{request.mt};
    std::list<ock::acc::AccTcpRequestContext> wakeupWaiters;
    SM_LOG_DEBUG("CAS REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");

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
    SM_LOG_DEBUG("CAS REQUEST(" << context.SeqNo() << ") for key(" << key << ") finished.");

    responseMessage.values.push_back(exists);
    auto response = SmemMessagePacker::Pack(responseMessage);
    ReplyWithMessage(context, StoreErrorCode::SUCCESS, response);
    if (!wakeupWaiters.empty()) {
        WakeupWaiters(wakeupWaiters, newValue);
    }
    return SM_OK;
}

std::list<ock::acc::AccTcpRequestContext> AccStoreServer::GetOutWaitersInLock(
    const std::unordered_set<uint64_t> &ids) noexcept
{
    std::list<ock::acc::AccTcpRequestContext> reqCtx;
    for (auto id : ids) {
        auto it = waitCtx_.find(id);
        if (it != waitCtx_.end()) {
            reqCtx.emplace_back(std::move(it->second.ReqCtx()));
            waitCtx_.erase(it);
        }

        auto wit = timedWaiters_.find(it->second.TimeoutMs());
        if (wit != timedWaiters_.end()) {
            wit->second.erase(it->second.Id());
            if (wit->second.empty()) {
                timedWaiters_.erase(wit);
            }
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
        SM_LOG_DEBUG("WAKEUP REQUEST(" << context.SeqNo() << ").");
        ReplyWithMessage(context, StoreErrorCode::SUCCESS, response);
    }
}

void AccStoreServer::ReplyWithMessage(const ock::acc::AccTcpRequestContext &ctx, int16_t code,
                                      const std::string &message) noexcept
{
    auto response = ock::acc::AccDataBuffer::Create(message.c_str(), message.size());
    if (response == nullptr) {
        SM_LOG_ERROR("create response message failed");
        return;
    }

    ctx.Reply(code, response);
}

void AccStoreServer::ReplyWithMessage(const ock::acc::AccTcpRequestContext &ctx, int16_t code,
                                      const std::vector<uint8_t> &message) noexcept
{
    auto response = ock::acc::AccDataBuffer::Create(message.data(), message.size());
    if (response == nullptr) {
        SM_LOG_ERROR("create response message failed");
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
            SM_LOG_DEBUG("reply timeout response for : " << ctx.SeqNo());
            ReplyWithMessage(ctx, StoreErrorCode::TIMEOUT, "<timeout>");
        }

        lockerGuard.lock();
        storeCond_.wait_for(lockerGuard, std::chrono::milliseconds(1), [this]() { return !running_; });
    }
}
}  // namespace smem
}  // namespace ock