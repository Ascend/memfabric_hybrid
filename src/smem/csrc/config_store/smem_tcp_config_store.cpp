/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include "smem_logger.h"
#include "smem_message_packer.h"
#include "smem_tcp_config_store.h"

namespace ock {
namespace smem {
constexpr auto CONNECT_RETRY_MAX_TIMES = 60;

ClientWaitContext::ClientWaitContext(std::mutex &mtx, std::condition_variable &cond) noexcept
    : waitMutex_{mtx},
      waitCond_{cond},
      finished_{false}
{
}

std::shared_ptr<ock::acc::AccTcpRequestContext> ClientWaitContext::WaitFinished() noexcept
{
    std::unique_lock<std::mutex> locker{waitMutex_};
    waitCond_.wait(locker, [this]() { return finished_; });
    auto copy = responseInfo_;
    locker.unlock();

    return copy;
}

void ClientWaitContext::SetFinished(const ock::acc::AccTcpRequestContext &response) noexcept
{
    std::unique_lock<std::mutex> locker{waitMutex_};
    responseInfo_ = std::make_shared<ock::acc::AccTcpRequestContext>(response);
    finished_ = true;
    locker.unlock();

    waitCond_.notify_one();
}

std::atomic<uint32_t> TcpConfigStore::reqSeqGen_{0};
TcpConfigStore::TcpConfigStore(std::string ip, uint16_t port, bool isServer, int32_t rankId) noexcept
    : serverIp_{std::move(ip)},
      serverPort_{port},
      isServer_{isServer},
      rankId_{rankId}
{}

TcpConfigStore::~TcpConfigStore() noexcept
{
    Shutdown();
}

Result TcpConfigStore::Startup(int reconnectRetryTimes) noexcept
{
    Result result = SM_OK;
    auto retryMaxTimes = reconnectRetryTimes < 0 ? CONNECT_RETRY_MAX_TIMES : reconnectRetryTimes;

    std::lock_guard<std::mutex> guard(mutex_);
    if (accClient_ != nullptr) {
        SM_LOG_WARN("TcpConfigStore already startup");
        return SM_OK;
    }

    accClient_ = ock::acc::AccTcpServer::Create();
    if (accClient_ == nullptr) {
        SM_LOG_ERROR("create acc tcp client failed");
        return SM_ERROR;
    }

    if (isServer_) {
        accServer_ = SmMakeRef<AccStoreServer>(serverIp_, serverPort_);
        if (accServer_ == nullptr) {
            Shutdown();
            return SM_NEW_OBJECT_FAILED;
        }

        if ((result = accServer_->Startup()) != SM_OK) {
            Shutdown();
            return result;
        }
    }

    accClient_->RegisterNewRequestHandler(
        0, [this](const ock::acc::AccTcpRequestContext &context) { return ReceiveResponseHandler(context); });
    accClient_->RegisterLinkBrokenHandler(
        [this](const ock::acc::AccTcpLinkComplexPtr &link) { return LinkBrokenHandler(link); });

    ock::acc::AccTcpServerOptions options;
    options.linkSendQueueSize = ock::acc::UNO_48;
    if ((result = accClient_->Start(options)) != SM_OK) {
        SM_LOG_ERROR("start acc client failed, result: " << result);
        Shutdown();
        return result;
    }

    ock::acc::AccConnReq connReq;
    connReq.rankId = rankId_;
    result = accClient_->ConnectToPeerServer(serverIp_, serverPort_, connReq, retryMaxTimes, accClientLink_);
    if (result != 0) {
        SM_LOG_ERROR("connect to server failed, result: " << result);
        Shutdown();
        return result;
    }

    return SM_OK;
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

Result TcpConfigStore::Set(const std::string &key, const std::vector<uint8_t> &value) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        SM_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::SET};
    request.keys.push_back(key);
    request.values.push_back(value);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        SM_LOG_ERROR("send set for key: " << key << ", get null response");
        return IO_ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        SM_LOG_ERROR("send set for key: " << key << ", get response code: " << responseCode);
    }

    return responseCode;
}

Result TcpConfigStore::GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        SM_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::GET};
    request.keys.push_back(key);
    request.userDef = timeoutMs;

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        SM_LOG_ERROR("send get for key: " << key << ", get null response");
        return -1;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        SM_LOG_ERROR("send get for key: " << key << ", get response code: " << responseCode);
        return responseCode;
    }

    auto data = reinterpret_cast<const uint8_t *>(response->DataPtr());
    auto packedResponse = std::vector<uint8_t>(data, data + response->DataLen());
    SmemMessage responseBody;
    auto ret = SmemMessagePacker::Unpack(packedResponse, responseBody);
    if (ret < 0) {
        SM_LOG_ERROR("unpack response body failed, result: " << ret);
        return -1;
    }

    if (responseBody.values.empty()) {
        SM_LOG_ERROR("response body has no value");
        return -1;
    }

    value = std::move(responseBody.values[0]);
    return 0;
}

Result TcpConfigStore::Add(const std::string &key, int64_t increment, int64_t &value) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        SM_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::ADD};
    request.keys.push_back(key);
    std::string inc = std::to_string(increment);
    request.values.push_back(std::vector<uint8_t>(inc.begin(), inc.end()));

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        SM_LOG_ERROR("send add for key: " << key << ", get null response");
        return StoreErrorCode::ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        SM_LOG_ERROR("send add for key: " << key << ", get response code: " << responseCode);
        return responseCode;
    }

    std::string data(reinterpret_cast<char *>(response->DataPtr()), response->DataLen());
    value = strtol(data.c_str(), nullptr, 10);
    return StoreErrorCode::SUCCESS;
}

Result TcpConfigStore::Remove(const std::string &key) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        SM_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::REMOVE};
    request.keys.push_back(key);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        SM_LOG_ERROR("send remove for key: " << key << ", get null response");
        return StoreErrorCode::ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        SM_LOG_ERROR("send remove for key: " << key << ", get response code: " << responseCode);
        return responseCode;
    }

    return responseCode;
}

Result TcpConfigStore::Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        SM_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::APPEND};
    request.keys.push_back(key);
    request.values.push_back(value);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        SM_LOG_ERROR("send append for key: " << key << ", get null response");
        return StoreErrorCode::ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        SM_LOG_ERROR("send append for key: " << key << ", get response code: " << responseCode);
        return responseCode;
    }

    std::string data(reinterpret_cast<char *>(response->DataPtr()), response->DataLen());
    newSize = strtol(data.c_str(), nullptr, 10);

    return StoreErrorCode::SUCCESS;
}

Result TcpConfigStore::Cas(const std::string &key, const std::vector<uint8_t> &expect,
                           const std::vector<uint8_t> &value, std::vector<uint8_t> &exists) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        SM_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::CAS};
    request.keys.push_back(key);
    request.values.push_back(expect);
    request.values.push_back(value);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        SM_LOG_ERROR("send CAS for key: " << key << ", get null response");
        return StoreErrorCode::ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        SM_LOG_ERROR("send CAS for key: " << key << ", get response code: " << responseCode);
        return responseCode;
    }

    auto data = reinterpret_cast<const uint8_t *>(response->DataPtr());
    auto packedResponse = std::vector<uint8_t>(data, data + response->DataLen());
    SmemMessage responseBody;
    auto ret = SmemMessagePacker::Unpack(packedResponse, responseBody);
    if (ret < 0) {
        SM_LOG_ERROR("unpack response body failed, result: " << ret);
        return -1;
    }

    if (responseBody.values.empty()) {
        SM_LOG_ERROR("response body has no value");
        return -1;
    }

    exists = std::move(responseBody.values[0]);
    return 0;
}

std::shared_ptr<ock::acc::AccTcpRequestContext> TcpConfigStore::SendMessageBlocked(
    const std::vector<uint8_t> &reqBody) noexcept
{
    auto dup = DuplicateMessage(reqBody);
    if (dup == nullptr) {
        SM_LOG_ERROR("duplicate packed message failed");
        return nullptr;
    }

    auto seqNo = reqSeqGen_.fetch_add(1U);
    auto dataBuf = ock::acc::AccDataBuffer::Create(dup, reqBody.size());
    auto ret = accClientLink_->NonBlockSend(0, seqNo, dataBuf, nullptr);
    if (ret != SM_OK) {
        SM_LOG_ERROR("send message failed, result: " << ret);
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

Result TcpConfigStore::LinkBrokenHandler(const ock::acc::AccTcpLinkComplexPtr &link) noexcept
{
    SM_LOG_INFO("link broken, linkId: " << link->Id());
    return SM_OK;
}

Result TcpConfigStore::ReceiveResponseHandler(const ock::acc::AccTcpRequestContext &context) noexcept
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
        return SM_ERROR;
    }

    waitContext->SetFinished(context);
    return SM_OK;
}

uint8_t *TcpConfigStore::DuplicateMessage(const std::vector<uint8_t> &message) noexcept
{
    auto dup = (uint8_t *)malloc(message.size());
    SM_ASSERT_RETURN(dup != nullptr, nullptr);

    memcpy(dup, message.data(), message.size());
    return dup;
}
}  // namespace smem
}  // namespace ock