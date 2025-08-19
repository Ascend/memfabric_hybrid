/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <csignal>
#include "acc_common_util.h"
#include "acc_tcp_link_default.h"
#include "acc_tcp_client_default.h"

namespace ock {
namespace acc {
constexpr uint32_t PRINT_INTERVAL = 50000;
constexpr uint32_t TCP_CLIENT_SLEEP_TIME = 100;
constexpr uint32_t TCP_CLIENT_MAX_RECONN_TIMES = 100;

void AccTcpClientDefault::PollingThread()
{
    uint32_t waitConnectCount = 0;
    uint8_t* data = nullptr;
    uint32_t dataSize = 0;
    LOG_INFO("connRank: " << connRanks << ", tcp client polling thread start...");
    pollingStarted_.store(true);
    while (!needStop_.load()) {
        int16_t msgType = -1;
        int16_t msgRet = ACC_RESULT_BUTT;
        uint32_t bodyLength = 0;

        if (!isConnected_.load()) {
            if (waitConnectCount++ % PRINT_INTERVAL == 0) {
                LOG_INFO("connRank: " << connRanks << ", sleep for link connect ...");
            }
            usleep(TCP_CLIENT_SLEEP_TIME);
            continue;
        }

        auto result = Receive(nullptr, 0, msgType, msgRet, bodyLength);
        if (result != ACC_OK) {
            LOG_ERROR("connRank: " << connRanks << ", receive msg failed. ret: " << result);
            continue;
        }

        AllocRecvDataBuffer(data, dataSize, bodyLength);
        if (data == nullptr) {
            continue;
        }
        if (bodyLength != 0) {
            result = ReceiveRaw(data, bodyLength);
        }

        if (result != ACC_OK || msgType < MIN_MSG_TYPE || msgType >= MAX_MSG_TYPE) {
            LOG_ERROR("connRank: " << connRanks << ", recv msg_msg failed: " << result << ", msgType: " << msgType);
            continue;
        }

        if (newRequestHandle_[msgType] != nullptr) {
            newRequestHandle_[msgType](data, bodyLength);
        } else {
            LOG_ERROR("connRank: " << connRanks << ", no handle func! msgType: " << msgType);
            continue;
        }
    }
    LOG_INFO("connRank: " << connRanks << ", tcp client polling thread exit ...");
    pollingStarted_.store(false);
    if (data != nullptr) {
        free(data);
        data = nullptr;
    }
}

void AccTcpClientDefault::Destroy(bool needWait)
{
    Disconnect();
    needStop_.store(true);
    if (needWait) {
        LOG_INFO("start stop tcp client polling thread ...");
        while (pollingStarted_.load()) {
            usleep(TCP_CLIENT_SLEEP_TIME);
        }
        LOG_INFO("end stop tcp client polling thread ...");
    }
}

AccTcpClientDefault::~AccTcpClientDefault()
{
    if (pollThread_.joinable()) {
        pollThread_.join();
    }

    if (sslCtx_ != nullptr) {
        OpenSslApiWrapper::SslCtxFree(sslCtx_);
        sslCtx_ = nullptr;
    }
}

void AccTcpClientDefault::StartPolling()
{
    std::lock_guard<std::mutex> guard(writeMutex_);
    if (pollingStarted_.load()) {
        LOG_WARN("tcp_client has running polling thread.");
        return;
    }

    needStop_.store(false);
    std::thread tmpThread(&AccTcpClientDefault::PollingThread, this);
    pollThread_ = std::move(tmpThread);

    std::string tName = "tcp_client";
    if (pthread_setname_np(pollThread_.native_handle(), tName.c_str()) != 0) {
        LOG_WARN("set tcp_client thread name failed");
    }

    while (!pollingStarted_.load()) {
        usleep(TCP_CLIENT_SLEEP_TIME);
    }
}

Result AccTcpClientDefault::ConnectInit(int &fd)
{
    if (AccCommonUtil::CheckTlsOptions(tlsOption_) != ACC_OK) {
        LOG_ERROR("Invalid tls option");
        return ACC_ERROR;
    }

    /* only allow one thread to do the connecting */
    auto result = GenerateSslCtx();
    if (result != ACC_OK) {
        LOG_ERROR("Failed to generate ssl ctx, ret " << result);
        return result;
    }

    int tmpFD = ::socket(AF_INET, SOCK_STREAM, 0);
    if (tmpFD < 0) {
        LOG_ERROR("Failed to create socket, errno:" << errno << ", please check if fd is out of limit");
        return ACC_ERROR;
    }

    int flags = 1;
    setsockopt(tmpFD, SOL_TCP, TCP_NODELAY, reinterpret_cast<void*>(&flags), sizeof(flags));
    int synCnt = 1; /* Set connect() retry time for quick connect */
    setsockopt(tmpFD, IPPROTO_TCP, TCP_SYNCNT, &synCnt, sizeof(synCnt));

    if (localIp_ != "") {
        struct sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(localIp_.c_str());

        if (::bind(tmpFD, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            SafeCloseFd(tmpFD);
            LOG_ERROR("Failed to bind on " << localIp_ << " as errno " << errno);
            return ACC_ERROR;
        }
        LOG_INFO("connRank: " << connRanks << " bind ip " << localIp_);
    }

    fd = tmpFD;
    return ACC_OK;
}

Result AccTcpClientDefault::Connect(const AccConnReq &connReq, uint32_t maxConnRetryTimes)
{
    /* only allow one thread to do the connecting */
    std::lock_guard<std::mutex> guard(writeMutex_);
    connRanks = connReq.rankId;
    int tmpFD = -1;
    if (ConnectInit(tmpFD) != ACC_OK) {
        return ACC_ERROR;
    }

    void (*prevHandler)(int);
    prevHandler = signal(SIGPIPE, SIG_IGN);
    if (prevHandler == SIG_ERR) {
        LOG_ERROR("signal error");
    }
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(serverIp_.c_str());
    addr.sin_port = htons(serverPort_);

    uint32_t timesRetried = 0;
    LOG_INFO("connRank: " << connRanks << " connect max times: " << maxConnRetryTimes);
    while (timesRetried < maxConnRetryTimes && !needStop_.load()) {
        LOG_INFO("connRank: " << connRanks << " Trying to connect to " << IpAndPort());
        if (::connect(tmpFD, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
            reqforReConn_ = connReq;
            struct timeval timeout = {ACC_LINK_RECV_TIMEOUT, 0};
            setsockopt(tmpFD, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            return Handshake(tmpFD, connReq);
        }

        if (errno == EINTR) {
            continue;
        }

        // interval between each retry, 1 sec for each time,
        sleep(1);
        timesRetried++;

        LOG_WARN("connRank: " << connRanks << " Trying to connect to " << IpAndPort() << " errno:" << errno
                              << ", retry times:" << timesRetried);
    }

    SafeCloseFd(tmpFD);
    LOG_ERROR("Failed to connect to " << IpAndPort() << " after tried " << timesRetried << " times");
    return ACC_ERROR;
}

Result AccTcpClientDefault::GenerateSslCtx()
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
            LOG_ERROR("Failed to create client ssl helper");
            return ACC_MALLOC_FAIL;
        }
    } else {
        tmpHelperPtr = sslHelper_;
    }

    auto tmpSslCtx = OpenSslApiWrapper::SslCtxNew(OpenSslApiWrapper::TlsMethod());
    if (tmpSslCtx == nullptr) {
        LOG_ERROR("Failed to create client ssl ctx");
        return ACC_MALLOC_FAIL;
    }

    auto result = tmpHelperPtr->Start(tmpSslCtx, tlsOption_);
    if (result != ACC_OK) {
        LOG_ERROR("Failed to init client ssl ctx, ret " << result);
        OpenSslApiWrapper::SslCtxFree(tmpSslCtx);
        tmpSslCtx = nullptr;
        return result;
    }

    sslHelper_ = tmpHelperPtr;
    sslCtx_ = tmpSslCtx;
    return ACC_OK;
}

Result AccTcpClientDefault::Handshake(int &tmpFD, const AccConnReq &connReq)
{
    /* send connection request */
    auto result = ::send(tmpFD, reinterpret_cast<const void*>(&connReq), sizeof(connReq), 0);
    if (result != sizeof(connReq)) {
        LOG_ERROR("Failed to send connecting handshake to " << IpAndPort() << ", errno " << errno);
        SafeCloseFd(tmpFD);
        return ACC_ERROR;
    }

    SSL* ssl = nullptr;
    if (tlsOption_.enableTls) {
        result = AccTcpSslHelper::NewSslLink(false, tmpFD, sslCtx_, ssl);
        if (result != ACC_OK) {
            LOG_ERROR("Failed to new client ssl link");
            SafeCloseFd(tmpFD);
            return ACC_NEW_OBJECT_FAIL;
        }
    }

    auto tmpLink = AccMakeRef<AccTcpLinkDefault>(tmpFD, IpAndPort(), AccTcpLinkDefault::NewId(), ssl);
    if (tmpLink == nullptr) {
        LOG_ERROR("Failed to create Tcp link object, probably out of memory");
        if (ssl != nullptr) {
            if (AccCommonUtil::SslShutdownHelper(ssl) != ACC_OK) {
                LOG_ERROR("shut down ssl failed!");
            }
            OpenSslApiWrapper::SslFree(ssl);
            ssl = nullptr;
        }
        SafeCloseFd(tmpFD);
        return ACC_NEW_OBJECT_FAIL;
    }

    // tmpLink作为智能指针 异常分支返回时会自动析构释放资源
    AccConnResp connResp{};
    result = tmpLink->BlockRecv(&connResp, sizeof(AccConnResp));
    if (result != ACC_OK || connResp.result != ACC_OK) {
        LOG_ERROR("Failed to receive connecting handshake from " << IpAndPort() << ", errno " << errno << ", result "
                                                                 << result);
        return ACC_ERROR;
    }

    link_ = tmpLink.Get();
    LOG_INFO("Connect to " << IpAndPort() << " successfully, ssl " << (tlsOption_.enableTls ? "enable" : "disable"));
    isConnected_.store(true);
    return ACC_OK;
}

void AccTcpClientDefault::Disconnect()
{
    std::lock_guard<std::mutex> guard(writeMutex_);
    if (link_ != nullptr && link_.Get() != nullptr) {
        link_->Close();
        link_ = nullptr;
    }
    if (sslHelper_ != nullptr) {
        sslHelper_->Stop();
        sslHelper_ = nullptr;
    }
    isConnected_.store(false);
}

void AccTcpClientDefault::SetServerIpAndPort(std::string serverIp, uint16_t serverPort)
{
    ASSERT_RET_VOID(AccCommonUtil::IsValidIPv4(serverIp));
    serverIp_ = std::move(serverIp);
    serverPort_ = serverPort;
    LOG_INFO("set server ip( " << serverIp_ << " ) and port( " << serverPort_ << " )");
}

void AccTcpClientDefault::SetSslOption(const AccTlsOption &tlsOption)
{
    tlsOption_ = tlsOption;
    LOG_INFO("set ssl option with " << (tlsOption_.enableTls ? "enable" : "disable"));
}

Result AccTcpClientDefault::LoadDynamicLib(const std::string &dynLibPath)
{
    auto ret = OpenSslApiWrapper::Load(dynLibPath);
    if (ret != ACC_OK) {
        LOG_ERROR("load open ssl failed");
        return ACC_ERROR;
    }
    return ACC_OK;
}

void AccTcpClientDefault::SetLocalIp(std::string localIp)
{
    if (localIp != "" && !AccCommonUtil::IsValidIPv4(localIp)) {
        LOG_WARN("local ip:" << localIp << " is invalid ipV4 address!");
        return;
    }
    localIp_ = std::move(localIp);
    LOG_INFO("set processor local ip( " << localIp_);
}

void AccTcpClientDefault::SetMaxReconnCnt(uint32_t maxReconnCnt)
{
    ASSERT_RET_VOID(maxReconnCnt <= TCP_CLIENT_MAX_RECONN_TIMES);
    this->maxReconnCnt_ = maxReconnCnt;
}

Result AccTcpClientDefault::Send(int16_t msgType, uint8_t* data, uint32_t len) noexcept
{
    ASSERT_RETURN(data != nullptr, ACC_INVALID_PARAM);
    ASSERT_RETURN(msgType < MAX_MSG_TYPE, ACC_INVALID_PARAM);
    ASSERT_RETURN(isConnected_.load(), ACC_LINK_ERROR);
    AccMsgHeader header(msgType, len, seqNo_++);
    Result callResult = ACC_OK;
    int maxReconnAttempt = static_cast<int>(maxReconnCnt_);
    do {
        maxReconnAttempt--;
        std::lock_guard<std::mutex> guard(writeMutex_);
        ASSERT_RETURN(link_ != nullptr, ACC_CONNECTION_NOT_READY);
        ASSERT_RETURN(link_.Get(), ACC_CONNECTION_NOT_READY);
        callResult = link_->BlockSend(&header, sizeof(AccMsgHeader));
        if (callResult != ACC_OK) {
            continue;
        }
        if (len != 0) {
            callResult = link_->BlockSend(data, len);
        }
        if (callResult == ACC_OK) {
            break;
        }
    } while (maxReconnAttempt >= 0 && ReconnectCtrl(callResult));

    if (maxReconnCnt_ == 0 && (callResult == ACC_LINK_NEED_RECONN || callResult == ACC_LINK_ERROR)) {
        isConnected_.store(false);
    }
    return callResult;
}

Result AccTcpClientDefault::Receive(uint8_t* data, uint32_t len, int16_t &msgType, int16_t &result,
                                    uint32_t &acLen) noexcept
{
    AccMsgHeader header;
    Result callResult = ACC_OK;
    int maxReconnAttempt = static_cast<int>(maxReconnCnt_);  // already checked < TCP_CLIENT_MAX_RECONN_TIMES
    do {
        maxReconnAttempt--;
        std::lock_guard<std::mutex> guard(readMutex_);
        ASSERT_RETURN(link_.Get(), ACC_CONNECTION_NOT_READY);
        callResult = link_->BlockRecv(&header, sizeof(AccMsgHeader));
        if (callResult != ACC_OK) {
            continue;
        }
        msgType = header.type;
        result = header.result;
        acLen = header.bodyLen;
        if (len != 0) {
            ASSERT_RETURN(acLen <= len, ACC_LINK_MSG_INVALID);
            callResult = link_->BlockRecv(data, len);
        }
        if (callResult == ACC_OK) {
            break;
        }
    } while (maxReconnAttempt >= 0 && ReconnectCtrl(callResult));

    if (maxReconnCnt_ == 0 && (callResult == ACC_LINK_NEED_RECONN || callResult == ACC_LINK_ERROR)) {
        isConnected_.store(false);
    }
    ASSERT_RETURN(acLen <= MAX_RECV_BODY_LEN, ACC_LINK_MSG_INVALID);
    return callResult;
}

bool AccTcpClientDefault::ReconnectCtrl(Result callResult) noexcept
{
    if (!isConnected_.load()) {
        return false;
    }

    LOG_INFO("connRank: " << connRanks << " Reconnecting, errno:" << -callResult);
    if (callResult != ACC_LINK_NEED_RECONN && callResult != ACC_LINK_ERROR) {
        return false;
    }
    std::lock_guard<std::mutex> guard(reConnectMutex_);
    // To see if the connection has been fixed by others;
    if (link_ != nullptr && link_->IsConnected()) {
        return true;
    }

    Result reConnResult = Connect(reqforReConn_, 5L);
    if (reConnResult == ACC_OK) {
        return true;
    } else {
        isConnected_.store(false);
        return false;
    }
}

void AccTcpClientDefault::AllocRecvDataBuffer(uint8_t* &data, uint32_t &dataSize, uint32_t demandSize)
{
    if (UNLIKELY(data == nullptr)) {
        dataSize = std::max(dataSize, demandSize);
        if (dataSize > MAX_RECV_BODY_LEN) {
            data = nullptr;
            return;
        }
        data = static_cast<uint8_t*>(malloc(dataSize));
    } else if (demandSize > dataSize) {
        // free old and malloc new one
        free(data);
        data = nullptr;
        dataSize = demandSize;
        data = static_cast<uint8_t*>(malloc(dataSize));
    }
    if (data == nullptr) {
        LOG_ERROR("connRank: " << connRanks << ", malloc receive buffer failed, demand size:" << demandSize);
    }
}
}  // namespace acc
}  // namespace ock