/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "acc_tcp_server.h"
#include "acc_tcp_server_default.h"
#include "acc_common_util.h"

namespace ock {
namespace acc {
template <class T>
class AtmoicRollback {
public:
    AtmoicRollback(std::atomic<T> &value, T failedValue) : value_(value), failedValue_(failedValue)
    {}

    ~AtmoicRollback()
    {
        if (!success_) {
            value_ = failedValue_;
        }
    }

    void SetSuccess(bool val)
    {
        success_ = val;
    }

private:
    bool success_ = true;
    std::atomic<T> &value_;
    T failedValue_;
};

Result AccTcpServerDefault::Start(const AccTcpServerOptions &opt, const AccTlsOption &tlsOption)
{
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true)) {
        return ACC_OK;
    }

    AtmoicRollback<bool> rollback{started_, false};
    rollback.SetSuccess(false);
    options_ = opt;
    tlsOption_ = tlsOption;

    auto result = ValidateOptions();
    LOG_ERROR_RETURN_IT_IF_NOT_OK(result, "Failed to start AccTcpServerDefault as options are invalid");

    maxWorkerLinkeCnt_ = options_.maxWorldSize;

    result = ValidateHandler();
    LOG_ERROR_RETURN_IT_IF_NOT_OK(result, "Failed to start AccTcpServerDefault as handler are invalid");

    result = GenerateSslCtx();
    LOG_ERROR_RETURN_IT_IF_NOT_OK(result, "Failed to generate ssl ctx as " << result);

    result = StartDelayCleanup();
    LOG_ERROR_RETURN_IT_IF_NOT_OK(result, "Failed to start AccTcpServerDefault delay cleanup");

    /* start workers firstly, in case of connecting comes just after listener started */
    result = StartWorkers();
    if (result != ACC_OK) {
        StopAndCleanDelayCleanup();
        StopAndCleanWorkers();
        LOG_ERROR("Failed to start AccTcpServerDefault workers");
        return result;
    }

    /* start listener secondly */
    result = StartListener();
    if (result != ACC_OK) {
        StopAndCleanDelayCleanup();
        StopAndCleanWorkers();
        LOG_ERROR("Failed to start AccTcpServerDefault listener");
        return result;
    }

    rollback.SetSuccess(true);
    return ACC_OK;
}

Result AccTcpServerDefault::LoadDynamicLib(const std::string &dynLibPath)
{
    auto ret = OpenSslApiWrapper::Load(dynLibPath);
    if (ret != ACC_OK) {
        LOG_ERROR("load open ssl failed");
        return ACC_ERROR;
    }
    return ACC_OK;
}

void AccTcpServerDefault::Stop()
{
    bool expected = true;
    if (!started_.compare_exchange_strong(expected, false)) {
        return;
    }

    /* stop listener firstly */
    StopAndCleanListener();
    /* stop workers secondly */
    StopAndCleanWorkers();
    /* stop delay cleanup */
    StopAndCleanDelayCleanup();

    StopAndCleanSSLHelper();
}

void AccTcpServerDefault::StopAfterFork()
{
    bool expected = true;
    if (!started_.compare_exchange_strong(expected, false)) {
        return;
    }

    /* stop listener firstly */
    StopAndCleanListener(true);
    /* stop workers secondly */
    StopAndCleanWorkers(true);
    /* stop delay cleanup */
    StopAndCleanDelayCleanup(true);

    StopAndCleanSSLHelper();
}

Result AccTcpServerDefault::ValidateOptions() const
{
    if (options_.enableListener) {
        if (options_.listenIp.empty()) {
            LOG_ERROR("Invalid listen ip as it is empty");
            return ACC_INVALID_PARAM;
        }

        if (options_.listenPort == 0) {
            LOG_ERROR("Invalid listen port as it should not be 0");
            return ACC_INVALID_PARAM;
        }
    }

    if (options_.workerCount > UNO_256 || options_.workerCount == 0) {
        LOG_ERROR("Invalid worker count as it should be between 1 and 256");
        return ACC_INVALID_PARAM;
    }

    if (options_.workerStartCpuId < -1) {
        LOG_ERROR("Invalid worker start cpu Id as it should not be smaller than -1");
        return ACC_INVALID_PARAM;
    }

    if (options_.linkSendQueueSize < UNO_32) {
        LOG_ERROR("Invalid send queue size of link as it should not be smaller than 32");
        return ACC_INVALID_PARAM;
    }

    if (options_.keepaliveIdleTime == 0) {
        LOG_ERROR("Invalid keepalive idle time as it should not be 0");
        return ACC_INVALID_PARAM;
    }

    if (options_.keepaliveProbeTimes == 0) {
        LOG_ERROR("Invalid keepalive probe times as it should not be 0");
        return ACC_INVALID_PARAM;
    }

    if (options_.keepaliveProbeInterval == 0) {
        LOG_ERROR("Invalid keepalive probe interval as it should not be 0");
        return ACC_INVALID_PARAM;
    }

    if (options_.maxWorldSize == 0) {
        LOG_ERROR("Invalid max number of links per worker as it should be bigger than 0");
        return ACC_INVALID_PARAM;
    }

    std::string error;
    if (!tlsOption_.ValidateOption(error)) {
        LOG_ERROR("Invalid tls option as " << error);
        return ACC_INVALID_PARAM;
    }

    return ACC_OK;
}

Result AccTcpServerDefault::ValidateHandler() const
{
    int16_t handlerCount = 0;
    for (auto &item : newRequestHandle_) {
        if (item != nullptr) {
            handlerCount++;
        }
    }

    if (handlerCount == 0) {
        LOG_ERROR("Invalid param, no newRequestHandler is not registered");
        return ACC_INVALID_PARAM;
    }

    for (auto &item : requestSentHandle_) {
        if (item != nullptr) {
            handlerCount++;
        }
    }

    if (handlerCount == 0) {
        LOG_WARN("Invalid param, no requestSentHandler is not registered");
    }

    if (linkBrokenHandle_ == nullptr) {
        LOG_ERROR("Invalid param, link broken handler is not set");
        return ACC_INVALID_PARAM;
    }

    return ACC_OK;
}

Result AccTcpServerDefault::StartWorkers()
{
    AccTcpWorkerOptions workerOptions;
    workerOptions.threadPriority = options_.workerThreadPriority;
    workerOptions.cpuId = -1;
    workerOptions.pollingTimeoutMs = options_.workerPollTimeoutMs;
    for (uint16_t i = 0; i < options_.workerCount; i++) {
        if (options_.workerStartCpuId != -1) {
            workerOptions.cpuId = options_.workerStartCpuId + i;
        }
        workerOptions.index = i;

        AccTcpWorkerPtr tmpWorker = new (std::nothrow) AccTcpWorker(workerOptions);
        ASSERT_RETURN(tmpWorker.Get() != nullptr, ACC_NEW_OBJECT_FAIL);
        tmpWorker->RegisterNewRequestHandler(
            std::bind(&AccTcpServerDefault::HandleNewRequest, this, std::placeholders::_1));
        tmpWorker->RegisterRequestSentHandler(std::bind(&AccTcpServerDefault::HandleRequestSent, this,
                                                        std::placeholders::_1, std::placeholders::_2,
                                                        std::placeholders::_3));
        tmpWorker->RegisterLinkBrokenHandler(
            std::bind(&AccTcpServerDefault::HandleLinkBroken, this, std::placeholders::_1));
        workers_.push_back(tmpWorker);
    }

    for (auto &item : workers_) {
        auto result = item->Start();
        if (result != ACC_OK) {
            StopAndCleanWorkers();
            return result;
        }
    }

    return ACC_OK;
}

void AccTcpServerDefault::StopAndCleanWorkers(bool afterFork)
{
    if (afterFork) {
        for (auto &item : connectedLinks_) {
            item.second->DecreaseRef();
        }
    } else {
        std::lock_guard<std::mutex> guard(mutex_);
        // HandleNewConnection时引用计数+1,未linkdown,计数不会自动-1,这里手动-1
        for (auto &item: connectedLinks_) {
            item.second->DecreaseRef();
        }
    }

    for (auto &item : workers_) {
        item->Stop();
    }
    workers_.clear();
    connectedLinks_.clear();
}

Result AccTcpServerDefault::StartListener()
{
    if (!options_.enableListener) {
        return ACC_OK;
    }

    AccTcpListenerPtr tmpListener = new (std::nothrow)
        AccTcpListener(options_.listenIp, options_.listenPort, options_.reusePort, tlsOption_.enableTls, sslCtx_);
    ASSERT_RETURN(tmpListener.Get() != nullptr, ACC_NEW_OBJECT_FAIL);

    tmpListener->RegisterNewConnectionHandler(
        std::bind(&AccTcpServerDefault::HandleNewConnection, this, std::placeholders::_1, std::placeholders::_2));

    auto result = tmpListener->Start();
    if (result != ACC_OK) {
        return result;
    }

    listener_ = tmpListener;
    return ACC_OK;
}

void AccTcpServerDefault::StopAndCleanListener(bool afterFork)
{
    if (listener_ == nullptr) {
        return;
    }

    listener_->Stop(afterFork);
    listener_ = nullptr;
}

void AccTcpServerDefault::StopAndCleanSSLHelper(bool afterFork)
{
    if (sslHelper_ == nullptr) {
        return;
    }

    sslHelper_->Stop(afterFork);
    sslHelper_ = nullptr;
}

Result AccTcpServerDefault::StartDelayCleanup()
{
    if (delayCleanup_ != nullptr) {
        return true;
    }

    AccTcpLinkDelayCleanupPtr tmpClean = new (std::nothrow) AccTcpLinkDelayCleanup();
    ASSERT_RETURN(tmpClean != nullptr, ACC_NEW_OBJECT_FAIL);

    auto result = tmpClean->Start();
    if (result != ACC_OK) {
        return result;
    }

    delayCleanup_ = tmpClean;
    return ACC_OK;
}

void AccTcpServerDefault::StopAndCleanDelayCleanup(bool afterFork)
{
    if (delayCleanup_ == nullptr) {
        return;
    }

    delayCleanup_->Stop(afterFork);
    delayCleanup_ = nullptr;
}

Result AccTcpServerDefault::HandleNewConnection(const AccConnReq &req, const AccTcpLinkComplexDefaultPtr &newLink)
{
    ASSERT_RETURN(newLink.Get() != nullptr, ACC_INVALID_PARAM);
    if (req.magic != options_.magic) {
        LOG_ERROR("New link connected but magic mismatched, refuse the link from " << newLink->ShortName());
        return ACC_ERROR;
    }

    if (req.version != options_.version) {
        LOG_ERROR("New link connected but version mismatched, refuse the link from " << newLink->ShortName());
        return ACC_ERROR;
    }

    auto workIndex = WorkerSelect();
    if (workIndex == ACC_ERROR) {
        LOG_ERROR("Failed to select available worker.");
        return ACC_ERROR;
    }

    auto &worker = workers_[workIndex];
    auto result = newLink->Initialize(options_.linkSendQueueSize, workIndex, worker.Get());
    if (UNLIKELY(result != ACC_OK)) {
        LOG_ERROR("Failed to initialize the link from " << newLink->ShortName() << ", result " << result);
        return ACC_ERROR;
    }

    result = newLinkHandle_(req, newLink.Get());
    if (UNLIKELY(result != ACC_OK)) {
        return result;
    }

    newLink->EnableNoBlocking();
    {
        /* check and add new link into map */
        std::lock_guard<std::mutex> guard(mutex_);
        if (!started_) {
            LOG_WARN("The server is being destroyed or has been destroyed. can't receive new connection.");
            return ACC_ERROR;
        }
        auto iter = connectedLinks_.find(newLink->Id());
        if (iter != connectedLinks_.end()) {
            LOG_ERROR("Failed to handle new connection as found duplicated link id " << newLink->Id());
            return ACC_ERROR;
        }

        /* added to worker */
        result = worker->AddLink(newLink, EPOLLIN | EPOLLOUT | EPOLLET);
        if (UNLIKELY(result != ACC_OK)) {
            return result;
        }

        /* emplace map */
        connectedLinks_.emplace(newLink->Id(), newLink);
    }

    return ACC_OK;
}

Result AccTcpServerDefault::WorkerSelect()
{
    auto workerSize = workers_.size();
    nextWorkerIndex_.fetch_add(1, std::memory_order_relaxed);
    if (workerSize == 0) {
        return ACC_ERROR;
    }
    auto workIndex = nextWorkerIndex_ % workerSize;
    for (uint32_t i = 0; i < workers_.size(); i++) {
        if (WorkerLinkLimitCheck(workIndex)) {
            return workIndex;
        } else {
            workIndex += 1;
            workIndex = workIndex % workerSize;
        }
    }
    LOG_ERROR("All workers reached the link load maximum.");
    return ACC_ERROR;
}

bool AccTcpServerDefault::WorkerLinkLimitCheck(uint32_t workerIdx)
{
    std::unique_lock<std::mutex> lockGuard{ linkCntMutex };

    auto it = workerLinkCnt_.find(workerIdx);
    if (it == workerLinkCnt_.end()) {
        workerLinkCnt_.emplace(workerIdx, 1);
        return true;
    } else if (it->second >= maxWorkerLinkeCnt_) {
        return false;
    } else {
        it->second++;
        return true;
    }
}

void AccTcpServerDefault::WorkerLinkCntUpdate(uint32_t workerIdx)
{
    std::unique_lock<std::mutex> lockGuard{ linkCntMutex };
    auto pos = workerLinkCnt_.find(workerIdx);
    if (pos == workerLinkCnt_.end()) {
        return;
    }
    if (--pos->second == 0) {
        workerLinkCnt_.erase(pos);
    }
    return;
}

Result AccTcpServerDefault::ConnectToPeerServer(const std::string &peerIp, uint16_t port, const AccConnReq &req,
                                                uint32_t maxRetryTimes, AccTcpLinkComplexPtr &newLink)
{
    ASSERT_RETURN(AccCommonUtil::IsValidIPv4(peerIp), ACC_ERROR);
    std::string ipAndPort = peerIp + ":" + std::to_string(port);

    auto tmpFD = ::socket(AF_INET, SOCK_STREAM, 0);
    if (tmpFD < 0) {
        LOG_ERROR("Failed to create socket, errno:" << errno << ", please check if fd is out of limit");
        return ACC_ERROR;
    }

    int flags = 1;
    setsockopt(tmpFD, SOL_TCP, TCP_NODELAY, reinterpret_cast<void*>(&flags), sizeof(flags));
    int synCnt = 1; /* Set connect() retry time for quick connect */
    setsockopt(tmpFD, IPPROTO_TCP, TCP_SYNCNT, &synCnt, sizeof(synCnt));

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(peerIp.c_str());
    addr.sin_port = htons(port);

    uint32_t timesRetried = 0;
    int lastErrno = 0;

    while (timesRetried < maxRetryTimes) {
        LOG_INFO("Trying to connect to " << ipAndPort);
        if (::connect(tmpFD, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
            struct timeval timeout = {ACC_LINK_RECV_TIMEOUT, 0};
            setsockopt(tmpFD, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            return Handshake(tmpFD, req, ipAndPort, newLink);
        }

        if (errno == EINTR) {
            continue;
        }

        if (lastErrno != errno) {
            LOG_INFO("Trying to connect to " << ipAndPort << " errno:" << errno << ", retry times:" << timesRetried);
            lastErrno = errno;
        }

        // interval between each retry, 1 sec for each time,
        sleep(1);
        timesRetried++;
    }

    SafeCloseFd(tmpFD);
    LOG_ERROR("Failed to connect to " << ipAndPort << " after tried " << timesRetried << " times");
    return ACC_ERROR;
}

AccTcpServerDefault::~AccTcpServerDefault()
{
    if (sslCtx_ != nullptr) {
        OpenSslApiWrapper::SslCtxFree(sslCtx_);
        sslCtx_ = nullptr;
    }
}

Result AccTcpServerDefault::GenerateSslCtx()
{
    if (!tlsOption_.enableTls) {
        return ACC_OK;
    }

    if (sslCtx_ != nullptr) {
        return ACC_OK;
    }

    AccTcpSslHelperPtr tmpHelperPtr = nullptr;
    if (sslHelper_ == nullptr) {
        tmpHelperPtr = AccMakeRef<AccTcpSslHelper>();
        if (tmpHelperPtr == nullptr) {
            LOG_ERROR("Failed to create server ssl helper");
            return ACC_MALLOC_FAIL;
        }
    } else {
        tmpHelperPtr = sslHelper_;
    }

    if (!tlsOption_.tlsPkPwd.empty()) { // if private key is encrypted, register decrypt handler
        tmpHelperPtr->RegisterDecryptHandler(decryptHandler_);
    }

    auto tmpSslCtx = OpenSslApiWrapper::SslCtxNew(OpenSslApiWrapper::TlsMethod());
    if (tmpSslCtx == nullptr) {
        LOG_ERROR("Failed to create server ssl ctx");
        return ACC_MALLOC_FAIL;
    }

    auto result = tmpHelperPtr->Start(tmpSslCtx, tlsOption_);
    if (result != ACC_OK) {
        LOG_ERROR("Failed to init server ssl ctx, ret " << result);
        OpenSslApiWrapper::SslCtxFree(tmpSslCtx);
        tmpSslCtx = nullptr;
        return result;
    }

    sslHelper_ = tmpHelperPtr;
    sslCtx_ = tmpSslCtx;
    return ACC_OK;
}

Result AccTcpServerDefault::CreateSSLLink(SSL* &ssl, int &tmpFD)
{
    if (tlsOption_.enableTls) {
        auto result = AccTcpSslHelper::NewSslLink(false, tmpFD, sslCtx_, ssl);
        if (result != ACC_OK) {
            LOG_ERROR("Failed to new server ssl link");
            SafeCloseFd(tmpFD);
            return ACC_NEW_OBJECT_FAIL;
        }
    }
    return ACC_OK;
}

void AccTcpServerDefault::ValidateSSLLink(SSL* &ssl, int &tmpFD)
{
    if (ssl != nullptr) {
        if (AccCommonUtil::SslShutdownHelper(ssl) != ACC_OK) {
            LOG_ERROR("shut down ssl failed!");
        }
        OpenSslApiWrapper::SslFree(ssl);
        ssl = nullptr;
    }
    SafeCloseFd(tmpFD);
}

Result AccTcpServerDefault::LinkReceive(AccRef<AccTcpLinkComplexDefault> &tmpLink, const std::string &ipAndPort)
{
    AccConnResp connResp{};
    auto result = tmpLink->BlockRecv(&connResp, sizeof(AccConnResp));
    if (result != ACC_OK || connResp.result != ACC_OK) {
        LOG_ERROR("Failed to receive server connecting handshake from " << ipAndPort << ", errno " << errno);
        return ACC_ERROR;
    }
    return ACC_OK;
}

Result AccTcpServerDefault::Handshake(int &tmpFD, const AccConnReq &connReq, const std::string &ipAndPort,
                                      AccTcpLinkComplexPtr &newLink)
{
    /* send connection request */
    auto result = ::send(tmpFD, reinterpret_cast<const void *>(&connReq), sizeof(connReq), 0);
    if (result != sizeof(connReq)) {
        LOG_ERROR("Failed to send connecting handshake to " << ipAndPort << ", errno " << errno);
        SafeCloseFd(tmpFD);
        return ACC_ERROR;
    }

    SSL *ssl = nullptr;
    if (CreateSSLLink(ssl, tmpFD) != ACC_OK) {
        return ACC_NEW_OBJECT_FAIL;
    }

    auto tmpLink = AccMakeRef<AccTcpLinkComplexDefault>(tmpFD, ipAndPort, AccTcpLinkDefault::NewId(), ssl);
    if (tmpLink == nullptr) {
        LOG_ERROR("Failed to create tcp server link object, probably out of memory");
        ValidateSSLLink(ssl, tmpFD);
        return ACC_NEW_OBJECT_FAIL;
    }

    // tmpLink作为智能指针 异常分支返回时会自动析构释放资源
    if (LinkReceive(tmpLink, ipAndPort) != ACC_OK) {
        return ACC_ERROR;
    }

    auto workIndex = WorkerSelect();
    if (workIndex == ACC_ERROR) {
        LOG_ERROR("Failed to select available worker.");
        return ACC_ERROR;
    }

    auto &worker = workers_[workIndex];
    result = tmpLink->Initialize(options_.linkSendQueueSize, workIndex, worker.Get());
    if (UNLIKELY(result != ACC_OK)) {
        LOG_ERROR("Failed to initialize the link from " << tmpLink->ShortName() << ", result " << result);
        return ACC_ERROR;
    }

    tmpLink->EnableNoBlocking();
    {
        /* check and add new link into map */
        std::lock_guard<std::mutex> guard(mutex_);
        auto iter = connectedLinks_.find(tmpLink->Id());
        if (iter != connectedLinks_.end()) {
            LOG_ERROR("Failed to handle new connection as found duplicated link id " << tmpLink->Id());
            return ACC_ERROR;
        }

        /* added to worker */
        result = worker->AddLink(tmpLink, EPOLLIN | EPOLLOUT | EPOLLET);
        if (UNLIKELY(result != ACC_OK)) {
            return result;
        }

        /* emplace map */
        connectedLinks_.emplace(tmpLink->Id(), tmpLink);
    }

    newLink = tmpLink.Get();
    LOG_INFO("Connect to " << ipAndPort << " successfully, with ssl " << (tlsOption_.enableTls ? "enable" : "disable"));
    return ACC_OK;
}
}  // namespace acc
}  // namespace ock