/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include "smem_logger.h"
#include "smem_message_packer.h"
#include "smem_tcp_config_store.h"

namespace ock {
namespace smem {

ClientWaitContext::ClientWaitContext(std::mutex &mtx, std::condition_variable &cond) noexcept
    : waitMutex_{mtx},
      waitCond_{cond},
      finished{false}
{
}

std::shared_ptr<ock::acc::AccTcpRequestContext> ClientWaitContext::WaitFinished() noexcept
{
    std::unique_lock<std::mutex> locker{waitMutex_};
    waitCond_.wait(locker, [this]() { return finished; });
    auto copy = responseInfo_;
    locker.unlock();

    return copy;
}

void ClientWaitContext::SetFinished(const ock::acc::AccTcpRequestContext &response) noexcept
{
    std::unique_lock<std::mutex> locker{waitMutex_};
    responseInfo_ = std::make_shared<ock::acc::AccTcpRequestContext>(response);
    finished = true;
    locker.unlock();

    waitCond_.notify_one();
}

std::atomic<uint32_t> TcpConfigStore::reqSeqGen_{0};
TcpConfigStore::TcpConfigStore(std::string ip, uint16_t port, bool isServer, int32_t rankId) noexcept
    : serverIp_{std::move(ip)},
      serverPort_{port},
      isServer_{isServer},
      rankId_{rankId}
{
    if (isServer_) {
        accServer_ = std::make_shared<AccStoreServer>(serverIp_, serverPort_);
    }
}

TcpConfigStore::~TcpConfigStore() noexcept
{
    Shutdown();
}

int32_t TcpConfigStore::Startup() noexcept
{
    if (accClient_ != nullptr) {
        SM_LOG_ERROR("TcpConfigStore already startup");
        return -1;
    }

    accClient_ = ock::acc::AccTcpServer::Create();
    if (accClient_ == nullptr) {
        SM_LOG_ERROR("create acc tcp client failed.");
        return -1;
    }

    if (isServer_) {
        accServer_ = std::make_shared<AccStoreServer>(serverIp_, serverPort_);
        auto ret = accServer_->Startup();
        if (ret != 0) {
            Shutdown();
            return ret;
        }
    }

    accClient_->RegisterNewRequestHandler(
        0, [this](const ock::acc::AccTcpRequestContext &context) { return ReceiveResponseHandler(context); });
    accClient_->RegisterLinkBrokenHandler(
        [this](const ock::acc::AccTcpLinkComplexPtr &link) { return LinkBrokenHandler(link); });

    ock::acc::AccTcpServerOptions options;
    options.linkSendQueueSize = ock::acc::UNO_48;
    auto ret = accClient_->Start(options);
    if (ret != 0) {
        SM_LOG_ERROR("start acc client failed: " << ret);
        Shutdown();
        return ret;
    }

    ock::acc::AccConnReq connReq;
    connReq.rankId = rankId_;
    ret = accClient_->ConnectToPeerServer(serverIp_, serverPort_, connReq, accClientLink_);
    if (ret != 0) {
        SM_LOG_ERROR("connect to server failed: " << ret);
        Shutdown();
        return ret;
    }

    return 0;
}

void TcpConfigStore::Shutdown() noexcept
{
    accClientLink_ = nullptr;

    if (accClient_ != nullptr) {
        accClient_->Stop();
        accClient_ = nullptr;
    }

    if (accServer_ != nullptr) {
        accServer_->Shutdown();
        accServer_ = nullptr;
    }
}

int TcpConfigStore::Set(const std::string &key, const std::vector<uint8_t> &value) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        SM_LOG_ERROR("key length invalid.");
        return INVALID_KEY;
    }

    SmemMessage request{MessageType::SET};
    request.keys.push_back(key);
    request.values.push_back(value);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        SM_LOG_ERROR("send set for key : " << key << ", get null response");
        return IO_ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        SM_LOG_ERROR("send set for key : " << key << ", get response code: " << responseCode);
    }

    return responseCode;
}

int TcpConfigStore::GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        SM_LOG_ERROR("key length invalid.");
        return INVALID_KEY;
    }

    SmemMessage request{MessageType::GET};
    request.keys.push_back(key);
    request.userDef = timeoutMs;

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        SM_LOG_ERROR("send get for key : " << key << ", get null response");
        return -1;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        SM_LOG_ERROR("send get for key : " << key << ", get response code: " << responseCode);
        return responseCode;
    }

    auto data = reinterpret_cast<const uint8_t *>(response->DataPtr());
    auto packedResponse = std::vector<uint8_t>(data, data + response->DataLen());
    SmemMessage responseBody;
    auto ret = SmemMessagePacker::Unpack(packedResponse, responseBody);
    if (ret < 0) {
        SM_LOG_ERROR("unpack response body failed: " << ret);
        return -1;
    }

    if (responseBody.values.empty()) {
        SM_LOG_ERROR("response body no values.");
        return -1;
    }

    value = std::move(responseBody.values[0]);
    return 0;
}

int TcpConfigStore::Add(const std::string &key, int64_t increment, int64_t &value) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        SM_LOG_ERROR("key length invalid.");
        return INVALID_KEY;
    }

    SmemMessage request{MessageType::ADD};
    request.keys.push_back(key);
    std::string inc = std::to_string(increment);
    request.values.push_back(std::vector<uint8_t>(inc.begin(), inc.end()));

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        SM_LOG_ERROR("send add for key : " << key << ", get null response");
        return -1;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        SM_LOG_ERROR("send add for key : " << key << ", get response code: " << responseCode);
        return responseCode;
    }

    auto data = reinterpret_cast<const char *>(response->DataPtr());
    value = strtol(data, nullptr, 10);

    return 0;
}

int TcpConfigStore::Remove(const std::string &key) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        SM_LOG_ERROR("key length invalid.");
        return INVALID_KEY;
    }

    SmemMessage request{MessageType::REMOVE};
    request.keys.push_back(key);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        SM_LOG_ERROR("send remove for key : " << key << ", get null response");
        return -1;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        SM_LOG_ERROR("send remove for key : " << key << ", get response code: " << responseCode);
        return responseCode;
    }

    return responseCode;
}

int TcpConfigStore::Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        SM_LOG_ERROR("key length invalid.");
        return INVALID_KEY;
    }

    SmemMessage request{MessageType::APPEND};
    request.keys.push_back(key);
    request.values.push_back(value);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        SM_LOG_ERROR("send append for key : " << key << ", get null response");
        return -1;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        SM_LOG_ERROR("send append for key : " << key << ", get response code: " << responseCode);
        return responseCode;
    }

    auto data = reinterpret_cast<const char *>(response->DataPtr());
    newSize = strtol(data, nullptr, 10);

    return 0;
}

std::shared_ptr<ock::acc::AccTcpRequestContext> TcpConfigStore::SendMessageBlocked(
    const std::vector<uint8_t> &reqBody) noexcept
{
    auto dup = DuplicateMessage(reqBody);
    if (dup == nullptr) {
        SM_LOG_ERROR("dup packed message failed.");
        return nullptr;
    }

    auto seqNo = reqSeqGen_.fetch_add(1U);
    auto dataBuf = ock::acc::AccDataBuffer::Create(dup, reqBody.size());
    auto ret = accClientLink_->NonBlockSend(0, seqNo, dataBuf, nullptr);
    if (ret != 0) {
        SM_LOG_ERROR("send message failed: " << ret);
        return nullptr;
    }

    std::mutex waitRespMutex;
    std::condition_variable waitRespCond;
    auto waitContext = std::make_shared<ClientWaitContext>(waitRespMutex, waitRespCond);

    std::unique_lock<std::mutex> msgCtxLocker{msgCtxMutex_};
    msgWaitContext_.emplace(seqNo, waitContext);
    msgCtxLocker.unlock();

    auto response = waitContext->WaitFinished();
    return response;
}

int32_t TcpConfigStore::LinkBrokenHandler(const ock::acc::AccTcpLinkComplexPtr &link) noexcept
{
    SM_LOG_INFO("link broken: " << link->Id());
    return 0;
}

int32_t TcpConfigStore::ReceiveResponseHandler(const ock::acc::AccTcpRequestContext &context) noexcept
{
    SM_LOG_DEBUG("client received message: " << context.SeqNo());
    std::shared_ptr<ClientWaitContext> waitContext;
    std::unique_lock<std::mutex> msgCtxLocker{msgCtxMutex_};
    auto pos = msgWaitContext_.find(context.SeqNo());
    if (pos != msgWaitContext_.end()) {
        waitContext = std::move(pos->second);
        msgWaitContext_.erase(pos);
    }
    msgCtxLocker.unlock();

    if (waitContext == nullptr) {
        SM_LOG_ERROR("receive response(" << context.SeqNo() << ") not sent request.");
        return -1;
    }

    waitContext->SetFinished(context);
    return 0;
}

uint8_t *TcpConfigStore::DuplicateMessage(const std::vector<uint8_t> &message) noexcept
{
    auto dup = (uint8_t *)malloc(message.size());
    if (dup == nullptr) {
        return nullptr;
    }

    memcpy(dup, message.data(), message.size());
    return dup;
}
}
}