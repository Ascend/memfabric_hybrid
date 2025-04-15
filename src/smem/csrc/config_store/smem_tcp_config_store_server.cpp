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
AccStoreServer::AccStoreServer(std::string ip, uint16_t port) noexcept
    : listenIp_{std::move(ip)},
      listenPort_{port},
      requestHandlers_{{MessageType::SET, &AccStoreServer::SetHandler},
                       {MessageType::GET, &AccStoreServer::GetHandler},
                       {MessageType::ADD, &AccStoreServer::AddHandler},
                       {MessageType::REMOVE, &AccStoreServer::RemoveHandler},
                       {MessageType::APPEND, &AccStoreServer::AppendHandler}}
{
}

int32_t AccStoreServer::Startup() noexcept
{
    if (accTcpServer_ != nullptr) {
        SM_LOG_ERROR("tcp store server already startup");
        return -1;
    }

    accTcpServer_ = ock::acc::AccTcpServer::Create();
    if (accTcpServer_ == nullptr) {
        SM_LOG_ERROR("create acc tcp server failed.");
        return -1;
    }

    accTcpServer_->RegisterNewRequestHandler(
        0, [this](const ock::acc::AccTcpRequestContext &context) { return ReceiveMessageHandler(context); });
    accTcpServer_->RegisterNewLinkHandler(
        [this](const ock::acc::AccConnReq &req, const ock::acc::AccTcpLinkComplexPtr &link) {
            return LinkConnectedHandler(req, link);
        });
    accTcpServer_->RegisterLinkBrokenHandler(
        [this](const ock::acc::AccTcpLinkComplexPtr &link) { return LinkBrokenHandler(link); });

    ock::acc::AccTcpServerOptions options;
    options.listenIp = listenIp_;
    options.listenPort = listenPort_;
    options.enableListener = true;
    options.linkSendQueueSize = ock::acc::UNO_48;
    auto ret = accTcpServer_->Start(options);
    if (ret != 0) {
        SM_LOG_ERROR("startup acc tcp server on port:" << listenPort_ << " failed: " << ret);
        accTcpServer_ = nullptr;
        return -1;
    }

    std::unique_lock<std::mutex> lockGuard{storeMutex_};
    running_ = true;
    lockGuard.unlock();

    timerThread_ = std::thread{[this]() {
        TimerThreadTask();
    }};

    return 0;
}

void AccStoreServer::Shutdown() noexcept
{
    SM_LOG_INFO("BEGIN Shutdown Acc Store Server");
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
    SM_LOG_INFO("FINISH Shutdown Acc Store Server");
}

int32_t AccStoreServer::ReceiveMessageHandler(const ock::acc::AccTcpRequestContext &context) noexcept
{
    auto data = reinterpret_cast<const uint8_t *>(context.DataPtr());
    if (data == nullptr) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle get null request body.");
        ReplyWithMessage(context, ErrorCode::INVALID_MESSAGE, "request no body");
        return -1;
    }

    std::vector<uint8_t> body(data, data + context.DataLen());
    SmemMessage requestMessage;
    auto size = SmemMessagePacker::Unpack(body, requestMessage);
    if (size < 0) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body.");
        ReplyWithMessage(context, ErrorCode::INVALID_MESSAGE, "invalid request");
        return -1;
    }

    auto pos = requestHandlers_.find(requestMessage.mt);
    if (pos == requestHandlers_.end()) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid message type: " << requestMessage.mt);
        ReplyWithMessage(context, ErrorCode::INVALID_MESSAGE, "invalid request message type");
        return -1;
    }

    return (this->*(pos->second))(context, requestMessage);
}

int AccStoreServer::LinkConnectedHandler(const ock::acc::AccConnReq &req,
                                         const ock::acc::AccTcpLinkComplexPtr &link) noexcept
{
    SM_LOG_INFO("new connected link: " << link->Id() << ", rank=" << req.rankId);
    return 0;
}

int32_t AccStoreServer::LinkBrokenHandler(const ock::acc::AccTcpLinkComplexPtr &link) noexcept
{
    SM_LOG_INFO("link broken: " << link->Id());
    return 0;
}

int32_t AccStoreServer::SetHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || request.values.size() != 1) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body.");
        ReplyWithMessage(context, ErrorCode::INVALID_MESSAGE, "invalid request: key value should be one.");
        return -1;
    }

    auto &key = request.keys[0];
    auto &value = request.values[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        SM_LOG_ERROR("key length too large: " << key.length());
        return ErrorCode::INVALID_KEY;
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

    ReplyWithMessage(context, ErrorCode::SUCCESS, "success");
    if (!wakeupWaiters.empty()) {
        WakeupWaiters(wakeupWaiters, reqVal);
    }

    return 0;
}

int32_t AccStoreServer::GetHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || !request.values.empty()) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body.");
        ReplyWithMessage(context, ErrorCode::INVALID_MESSAGE, "invalid request: key should be one and no values.");
        return -1;
    }

    auto &key = request.keys[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        SM_LOG_ERROR("key length too large: " << key.length());
        return ErrorCode::INVALID_KEY;
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
        ReplyWithMessage(context, ErrorCode::SUCCESS, response);
        return 0;
    }

    if (request.userDef == 0) {
        lockGuard.unlock();

        SM_LOG_DEBUG("GET REQUEST(" << context.SeqNo() << ") for key(" << key << ") not exist.");
        ReplyWithMessage(context, ErrorCode::NOT_EXIST, "<not exist>");
        return -1;
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

    return 0;
}

int32_t AccStoreServer::AddHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || request.values.size() != 1) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body.");
        ReplyWithMessage(context, ErrorCode::INVALID_MESSAGE, "invalid request: key value should be one.");
        return -1;
    }

    auto &key = request.keys[0];
    auto &value = request.values[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        SM_LOG_ERROR("key length too large: " << key.length());
        return ErrorCode::INVALID_KEY;
    }

    SM_LOG_DEBUG("ADD REQUEST(" << context.SeqNo() << ") for key(" << key << ") start.");
    std::string valueStr{value.begin(), value.end()};
    auto valueNum = strtol(valueStr.c_str(), nullptr, 10);
    if (valueStr != std::to_string(valueNum)) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") add for key(" << key << ") value is not number");
        ReplyWithMessage(context, ErrorCode::INVALID_MESSAGE, "invalid request: value should be a number.");
        return -1;
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

    ReplyWithMessage(context, ErrorCode::SUCCESS, std::to_string(responseValue));
    if (!wakeupWaiters.empty()) {
        WakeupWaiters(wakeupWaiters, reqVal);
    }
    return 0;
}

int32_t AccStoreServer::RemoveHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || !request.values.empty()) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body.");
        ReplyWithMessage(context, ErrorCode::INVALID_MESSAGE, "invalid request: key should be one and no values.");
        return -1;
    }

    auto &key = request.keys[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        SM_LOG_ERROR("key length too large: " << key.length());
        return ErrorCode::INVALID_KEY;
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
    ReplyWithMessage(context, removed ? ErrorCode::SUCCESS : ErrorCode::NOT_EXIST, removed ? "success" : "not exist");

    return 0;
}

int32_t AccStoreServer::AppendHandler(const ock::acc::AccTcpRequestContext &context, SmemMessage &request) noexcept
{
    if (request.keys.size() != 1 || request.values.size() != 1) {
        SM_LOG_ERROR("request(" << context.SeqNo() << ") handle invalid body.");
        ReplyWithMessage(context, ErrorCode::INVALID_MESSAGE, "invalid request: key & value should be one.");
        return -1;
    }

    auto &key = request.keys[0];
    auto &value = request.values[0];
    if (key.length() > MAX_KEY_LEN_SERVER) {
        SM_LOG_ERROR("key length too large: " << key.length());
        return ErrorCode::INVALID_KEY;
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
    ReplyWithMessage(context, ErrorCode::SUCCESS, std::to_string(newSize));
    if (!wakeupWaiters.empty()) {
        WakeupWaiters(wakeupWaiters, value);
    }

    return 0;
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
        ReplyWithMessage(context, ErrorCode::SUCCESS, response);
    }
}

void AccStoreServer::ReplyWithMessage(const ock::acc::AccTcpRequestContext &ctx, int16_t code,
                                      const std::string &message) noexcept
{
    auto data = (uint8_t *)strdup(message.c_str());
    if (data == nullptr) {
        SM_LOG_ERROR("dup message failed.");
        return;
    }

    ctx.Reply(code, ock::acc::AccDataBuffer::Create(data, message.size()));
}

void AccStoreServer::ReplyWithMessage(const ock::acc::AccTcpRequestContext &ctx, int16_t code,
                                      const std::vector<uint8_t> &message) noexcept
{
    auto data = (uint8_t *)malloc(message.size());
    if (data == nullptr) {
        SM_LOG_ERROR("dup message failed.");
        return;
    }

    memcpy(data, message.data(), message.size());
    ctx.Reply(code, ock::acc::AccDataBuffer::Create(data, message.size()));
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
            ReplyWithMessage(ctx, ErrorCode::TIMEOUT, "<timeout>");
        }

        lockerGuard.lock();
        storeCond_.wait_for(lockerGuard, std::chrono::milliseconds(1), [this]() { return !running_; });
    }
}
}
}