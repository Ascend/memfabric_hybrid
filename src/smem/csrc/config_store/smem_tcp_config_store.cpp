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

#include "smem_tcp_config_store.h"
#include "config_store_log.h"
#include "smem_message_packer.h"
#include "smem_tcp_config_store_ssl_helper.h"
#include "mf_str_util.h"

namespace ock {
namespace smem {
constexpr auto CONNECT_RETRY_MAX_TIMES = 60;
constexpr int DECIMAL_BASE = 10;
constexpr int WORLD_SIZE_SHIFT = 32;

class ClientWaitContext : public ClientCommonContext {
public:
    ClientWaitContext(std::mutex &mtx, std::condition_variable &cond) noexcept
        : waitMutex_{mtx}, waitCond_{cond}, finished_{false}
    {}

    std::shared_ptr<ock::acc::AccTcpRequestContext> WaitFinished() noexcept override
    {
        std::unique_lock<std::mutex> locker{waitMutex_};
        waitCond_.wait(locker, [this]() { return finished_; });
        auto copy = responseInfo_;
        locker.unlock();

        return copy;
    }

    void SetFinished(const ock::acc::AccTcpRequestContext &response) noexcept override
    {
        std::unique_lock<std::mutex> locker{waitMutex_};
        responseInfo_ = std::make_shared<ock::acc::AccTcpRequestContext>(response);
        finished_ = true;
        locker.unlock();

        waitCond_.notify_one();
    }

    void SetFailedFinish() noexcept override
    {
        std::unique_lock<std::mutex> locker{waitMutex_};
        finished_ = true;
        responseInfo_ = nullptr;
        locker.unlock();

        waitCond_.notify_one();
    }
    bool Blocking() const noexcept override
    {
        return true;
    }

private:
    std::mutex &waitMutex_;
    std::condition_variable &waitCond_;
    bool finished_;
    std::shared_ptr<ock::acc::AccTcpRequestContext> responseInfo_;
};

class ClientWatchContext : public ClientCommonContext {
public:
    explicit ClientWatchContext(std::function<void(int, const std::vector<uint8_t> &)> nfy,
                                bool oneTime = true) noexcept
        : notify_{std::move(nfy)}, onlyOneTime_{oneTime}
    {}

    std::shared_ptr<ock::acc::AccTcpRequestContext> WaitFinished() noexcept override
    {
        return nullptr;
    }

    void SetFinished(const ock::acc::AccTcpRequestContext &response) noexcept override
    {
        auto data = reinterpret_cast<const uint8_t *>(response.DataPtr());
        if (data == nullptr) {
            STORE_LOG_ERROR("data is nullptr.");
            return;
        }

        SmemMessage responseBody;
        auto ret = SmemMessagePacker::Unpack(data, response.DataLen(), responseBody);
        if (ret < 0) {
            STORE_LOG_ERROR("unpack response body failed, result: " << ret);
            notify_(IO_ERROR, std::vector<uint8_t>{});
            return;
        }

        if (responseBody.values.empty()) {
            STORE_LOG_ERROR("response body has no value");
            notify_(IO_ERROR, std::vector<uint8_t>{});
            return;
        }

        STORE_LOG_DEBUG("watch end, id: " << response.SeqNo());
        notify_(SUCCESS, responseBody.values[0]);
    }

    void SetFailedFinish() noexcept override
    {
        notify_(IO_ERROR, std::vector<uint8_t>{});
    }

    bool Blocking() const noexcept override
    {
        return false;
    }

    bool OnlyOneTime() const noexcept override
    {
        return onlyOneTime_;
    }

private:
    const std::function<void(int result, const std::vector<uint8_t> &)> notify_;
    const bool onlyOneTime_;
};

std::atomic<uint32_t> TcpConfigStore::reqSeqGen_{0};
TcpConfigStore::TcpConfigStore(std::string ip, uint16_t port, bool isServer, uint32_t worldSize,
                               int32_t rankId) noexcept
    : serverIp_{std::move(ip)}, serverPort_{port}, isServer_{isServer}, worldSize_{worldSize}, rankId_{rankId}
{}

TcpConfigStore::~TcpConfigStore() noexcept
{
    Shutdown();
}

Result TcpConfigStore::Startup(const smem_tls_config &tlsConfig, int reconnectRetryTimes) noexcept
{
    Result result = SM_OK;
    if (isServer_) {
        result = ServerStart(tlsConfig, reconnectRetryTimes);
        if (result != 0) {
            STORE_LOG_ERROR("Failed to start config store server ret:" << result);
            return result;
        }
    }

    result = ClientStart(tlsConfig, reconnectRetryTimes);
    if (result != 0) {
        STORE_LOG_ERROR("Failed to start config store client ret:" << result);
        return result;
    }
    isConnect_.store(true);
    return result;
}

Result TcpConfigStore::ClientStart(const smem_tls_config &tlsConfig, int reconnectRetryTimes) noexcept
{
    Result result = SM_OK;
    auto retryMaxTimes = reconnectRetryTimes < 0 ? CONNECT_RETRY_MAX_TIMES : reconnectRetryTimes;

    std::lock_guard<std::mutex> guard(mutex_);
    if (accClient_ != nullptr) {
        STORE_LOG_WARN("TcpConfigStore already startup");
        return SM_OK;
    }

    accClient_ = ock::acc::AccTcpServer::Create();
    if (accClient_ == nullptr) {
        STORE_LOG_ERROR("create acc tcp client failed");
        return SM_ERROR;
    }
    if (tlsConfig.tlsEnable && PrepareTlsForAccTcpServer(accClient_, tlsConfig) != SM_OK) {
        STORE_LOG_ERROR("Failed to prepare TLS for acc client");
    }

    accClient_->RegisterNewRequestHandler(
        0, [this](const ock::acc::AccTcpRequestContext &context) { return ReceiveResponseHandler(context); });
    accClient_->RegisterLinkBrokenHandler(
        [this](const ock::acc::AccTcpLinkComplexPtr &link) { return LinkBrokenHandler(link); });

    ock::acc::AccTcpServerOptions options;
    options.linkSendQueueSize = ock::acc::UNO_48;
    if ((result = accClient_->Start(options, GetAccTlsOption(tlsConfig))) != SM_OK) {
        STORE_LOG_ERROR("start acc client failed, result: " << result);
        Shutdown();
        return result;
    }

    ock::acc::AccConnReq connReq;
    connReq.rankId =
        rankId_ >= 0 ? ((static_cast<uint64_t>(worldSize_) << WORLD_SIZE_SHIFT) | static_cast<uint64_t>(rankId_))
                     : ((static_cast<uint64_t>(worldSize_) << WORLD_SIZE_SHIFT) | std::numeric_limits<uint32_t>::max());
    result = accClient_->ConnectToPeerServer(serverIp_, serverPort_, connReq, retryMaxTimes, accClientLink_);
    if (result != 0) {
        STORE_LOG_ERROR("connect to server failed, result: " << result);
        Shutdown();
        return result;
    }
    isRunning_.store(true);
    heartBeatThread_ = std::thread{[this]() { HeartBeat(); }};
    return result;
}

Result TcpConfigStore::ServerStart(const smem_tls_config &tlsConfig, int reconnectRetryTimes) noexcept
{
    Result result = SM_OK;
    std::lock_guard<std::mutex> guard(mutex_);
    accServer_ = SmMakeRef<AccStoreServer>(serverIp_, serverPort_, worldSize_);
    if (accServer_ == nullptr) {
        Shutdown();
        return SM_NEW_OBJECT_FAILED;
    }

    if ((result = accServer_->Startup(tlsConfig)) != SM_OK) {
        Shutdown();
        return result;
    }
    return result;
}

void TcpConfigStore::Shutdown(bool afterFork) noexcept
{
    isRunning_.store(false);
    if (heartBeatThread_.joinable()) {
        heartBeatThread_.join();
    }
    accClientLink_ = nullptr;

    if (accClient_ != nullptr) {
        if (afterFork) {
            accClient_->StopAfterFork();
        } else {
            accClient_->Stop();
        }
        accClient_ = nullptr;
    }

    if (accServer_ != nullptr) {
        accServer_->Shutdown(afterFork);
        accServer_ = nullptr;
    }
}

Result TcpConfigStore::Set(const std::string &key, const std::vector<uint8_t> &value) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        STORE_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::SET};
    request.keys.push_back(key);
    request.values.push_back(value);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        STORE_LOG_ERROR("send set for key: " << key << ", get null response");
        return IO_ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        STORE_LOG_ERROR("send set for key: " << key << ", get response code: " << responseCode);
    }

    return responseCode;
}

Result TcpConfigStore::GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        STORE_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::GET};
    request.keys.push_back(key);
    request.userDef = timeoutMs;

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        STORE_LOG_ERROR("send get for key: " << key << ", get null response");
        return IO_ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0 && responseCode != RESTORE) {
        if (responseCode != NOT_EXIST) {
            STORE_LOG_WARN("send get for key: " << key << ", resp code: " << responseCode << " timeout:" << timeoutMs);
        }
        return responseCode;
    }

    auto data = reinterpret_cast<const uint8_t *>(response->DataPtr());
    STORE_ASSERT_RETURN(data != nullptr, SM_MALLOC_FAILED);
    SmemMessage responseBody;
    auto ret = SmemMessagePacker::Unpack(data, response->DataLen(), responseBody);
    if (ret < 0) {
        STORE_LOG_ERROR("unpack response body failed, result: " << ret);
        return -1;
    }

    if (responseBody.values.empty()) {
        STORE_LOG_ERROR("response body has no value");
        return -1;
    }

    value = std::move(responseBody.values[0]);
    return static_cast<Result>(responseCode);
}

Result TcpConfigStore::Add(const std::string &key, int64_t increment, int64_t &value) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        STORE_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::ADD};
    request.keys.push_back(key);
    std::string inc = std::to_string(increment);
    request.values.push_back(std::vector<uint8_t>(inc.begin(), inc.end()));

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        STORE_LOG_ERROR("send add for key: " << key << ", get null response");
        return StoreErrorCode::IO_ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        STORE_LOG_ERROR("send add for key: " << key << ", get response code: " << responseCode);
        return responseCode;
    }
    STORE_ASSERT_RETURN(response->DataPtr() != nullptr, IO_ERROR);
    std::string data(reinterpret_cast<char *>(response->DataPtr()), response->DataLen());

    value = 0;
    auto ret = mf::StrUtil::String2Int<int64_t>(data, value);
    STORE_ASSERT_RETURN(errno != ERANGE, IO_ERROR);
    if ((value == 0 && data != "0") || !ret) {
        STORE_LOG_ERROR("data=" << data);
        return StoreErrorCode::ERROR;
    }
    return StoreErrorCode::SUCCESS;
}

Result TcpConfigStore::Remove(const std::string &key, bool printKeyNotExist) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        STORE_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::REMOVE};
    request.keys.push_back(key);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        STORE_LOG_ERROR("send remove for key: " << key << ", get null response");
        return StoreErrorCode::IO_ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode == StoreErrorCode::NOT_EXIST) {
        if (printKeyNotExist) {
            // Do not need to print ERROR message when sending remove key which is already not exist.
            STORE_LOG_INFO("send remove for key: " << key << ", is already not exist");
        }
    } else if (responseCode != 0) {
        STORE_LOG_ERROR("send remove for key: " << key << ", get response code: " << responseCode);
    }

    return responseCode;
}

Result TcpConfigStore::Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        STORE_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::APPEND};
    request.keys.push_back(key);
    request.values.push_back(value);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        STORE_LOG_ERROR("send append for key: " << key << ", get null response");
        return StoreErrorCode::IO_ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        STORE_LOG_ERROR("send append for key: " << key << ", get response code: " << responseCode);
        return responseCode;
    }
    STORE_ASSERT_RETURN(response->DataPtr() != nullptr, IO_ERROR);
    std::string data(reinterpret_cast<char *>(response->DataPtr()), response->DataLen());

    long tmpValue = 0;
    STORE_VALIDATE_RETURN(mf::StrUtil::String2Int<long>(data, tmpValue), "convert string to long failed.",
                          StoreErrorCode::ERROR);
    newSize = static_cast<uint64_t>(tmpValue);

    return StoreErrorCode::SUCCESS;
}

Result TcpConfigStore::Write(const std::string &key, const std::vector<uint8_t> &value, const uint32_t offset) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        STORE_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::WRITE};
    std::vector<uint8_t> sendValue(reinterpret_cast<const uint8_t *>(&offset),
                                   reinterpret_cast<const uint8_t *>(&offset) + sizeof(offset));
    sendValue.insert(sendValue.end(), value.begin(), value.end());
    request.keys.push_back(key);
    request.values.push_back(sendValue);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        STORE_LOG_ERROR("send set for key: " << key << ", get null response");
        return IO_ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        STORE_LOG_ERROR("send set for key: " << key << ", get response code: " << responseCode);
    }

    return responseCode;
}

Result TcpConfigStore::Cas(const std::string &key, const std::vector<uint8_t> &expect,
                           const std::vector<uint8_t> &value, std::vector<uint8_t> &exists) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        STORE_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::CAS};
    request.keys.push_back(key);
    request.values.push_back(expect);
    request.values.push_back(value);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto response = SendMessageBlocked(packedRequest);
    if (response == nullptr) {
        STORE_LOG_ERROR("send CAS for key: " << key << ", get null response");
        return StoreErrorCode::IO_ERROR;
    }

    auto responseCode = response->Header().result;
    if (responseCode != 0) {
        STORE_LOG_ERROR("send CAS for key: " << key << ", get response code: " << responseCode);
        return responseCode;
    }

    auto data = reinterpret_cast<const uint8_t *>(response->DataPtr());
    STORE_ASSERT_RETURN(data != nullptr, SM_MALLOC_FAILED);
    SmemMessage responseBody;
    auto ret = SmemMessagePacker::Unpack(data, response->DataLen(), responseBody);
    if (ret < 0) {
        STORE_LOG_ERROR("unpack response body failed, result: " << ret);
        return -1;
    }

    if (responseBody.values.empty()) {
        STORE_LOG_ERROR("response body has no value");
        return -1;
    }

    exists = std::move(responseBody.values[0]);
    return 0;
}

Result
TcpConfigStore::Watch(const std::string &key,
                      const std::function<void(int result, const std::string &, const std::vector<uint8_t> &)> &notify,
                      uint32_t &wid) noexcept
{
    if (key.empty() || key.length() > MAX_KEY_LEN_CLIENT) {
        STORE_LOG_ERROR("key length is invalid");
        return StoreErrorCode::INVALID_KEY;
    }

    SmemMessage request{MessageType::GET};
    request.keys.push_back(key);

    auto packedRequest = SmemMessagePacker::Pack(request);
    auto ret = SendWatchRequest(
        packedRequest, [key, notify](int res, const std::vector<uint8_t> &value) { notify(res, key, value); }, wid);
    if (ret != SM_OK) {
        STORE_LOG_ERROR_LIMIT("send get for key: " << key << ", get null response");
        return ret;
    }

    STORE_LOG_DEBUG("watch for key: " << key << ", id: " << wid);
    return SM_OK;
}

Result TcpConfigStore::Watch(WatchRankType type, const std::function<void(WatchRankType, uint32_t)> &notify,
                             uint32_t &wid) noexcept
{
    if (type != WATCH_RANK_LINK_DOWN) {
        STORE_LOG_ERROR("invalid watch rank type: " << type);
        return SM_INVALID_PARAM;
    }

    SmemMessage request{MessageType::WATCH_RANK_STATE};
    request.keys.emplace_back(WATCH_RANK_DOWN_KEY);
    auto packedRequest = SmemMessagePacker::Pack(request);
    auto ret = SendWatchRequest(
        packedRequest,
        [notify](int res, const std::vector<uint8_t> &value) {
            if (res == SM_OK && value.size() == sizeof(uint32_t)) {
                notify(WATCH_RANK_LINK_DOWN, *(const uint32_t *)(const void *)value.data());
            }
        },
        wid);
    if (ret != SM_OK) {
        STORE_LOG_ERROR("send watch for rank down get null response");
        return ret;
    }

    STORE_LOG_DEBUG("send watch for rank down success, id: " << wid);
    return SM_OK;
}

Result TcpConfigStore::Unwatch(uint32_t wid) noexcept
{
    std::shared_ptr<ClientCommonContext> watchContext;

    std::unique_lock<std::mutex> msgCtxLocker{msgCtxMutex_};
    auto pos = msgClientContext_.find(wid);
    if (pos != msgClientContext_.end() && !pos->second->Blocking()) {
        watchContext = std::move(pos->second);
        msgClientContext_.erase(pos);
    }
    msgCtxLocker.unlock();

    if (watchContext == nullptr) {
        STORE_LOG_WARN("unwatch for id: " << wid << ", not exist.");
        return NOT_EXIST;
    }

    STORE_LOG_INFO("unwatch for id: " << wid << " success.");
    return SM_OK;
}

std::shared_ptr<ock::acc::AccTcpRequestContext>
TcpConfigStore::SendMessageBlocked(const std::vector<uint8_t> &reqBody) noexcept
{
    auto seqNo = reqSeqGen_.fetch_add(1U);
    auto dataBuf = ock::acc::AccDataBuffer::Create(reqBody.data(), reqBody.size());
    STORE_ASSERT_RETURN(accClientLink_ != nullptr, nullptr);
    std::mutex waitRespMutex;
    std::condition_variable waitRespCond;
    auto waitContext = std::make_shared<ClientWaitContext>(waitRespMutex, waitRespCond);
    STORE_ASSERT_RETURN(waitContext != nullptr, nullptr);
    std::unique_lock<std::mutex> msgCtxLocker{msgCtxMutex_};
    msgClientContext_.emplace(seqNo, waitContext);
    msgCtxLocker.unlock();
    auto ret = LocalNonBlockSend(0, seqNo, dataBuf, nullptr);
    if (ret != SM_OK) {
        STORE_LOG_ERROR("send message failed, result: " << ret);
        return nullptr;
    }

    auto response = waitContext->WaitFinished();
    return response;
}

Result TcpConfigStore::ReConnectAfterBroken(int reconnectRetryTimes) noexcept
{
    auto retryMaxTimes = reconnectRetryTimes < 0 ? CONNECT_RETRY_MAX_TIMES : reconnectRetryTimes;
    ock::acc::AccConnReq connReq;
    connReq.rankId = rankId_;
    auto result = accClient_->ConnectToPeerServer(serverIp_, serverPort_, connReq, retryMaxTimes, accClientLink_);
    if (result != 0) {
        STORE_LOG_ERROR_LIMIT("Reconnect to server failed, result.");
        return result;
    }
    STORE_LOG_INFO("Reconnect to server successful.");
    return SM_OK;
}

bool TcpConfigStore::GetConnectStatus() noexcept
{
    return isConnect_.load();
}

void TcpConfigStore::SetConnectStatus(bool status) noexcept
{
    isConnect_.store(status);
}

void TcpConfigStore::RegisterClientBrokenHandler(const ConfigStoreClientBrokenHandler &handler) noexcept
{
    brokenHandler_ = std::move(handler);
}

void TcpConfigStore::RegisterServerBrokenHandler(const ConfigStoreServerBrokenHandler &handler) noexcept
{
    if (accServer_ == nullptr) {
        STORE_LOG_INFO("accServer_ is null, cannot register server broken handler");
        return;
    }
    accServer_->RegisterBrokenLinkCHandler(handler);
}

void TcpConfigStore::RegisterServerOpHandler(int16_t opCode, const ConfigStoreServerOpHandler &handler) noexcept
{
    if (accServer_ == nullptr) {
        STORE_LOG_INFO("accServer_ is null, cannot register server op handler");
        return;
    }
    accServer_->RegisterOpHandler(opCode, handler);
}

Result TcpConfigStore::LinkBrokenHandler(const ock::acc::AccTcpLinkComplexPtr &link) noexcept
{
    STORE_LOG_WARN("link broken, linkId: " << link->Id());
    SetConnectStatus(false);
    if (brokenHandler_ != nullptr) {
        brokenHandler_();
    }
    std::unordered_map<uint32_t, std::shared_ptr<ClientCommonContext>> tempContext;
    std::unique_lock<std::mutex> msgCtxLocker{msgCtxMutex_};
    tempContext.swap(msgClientContext_);
    msgCtxLocker.unlock();

    for (auto &it : tempContext) {
        it.second->SetFailedFinish();
    }

    return SM_OK;
}

Result TcpConfigStore::ReceiveResponseHandler(const ock::acc::AccTcpRequestContext &context) noexcept
{
    STORE_LOG_DEBUG("client received message: " << context.SeqNo());
    std::shared_ptr<ClientCommonContext> clientContext;

    std::unique_lock<std::mutex> msgCtxLocker{msgCtxMutex_};
    auto pos = msgClientContext_.find(context.SeqNo());
    if (pos != msgClientContext_.end()) {
        if (pos->second->OnlyOneTime()) {
            clientContext = std::move(pos->second);
            msgClientContext_.erase(pos);
        } else {
            clientContext = pos->second;
        }
    }
    msgCtxLocker.unlock();

    if (clientContext == nullptr) {
        STORE_LOG_WARN("receive response(" << context.SeqNo() << ") not sent request.");
        return SM_ERROR;
    }

    clientContext->SetFinished(context);
    return SM_OK;
}

Result TcpConfigStore::SendWatchRequest(const std::vector<uint8_t> &reqBody,
                                        const std::function<void(int result, const std::vector<uint8_t> &)> &notify,
                                        uint32_t &id) noexcept
{
    auto seqNo = reqSeqGen_.fetch_add(1U);
    auto dataBuf = ock::acc::AccDataBuffer::Create(reqBody.data(), reqBody.size());
    STORE_ASSERT_RETURN(accClientLink_ != nullptr, SM_NOT_INITIALIZED);
    auto watchContext = std::make_shared<ClientWatchContext>(notify, false);
    STORE_ASSERT_RETURN(watchContext != nullptr, SM_MALLOC_FAILED);
    std::unique_lock<std::mutex> msgCtxLocker{msgCtxMutex_};
    msgClientContext_.emplace(seqNo, std::move(watchContext));
    msgCtxLocker.unlock();
    auto ret = LocalNonBlockSend(0, seqNo, dataBuf, nullptr);
    if (ret != SM_OK) {
        STORE_LOG_ERROR_LIMIT("send message failed, result: " << ret
                                                              << ", Established: " << accClientLink_->Established());
        return ret;
    }

    id = seqNo;
    return SM_OK;
}

void TcpConfigStore::HeartBeat() noexcept
{
    while (isRunning_.load()) {
        if (isConnect_.load()) {
            SmemMessage request{MessageType::HEARTBEAT};
            auto packedRequest = SmemMessagePacker::Pack(request);
            auto dataBuf = ock::acc::AccDataBuffer::Create(packedRequest.data(), packedRequest.size());
            if (dataBuf == nullptr) {
                STORE_LOG_ERROR("create data buffer falied, no enough mem");
                continue;
            }
            auto ret = LocalNonBlockSend(0, 0, dataBuf, nullptr);
            if (ret != SM_OK) {
                STORE_LOG_ERROR("send message failed, result: " << ret);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_INTERVAL));
    }

    STORE_LOG_INFO("TcpConfigStore heart beat thread exit.");
}

} // namespace smem
} // namespace ock