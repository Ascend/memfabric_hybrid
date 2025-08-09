/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <iostream>
#include <string>
#include "acc_file_validator.h"
#include "acc_file_validator.cpp"
#define protected public
#include "acc_tcp_client.h"
#include "acc_tcp_client_default.h"
#include "acc_tcp_worker.h"
#include "acc_tcp_link.h"
#include "acc_tcp_link_complex_default.h"
#include "acc_includes.h"
#undef protected
#define private public
#include "acc_tcp_server.h"
#include "acc_tcp_server_default.h"
#undef private
namespace {
const int BUFF_SIZE = 32;
const int LISTEN_PORT = 8100;
const int LINK_SEND_QUEUE_SIZE = 100;
const int WORKER_COUNT = 4;
const int TIMEOUT_TIME = 10000;
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
using namespace ock::acc;

enum OpCode : int32_t {
    TTP_OP_REGISTER = 0, // register
    TTP_OP_REGISTER_REPLY,
    TTP_OP_HEARTBEAT_SEND, // processor send heart beat to controller
    TTP_OP_HEARTBEAT_REPLY,
    TTP_OP_CKPT_SEND,   // controller send ckpt action to processor
    TTP_OP_CKPT_REPLY,  // processor reply result of ckpt to controller
    TTP_OP_CTRL_NOTIFY, // controller send back up controller to processor
    TTP_OP_RENAME,      // controller send rename request to processor
    TTP_OP_RENAME_REPLY,
    TTP_OP_EXIT,
    TTP_OP_BUTT,
};

static std::unordered_map<int32_t, AccTcpLinkComplexPtr> g_rankLinkMap;
uint32_t connectCnt{ 0 };
static void *g_cbCtx = nullptr;
static uint32_t initCnt = 0;

class TestAccTcpClient : public testing::Test {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();

public:
    void SetUp() override;
    void TearDown() override;
    void RegisterHandle();

public:
    int32_t HandleHeartBeat(const AccTcpRequestContext &context)
    {
        if (context.DataLen() != BUFF_SIZE) {
            std::cout << "receive data len mis match" << std::endl;
            return 1;
        }
        std::cout << "receive data len match" << std::endl;
        AccDataBufferPtr buffer = AccMakeRef<AccDataBuffer>(0);
        if (buffer == nullptr) {
            std::cout << "data buffer is nullptr" << std::endl;
            return 1;
        }
        return context.Reply(0, buffer);
        return 0;
    }

    int32_t HandleDumpReply(const AccTcpRequestContext &context)
    {
        if (context.DataLen() != BUFF_SIZE) {
            return 1;
        }
        return 0;
    }

    int32_t HandleRenameReply(const AccTcpRequestContext &context)
    {
        if (context.DataLen() != BUFF_SIZE) {
            return 1;
        }
        return 0;
    }

    int32_t HandleRegister(const AccTcpRequestContext &context)
    {
        if (context.DataLen() != BUFF_SIZE) {
            return 1;
        }
        return 0;
    }

    int32_t HbReplyCallBack(AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx)
    {
        g_cbCtx = (cbCtx == nullptr) ? nullptr : cbCtx->DataPtrVoid();
        if (result != MSG_SENT) {
            return 1;
        }
        return 0;
    }

    int32_t ControllerCkptCallBack(AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx)
    {
        g_cbCtx = (cbCtx == nullptr) ? nullptr : cbCtx->DataPtrVoid();
        return 0;
    }

    int32_t BroadcastCallBack(AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx)
    {
        g_cbCtx = (cbCtx == nullptr) ? nullptr : cbCtx->DataPtrVoid();
        return 0;
    }

    int32_t RenameCallBack(AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx)
    {
        g_cbCtx = (cbCtx == nullptr) ? nullptr : cbCtx->DataPtrVoid();
        return 0;
    }

    int32_t NotifyProcessorExitCb(AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx)
    {
        g_cbCtx = (cbCtx == nullptr) ? nullptr : cbCtx->DataPtrVoid();
        return 0;
    }

    int32_t HandleNewConnection(const AccConnReq &req, const AccTcpLinkComplexPtr &link)
    {
        connectCnt++;
        auto it = g_rankLinkMap.find(req.rankId);
        if (it != g_rankLinkMap.end()) {
            return 1;
        }
        g_rankLinkMap[req.rankId] = link;
        return 0;
    }

    int32_t HandleNewDefaultConnection(const AccConnReq &req, const AccTcpLinkComplexDefaultPtr &link)
    {
        connectCnt++;
        auto it = g_rankLinkMap.find(req.rankId);
        if (it != g_rankLinkMap.end()) {
            return 1;
        }
        auto tmpLink = AccConvert<AccTcpLinkComplexDefault, AccTcpLinkComplex>(link);
        g_rankLinkMap[req.rankId] = tmpLink;
        return 0;
    }

    int32_t HandleLinkBroken(const AccTcpLinkComplexPtr &link)
    {
        for (auto it = g_rankLinkMap.begin(); it != g_rankLinkMap.end(); ++it) {
            if (it->second->Id() == link->Id()) {
                g_rankLinkMap.erase(it->first);
                break;
            }
        }
        return 0;
    }

    int32_t HandleLinkDefaultBroken(const AccTcpLinkComplexDefaultPtr &link)
    {
        for (auto it = g_rankLinkMap.begin(); it != g_rankLinkMap.end(); ++it) {
            if (it->second->Id() == link->Id()) {
                g_rankLinkMap.erase(it->first);
                break;
            }
        }
        return 0;
    }

    void ClientHandleCtrlNotify(uint8_t *data, uint32_t len)
    {
        clientRecvLen = len;
        isClientRecv.store(true);
    }

protected:
    static AccTcpServerPtr mServer;
    std::atomic<bool> isClientRecv{ false };
    uint32_t clientRecvLen;
};

AccTcpServerPtr TestAccTcpClient::mServer = nullptr;

void TestAccTcpClient::SetUpTestSuite()
{
    mServer = AccTcpServer::Create();
    ASSERT_TRUE(mServer != nullptr);
}

void TestAccTcpClient::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

void TestAccTcpClient::RegisterHandle()
{
    // add server handler
    auto hbMethod = [this](const AccTcpRequestContext &context) { return HandleHeartBeat(context); };
    mServer->RegisterNewRequestHandler(TTP_OP_HEARTBEAT_SEND, hbMethod);
    auto dumpReplyMethod = [this](const AccTcpRequestContext &context) { return HandleDumpReply(context); };
    mServer->RegisterNewRequestHandler(TTP_OP_CKPT_REPLY, dumpReplyMethod);
    auto renameReplyMethod = [this](const AccTcpRequestContext &context) { return HandleRenameReply(context); };
    mServer->RegisterNewRequestHandler(TTP_OP_RENAME_REPLY, renameReplyMethod);
    auto registerMethod = [this](const AccTcpRequestContext &context) { return HandleRegister(context); };
    mServer->RegisterNewRequestHandler(TTP_OP_REGISTER, registerMethod);

    // add sent handler
    auto hdSentMethod = [this](AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx) {
        return HbReplyCallBack(result, header, cbCtx);
    };
    mServer->RegisterRequestSentHandler(TTP_OP_HEARTBEAT_REPLY, hdSentMethod);

    auto ckptSentMethod = [this](AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx) {
        return ControllerCkptCallBack(result, header, cbCtx);
    };
    mServer->RegisterRequestSentHandler(TTP_OP_CKPT_SEND, ckptSentMethod);

    auto bdSentMethod = [this](AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx) {
        return BroadcastCallBack(result, header, cbCtx);
    };
    mServer->RegisterRequestSentHandler(TTP_OP_CTRL_NOTIFY, bdSentMethod);

    auto rmSentMethod = [this](AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx) {
        return RenameCallBack(result, header, cbCtx);
    };
    mServer->RegisterRequestSentHandler(TTP_OP_RENAME, rmSentMethod);

    auto exitSentMethod = [this](AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx) {
        return NotifyProcessorExitCb(result, header, cbCtx);
    };
    mServer->RegisterRequestSentHandler(TTP_OP_EXIT, exitSentMethod);

    // add link handle
    auto linkMethod = [this](const AccConnReq &req, const AccTcpLinkComplexPtr &link) {
        return HandleNewConnection(req, link);
    };
    mServer->RegisterNewLinkHandler(linkMethod);

    auto linkBrokenMethod = [this](const AccTcpLinkComplexPtr &link) { return HandleLinkBroken(link); };
    mServer->RegisterLinkBrokenHandler(linkBrokenMethod);
}

void TestAccTcpClient::SetUp()
{
    if (initCnt++ == 0) {
        RegisterHandle();
    }

    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;
    ASSERT_EQ(mServer->Start(opts), ACC_OK);
}

void TestAccTcpClient::TearDown()
{
    mServer->Stop();
    g_rankLinkMap.clear();
    GlobalMockObject::verify();
}

// *********************************TEST_F*************************

int32_t LinkClose(AccTcpLinkDefault *link, void *data, uint32_t len)
{
    std::cout << "link close stub" << std::endl;
    link->Close();
    return ACC_LINK_NEED_RECONN;
}

TEST_F(TestAccTcpClient, send_reconnect_times_match)
{
    connectCnt = 0;
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", LISTEN_PORT);
    ASSERT_TRUE(mClient != nullptr);
    mClient->SetMaxReconnCnt(1);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_OK, result);
    ASSERT_EQ(1, connectCnt);

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    uint8_t *data = reinterpret_cast<uint8_t *>(buf);
    MOCKER_CPP(&AccTcpLinkDefault::BlockSend, int32_t(*)(AccTcpLinkDefault *, void *, uint32_t))
        .expects(atLeast(1))
        .will(invoke(LinkClose));
    result = mClient->Send(TTP_OP_HEARTBEAT_SEND, data, BUFF_SIZE);
    ASSERT_EQ(2, connectCnt);
    sleep(1);
    mClient->Disconnect();
}

TEST_F(TestAccTcpClient, recv_reconnect_times_match)
{
    connectCnt = 0;

    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", 8100);
    ASSERT_TRUE(mClient != nullptr);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_OK, result);

    MOCKER_CPP(&AccTcpLinkDefault::BlockRecv, int32_t(*)(AccTcpLinkDefault *, void *, uint32_t))
        .expects(atLeast(1))
        .will(invoke(LinkClose));
    std::thread recvThread([&result, mClient]() {
        int16_t msgType;
        int16_t msgRet;
        uint32_t bodyLength;
        result = mClient->Receive(nullptr, 0, msgType, msgRet, bodyLength);
        std::cout << "client recevie msg header" << std::endl;
    });
    recvThread.detach();
    ASSERT_EQ(result, 0);
    ASSERT_EQ(1, connectCnt);

    void *data = malloc(BUFF_SIZE);
    ASSERT_TRUE(data != nullptr);
    memset(data, 0, BUFF_SIZE);
    AccDataBufferPtr buffer = AccMakeRef<AccDataBuffer>(reinterpret_cast<uint8_t *>(data), BUFF_SIZE);
    ASSERT_TRUE(buffer != nullptr);

    for (auto it = g_rankLinkMap.begin(); it != g_rankLinkMap.end(); ++it) {
        AccTcpLinkComplexPtr link = it->second;
        result = link->NonBlockSend(TTP_OP_CTRL_NOTIFY, buffer, nullptr);
        ASSERT_EQ(ACC_OK, result);
    }
    std::cout << "server send msg" << std::endl;

    sleep(2);
    mClient->Disconnect();
    ASSERT_EQ(2, connectCnt);
    free(data);
}

TEST_F(TestAccTcpClient, test_client_connect_send_should_return_ok)
{
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", LISTEN_PORT);
    ASSERT_TRUE(mClient != nullptr);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_OK, result);

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    uint8_t *data = reinterpret_cast<uint8_t *>(buf);
    result = mClient->Send(TTP_OP_HEARTBEAT_SEND, data, BUFF_SIZE);
    ASSERT_EQ(ACC_OK, result);
    sleep(1);
    mClient->Disconnect();
}

TEST_F(TestAccTcpClient, test_client_connect_send_should_return_error)
{
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", LISTEN_PORT);
    ASSERT_TRUE(mClient != nullptr);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_OK, result);

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    uint8_t *data = reinterpret_cast<uint8_t *>(buf);
    mClient->Disconnect();
    result = mClient->Send(TTP_OP_HEARTBEAT_SEND, data, BUFF_SIZE);
    ASSERT_TRUE(result != ACC_OK);
}

TEST_F(TestAccTcpClient, test_client_connect_31_send_should_return_error)
{
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", LISTEN_PORT);
    ASSERT_TRUE(mClient != nullptr);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_OK, result);

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    uint8_t *data = reinterpret_cast<uint8_t *>(buf);
    result = mClient->Send(TTP_OP_HEARTBEAT_SEND, data, 31);
    ASSERT_TRUE(result != true);
    mClient->Disconnect();
}

TEST_F(TestAccTcpClient, test_client_connect_0_send_should_return_error)
{
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", LISTEN_PORT);
    ASSERT_TRUE(mClient != nullptr);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_OK, result);

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    uint8_t *data = reinterpret_cast<uint8_t *>(buf);
    result = mClient->Send(TTP_OP_HEARTBEAT_SEND, data, 0);
    ASSERT_TRUE(result != true);
    mClient->Disconnect();
}

TEST_F(TestAccTcpClient, test_client_connect_send_1_should_return_error)
{
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", LISTEN_PORT);
    ASSERT_TRUE(mClient != nullptr);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_OK, result);

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    uint8_t *data = reinterpret_cast<uint8_t *>(buf);
    mServer->Stop();
    sleep(1);
    result = mClient->Send(TTP_OP_HEARTBEAT_SEND, data, BUFF_SIZE);
    ASSERT_TRUE(result != true);
    mClient->Disconnect();
}

TEST_F(TestAccTcpClient, test_client_connect_should_return_error)
{
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 1;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", LISTEN_PORT);
    ASSERT_TRUE(mClient != nullptr);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_ERROR, result);
    mClient->Disconnect();
}

TEST_F(TestAccTcpClient, test_client_connect_1_should_return_error)
{
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", LISTEN_PORT);
    ASSERT_TRUE(mClient != nullptr);
    mServer->Stop();
    sleep(1);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_ERROR, result);
    sleep(1);
    mClient->Disconnect();
}

TEST_F(TestAccTcpClient, test_server_send_should_return_ok)
{
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", 8100);
    ASSERT_TRUE(mClient != nullptr);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_OK, result);

    std::thread recvThread([&result, mClient]() {
        int16_t msgType;
        int16_t msgRet;
        uint32_t bodyLength;
        result = mClient->Receive(nullptr, 0, msgType, msgRet, bodyLength);
        std::cout << "client recevie msg header" << std::endl;
        if (result != ACC_OK) {
            return;
        }

        char buf[bodyLength];
        memset(buf, 0, bodyLength);
        uint8_t *data = reinterpret_cast<uint8_t *>(buf);
        if (bodyLength != 0) {
            result = mClient->ReceiveRaw(data, bodyLength);
            std::cout << "client recevie msg body" << std::endl;
        }
    });
    recvThread.detach();
    ASSERT_EQ(result, 0);

    void *data = malloc(BUFF_SIZE);
    ASSERT_TRUE(data != nullptr);
    memset(data, 0, BUFF_SIZE);
    AccDataBufferPtr buffer = AccMakeRef<AccDataBuffer>(reinterpret_cast<uint8_t *>(data), BUFF_SIZE);
    ASSERT_TRUE(buffer != nullptr);

    for (auto it = g_rankLinkMap.begin(); it != g_rankLinkMap.end(); ++it) {
        AccTcpLinkComplexPtr link = it->second;
        result = link->NonBlockSend(TTP_OP_CTRL_NOTIFY, buffer, nullptr);
        ASSERT_EQ(ACC_OK, result);
    }
    std::cout << "server send msg" << std::endl;
    sleep(2);
    mClient->Disconnect();
    free(data);
}

TEST_F(TestAccTcpClient, test_server_send_should_return_error)
{
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", 8100);
    ASSERT_TRUE(mClient != nullptr);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_OK, result);

    void *data = malloc(BUFF_SIZE);
    ASSERT_TRUE(data != nullptr);
    memset(data, 0, BUFF_SIZE);
    AccDataBufferPtr buffer = AccMakeRef<AccDataBuffer>(reinterpret_cast<uint8_t *>(data), BUFF_SIZE);
    ASSERT_TRUE(buffer != nullptr);

    mClient->Disconnect();
    sleep(1);
    for (auto it = g_rankLinkMap.begin(); it != g_rankLinkMap.end(); ++it) {
        AccTcpLinkComplexPtr link = it->second;
        result = link->NonBlockSend(TTP_OP_CTRL_NOTIFY, buffer, nullptr);
        ASSERT_TRUE(true != result);
    }
    free(data);
    std::cout << "server send msg" << std::endl;
}

// TEST_F(TestAccTcpClient, test_server_send_ok_check_cb)
// {
//     AccConnReq req{};
//     req.rankId = 0;
//     req.magic = 0;
//     req.version = 1;
//     AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", 8100);
//     ASSERT_TRUE(mClient != nullptr);
//     int32_t result = mClient->Connect(req);
//     ASSERT_EQ(ACC_OK, result);

//     std::thread recvThread([&result, mClient]() {
//         int16_t msgType;
//         int16_t msgRet;
//         uint32_t bodyLength;
//         result = mClient->Receive(nullptr, 0, msgType, msgRet, bodyLength);
//         std::cout << "client recevie msg header" << std::endl;
//         if (result != ACC_OK) {
//             return;
//         }
//         char buf[bodyLength];
//         memset(buf, 0, bodyLength);
//         uint8_t *data = reinterpret_cast<uint8_t *>(buf);
//         if (bodyLength != 0) {
//             result = mClient->ReceiveRaw(data, bodyLength);
//             std::cout << "client recevie msg body" << std::endl;
//         }
//     });
//     recvThread.detach();
//     ASSERT_EQ(result, 0);

//     void *data = malloc(BUFF_SIZE);
//     ASSERT_TRUE(data != nullptr);
//     memset(data, 0, BUFF_SIZE);
//     AccDataBufferPtr buffer = AccMakeRef<AccDataBuffer>(reinterpret_cast<uint8_t *>(data), BUFF_SIZE);
//     if (buffer == nullptr) {
//         free(data);
//         ASSERT_TRUE(buffer != nullptr);
//     }

//     void *ctx = malloc(BUFF_SIZE);
//     ASSERT_TRUE(ctx != nullptr);
//     AccDataBufferPtr cbCtx = AccMakeRef<AccDataBuffer>(reinterpret_cast<uint8_t *>(ctx), BUFF_SIZE);

//     for (auto it = g_rankLinkMap.begin(); it != g_rankLinkMap.end(); ++it) {
//         AccTcpLinkComplexPtr link = it->second;
//         result = link->NonBlockSend(TTP_OP_CTRL_NOTIFY, buffer, nullptr);
//         ASSERT_EQ(ACC_OK, result);
//     }
//     std::cout << "server send msg" << std::endl;
//     sleep(2);
//     ASSERT_NE(g_cbCtx, ctx);
//     mClient->Disconnect();
// }

TEST_F(TestAccTcpClient, test_client_recv_by_polling)
{
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", LISTEN_PORT);
    ASSERT_TRUE(mClient != nullptr);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_OK, result);

    auto notifyMethod = [this](uint8_t *data, uint32_t len) { return ClientHandleCtrlNotify(data, len); };
    mClient->RegisterNewRequestHandler(TTP_OP_CTRL_NOTIFY, notifyMethod);

    mClient->SetReceiveTimeout(TIMEOUT_TIME); // 1s
    mClient->StartPolling();

    void *data = malloc(BUFF_SIZE);
    ASSERT_TRUE(data != nullptr);
    memset(data, 0, BUFF_SIZE);
    AccDataBufferPtr buffer = AccMakeRef<AccDataBuffer>(reinterpret_cast<uint8_t *>(data), BUFF_SIZE);
    ASSERT_TRUE(buffer != nullptr);

    for (auto it = g_rankLinkMap.begin(); it != g_rankLinkMap.end(); ++it) {
        AccTcpLinkComplexPtr link = it->second;
        result = link->NonBlockSend(TTP_OP_CTRL_NOTIFY, buffer, nullptr);
        ASSERT_EQ(ACC_OK, result);
    }
    std::cout << "server send msg" << std::endl;
    sleep(2);

    ASSERT_TRUE(isClientRecv.load());
    ASSERT_TRUE(clientRecvLen == BUFF_SIZE);
    free(data);
    mClient->Disconnect();
    mClient->Destroy();
}

TEST_F(TestAccTcpClient, test_client_change_ip)
{
    std::string ip1 = "127.0.0.1";
    std::string ip2 = "127.2.2.3";

    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create(ip1, LISTEN_PORT);
    ASSERT_TRUE(mClient != nullptr);
    ASSERT_TRUE(mClient->IpAndPort() == ip1);

    mClient->SetServerIpAndPort(ip2, 8080);
    ASSERT_TRUE(mClient->IpAndPort() == ip2);
}

TEST_F(TestAccTcpClient, test_worker_ValidateOptions)
{
    AccTcpWorkerOptions workerOptions;
    workerOptions.threadPriority = 0;
    workerOptions.cpuId = -1;
    workerOptions.pollingTimeoutMs = 10;
    AccTcpWorkerPtr worker = new (std::nothrow) AccTcpWorker(workerOptions);
    auto ret = worker->Start();
    ASSERT_TRUE(ret != ACC_OK);

    worker->RegisterNewRequestHandler(nullptr);
    auto hbMethod = [this](const AccTcpRequestContext &context) { return HandleHeartBeat(context); };
    worker->RegisterNewRequestHandler(hbMethod);
    worker->RegisterNewRequestHandler(hbMethod);
    ret = worker->Start();
    ASSERT_TRUE(ret != ACC_OK);

    worker->RegisterRequestSentHandler(nullptr);
    auto hdSentMethod = [this](AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx) {
        return HbReplyCallBack(result, header, cbCtx);
    };
    worker->RegisterRequestSentHandler(hdSentMethod);
    worker->RegisterRequestSentHandler(hdSentMethod);
    ret = worker->Start();
    ASSERT_TRUE(ret != ACC_OK);

    worker->RegisterLinkBrokenHandler(nullptr);
    auto linkBrokenMethod = [this](const AccTcpLinkComplexDefaultPtr &link) { return HandleLinkDefaultBroken(link); };
    worker->RegisterLinkBrokenHandler(linkBrokenMethod);
    worker->RegisterLinkBrokenHandler(linkBrokenMethod);
    ret = worker->Start();
    ASSERT_TRUE(ret == ACC_OK);
}

TEST_F(TestAccTcpClient, test_worker_ValidateOptions_NoName)
{
    AccTcpWorkerOptions workerOptions;
    workerOptions.threadPriority = 0;
    workerOptions.cpuId = -1;
    workerOptions.pollingTimeoutMs = 10;
    workerOptions.name_ = "";
    AccTcpWorkerPtr worker = new (std::nothrow) AccTcpWorker(workerOptions);

    auto hbMethod = [this](const AccTcpRequestContext &context) { return HandleHeartBeat(context); };
    worker->RegisterNewRequestHandler(hbMethod);

    auto hdSentMethod = [this](AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx) {
        return HbReplyCallBack(result, header, cbCtx);
    };
    worker->RegisterRequestSentHandler(hdSentMethod);

    auto linkBrokenMethod = [this](const AccTcpLinkComplexDefaultPtr &link) { return HandleLinkDefaultBroken(link); };
    worker->RegisterLinkBrokenHandler(linkBrokenMethod);

    auto ret = worker->Start();
    ASSERT_TRUE(ret != ACC_OK);
}

TEST_F(TestAccTcpClient, test_server_connect_to_peer_server_should_return_ok)
{
    const std::string nextIp = "127.0.0.1";
    uint16_t nextPort = LISTEN_PORT;
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpLinkComplexPtr nextLink;
    int32_t ret = mServer->ConnectToPeerServer(nextIp, nextPort, req, nextLink);
    ASSERT_EQ(ACC_OK, ret);
    std::cout << "finish" << std::endl;
}

TEST_F(TestAccTcpClient, test_server_connect_to_peer_server_should_return_error)
{
    const std::string nextIp = "127.0.0.1";
    uint16_t nextPort = 8100;
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpLinkComplexPtr nextLink;
    mServer->Stop();
    sleep(2);
    int32_t ret = mServer->ConnectToPeerServer(nextIp, nextPort, req, 5, nextLink);
    ASSERT_EQ(ACC_ERROR, ret);
    std::cout << "finish" << std::endl;
}

TEST_F(TestAccTcpClient, test_server_connect_to_peer_server_1_should_return_error)
{
    const std::string nextIp = "127.0.0.1";
    uint16_t nextPort = 8101;
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpLinkComplexPtr nextLink;
    int32_t ret = mServer->ConnectToPeerServer(nextIp, nextPort, req, 5, nextLink);
    ASSERT_EQ(ACC_ERROR, ret);
    std::cout << "finish" << std::endl;
}

TEST_F(TestAccTcpClient, test_server_connect_to_peer_server_2_should_return_error)
{
    const std::string nextIp = "127.0.0.1";
    uint16_t nextPort = LISTEN_PORT;
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 1;
    req.version = 1;
    AccTcpLinkComplexPtr nextLink;
    int32_t ret = mServer->ConnectToPeerServer(nextIp, nextPort, req, nextLink);
    ASSERT_EQ(ACC_ERROR, ret);
    std::cout << "finish" << std::endl;
}

TEST_F(TestAccTcpClient, test_server_start_listen_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = false;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
}

TEST_F(TestAccTcpClient, test_server_start_linkSendQueue_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = 1;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
}

TEST_F(TestAccTcpClient, test_server_start_listenIp_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = " ";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
}

TEST_F(TestAccTcpClient, test_server_start_listenPort_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = 0;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
}

TEST_F(TestAccTcpClient, test_server_start_workCount_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = 0;
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
}

TEST_F(TestAccTcpClient, test_server_start_workerStartCpuId_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;
    opts.workerStartCpuId = -2;
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
}

TEST_F(TestAccTcpClient, test_server_start_keepaliveIdleTime_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;
    opts.keepaliveIdleTime = 0;
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
}

TEST_F(TestAccTcpClient, test_server_start_keepaliveProbeTimes_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;
    opts.keepaliveProbeTimes = 0;
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
}

TEST_F(TestAccTcpClient, test_server_start_keepaliveProbeInterval_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;
    opts.keepaliveProbeInterval = 0;
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
}

TEST_F(TestAccTcpClient, test_server_start_newRequestHandle_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;

    auto realServer = dynamic_cast<AccTcpServerDefault*>(mServer.Get());
    for (uint32_t i = 0; i < UNO_16; i++) {
        realServer->newRequestHandle_[i] = nullptr;
    }
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
    initCnt = 0;
}

TEST_F(TestAccTcpClient, test_server_start_requestSentHandle_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;

    auto realServer = dynamic_cast<AccTcpServerDefault*>(mServer.Get());
    for (uint32_t i = 0; i < UNO_16; i++) {
        realServer->requestSentHandle_[i] = nullptr;
    }
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
    initCnt = 0;
}

TEST_F(TestAccTcpClient, test_server_start_linkBrokenHandle_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;

    auto realServer = dynamic_cast<AccTcpServerDefault*>(mServer.Get());
    realServer->linkBrokenHandle_ = nullptr;

    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
    initCnt = 0;
}

TEST_F(TestAccTcpClient, test_server_start_StartWorkers_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;

    MOCKER_CPP(&AccTcpServerDefault::StartWorkers, int32_t(*)(AccTcpServerDefault *)).stubs().will(returnValue(-2));
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
}

TEST_F(TestAccTcpClient, test_server_start_StartListener_validate_should_return_error)
{
    mServer->Stop();
    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;
    MOCKER_CPP(&AccTcpServerDefault::StartListener, int32_t(*)(AccTcpServerDefault *)).stubs().will(returnValue(-2));
    int32_t ret = mServer->Start(opts);
    ASSERT_TRUE(ret != true);
}

TEST_F(TestAccTcpClient, test_AccTcpLinkComplex_validate_should_return_error)
{
    mServer->Stop();
    AccTcpWorkerOptions opts;
    opts.pollingTimeoutMs = UNO_500; /* poll/epoll timeout */
    opts.index = 0;                  /* index of the worker */
    opts.cpuId = -1;                 /* cpu id for bounding */
    opts.threadPriority = 0;         /* thread nice */
    opts.name_ = "AccWrk";           /* worker name */

    AccTcpWorkerPtr mWorker = AccMakeRef<AccTcpWorker>(opts);
    ASSERT_TRUE(mWorker != nullptr);
    std::string ipPort = "127.0.0.1:8100";
    AccTcpLinkComplexDefaultPtr mLink = AccMakeRef<AccTcpLinkComplexDefault>(23, ipPort, AccTcpLinkDefault::NewId());
    ASSERT_TRUE(mLink != nullptr);

    int32_t ret = mLink->Initialize(255, 0, mWorker.Get());
    ASSERT_TRUE(ret != true);
    std::cout << "finish" << std::endl;
}

TEST_F(TestAccTcpClient, test_AccTcpLinkComplex_256_validate_should_return_error)
{
    mServer->Stop();
    AccTcpWorkerOptions opts;
    opts.pollingTimeoutMs = UNO_500;
    opts.index = 0;
    opts.cpuId = -1;
    opts.threadPriority = 0;
    opts.name_ = "AccWrk";

    AccTcpWorkerPtr mWorker = AccMakeRef<AccTcpWorker>(opts);
    ASSERT_TRUE(mWorker != nullptr);
    std::string ipPort = "127.0.0.1:8100";
    AccTcpLinkComplexDefaultPtr mLink = AccMakeRef<AccTcpLinkComplexDefault>(23, ipPort, AccTcpLinkDefault::NewId());
    ASSERT_TRUE(mLink != nullptr);

    int32_t ret = mLink->Initialize(256, 0, mWorker.Get());
    ASSERT_TRUE(ret != true);
    std::cout << "finish" << std::endl;
}

TEST_F(TestAccTcpClient, test_AccTcpLinkComplex_worker_nullptr_validate_should_return_error)
{
    mServer->Stop();
    std::string ipPort = "127.0.0.1:8100";
    AccTcpLinkComplexDefaultPtr mLink = AccMakeRef<AccTcpLinkComplexDefault>(23, ipPort, AccTcpLinkDefault::NewId());
    ASSERT_TRUE(mLink != nullptr);

    int32_t ret = mLink->Initialize(255, 0, nullptr);
    ASSERT_TRUE(ret != true);
    std::cout << "finish" << std::endl;
}

TEST_F(TestAccTcpClient, test_AccTcpLinkComplex_EnqueueAndModifyEpoll_nullptr_should_return_error)
{
    mServer->Stop();
    std::string ipPort = "127.0.0.1:8100";
    AccTcpLinkComplexDefaultPtr mLink = AccMakeRef<AccTcpLinkComplexDefault>(23, ipPort, AccTcpLinkDefault::NewId());
    ASSERT_TRUE(mLink != nullptr);

    AccMsgHeader header;
    AccDataBufferPtr buffer = AccMakeRef<AccDataBuffer>(0);
    int32_t ret = mLink->EnqueueAndModifyEpoll(header, buffer, nullptr);
    ASSERT_TRUE(ret != true);
    std::cout << "finish" << std::endl;
}

TEST_F(TestAccTcpClient, test_AccTcpLinkComplex_EnqueueAndModifyEpoll_mock_should_return_error)
{
    mServer->Stop();
    AccTcpWorkerOptions opts;
    opts.pollingTimeoutMs = UNO_500; /* poll/epoll timeout */
    opts.index = 0;                  /* index of the worker */
    opts.cpuId = -1;                 /* cpu id for bounding */
    opts.threadPriority = 0;         /* thread nice */
    opts.name_ = "AccWrk";           /* worker name */

    AccTcpWorkerPtr mWorker = AccMakeRef<AccTcpWorker>(opts);
    ASSERT_TRUE(mWorker != nullptr);
    std::string ipPort = "127.0.0.1:8100";
    int tFd = ::socket(AF_INET, SOCK_STREAM, 0);

    AccTcpLinkComplexDefaultPtr mLink = AccMakeRef<AccTcpLinkComplexDefault>(tFd, ipPort, AccTcpLinkDefault::NewId());
    ASSERT_TRUE(mLink != nullptr);

    AccMsgHeader header;
    AccDataBufferPtr buffer = AccMakeRef<AccDataBuffer>(0);
    int32_t ret = mLink->Initialize(255, 0, mWorker.Get());
    MOCKER_CPP(&AccLinkedMessageQueue::EnqueueBack,
        int32_t(*)(const AccMsgHeader &, const AccDataBufferPtr &, const AccDataBufferPtr &))
        .stubs()
        .will(returnValue(-4));
    ret = mLink->EnqueueAndModifyEpoll(header, buffer, nullptr);
    ASSERT_TRUE(ret != true);
    std::cout << "finish" << std::endl;
}

// assert_return nullptr
TEST_F(TestAccTcpClient, test_AccLinkedMessageQueue_EnqueueBack_nullptr_should_return_error)
{
    mServer->Stop();
    AccLinkedMessageQueuePtr mQueue = AccMakeRef<AccLinkedMessageQueue>(255);
    ASSERT_TRUE(mQueue != nullptr);
    AccMsgHeader header;
    std::cout << "queue size: " << mQueue->GetSize() << std::endl;
    int32_t ret = mQueue->EnqueueBack(header, nullptr, nullptr);
    ASSERT_TRUE(ret != ACC_OK);
}

TEST_F(TestAccTcpClient, test_tcp_link_connect_send_fd_should_return_error)
{
    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    uint8_t *data = reinterpret_cast<uint8_t *>(buf);
    AccTcpLinkDefaultPtr linnk = AccMakeRef<AccTcpLinkDefault>(-1, "127.0.0.1:8100", AccTcpLinkDefault::NewId());
    int32_t ret = linnk->BlockSend(data, BUFF_SIZE);
    ASSERT_TRUE(ret != ACC_OK);
}

TEST_F(TestAccTcpClient, test_tcp_link_connect_send_data_should_return_error)
{
    int tFd = ::socket(AF_INET, SOCK_STREAM, 0);
    AccTcpLinkDefaultPtr linnk = AccMakeRef<AccTcpLinkDefault>(tFd, "127.0.0.1:8100", AccTcpLinkDefault::NewId());
    int32_t ret = linnk->BlockSend(nullptr, BUFF_SIZE);
    ASSERT_TRUE(ret != ACC_OK);
}

TEST_F(TestAccTcpClient, test_tcp_link_connect_send_len_should_return_error)
{
    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    uint8_t *data = reinterpret_cast<uint8_t *>(buf);
    int tFd = ::socket(AF_INET, SOCK_STREAM, 0);
    AccTcpLinkDefaultPtr linnk = AccMakeRef<AccTcpLinkDefault>(tFd, "127.0.0.1:8100", AccTcpLinkDefault::NewId());
    int32_t ret = linnk->BlockSend(data, 0);
    ASSERT_TRUE(ret != ACC_OK);
}

TEST_F(TestAccTcpClient, test_tcp_link_connect_receive_fd_should_return_error)
{
    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    uint8_t *data = reinterpret_cast<uint8_t *>(buf);
    AccTcpLinkDefaultPtr linnk = AccMakeRef<AccTcpLinkDefault>(-1, "127.0.0.1:8100", AccTcpLinkDefault::NewId());
    int32_t ret = linnk->BlockRecv(data, BUFF_SIZE);
    ASSERT_TRUE(ret != ACC_OK);
}

TEST_F(TestAccTcpClient, test_tcp_link_connect_receive_data_should_return_error)
{
    int tFd = ::socket(AF_INET, SOCK_STREAM, 0);
    AccTcpLinkDefaultPtr linnk = AccMakeRef<AccTcpLinkDefault>(tFd, "127.0.0.1:8100", AccTcpLinkDefault::NewId());
    int32_t ret = linnk->BlockRecv(nullptr, BUFF_SIZE);
    ASSERT_TRUE(ret != ACC_OK);
}

TEST_F(TestAccTcpClient, test_tcp_link_connect_receive_len_should_return_error)
{
    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    uint8_t *data = reinterpret_cast<uint8_t *>(buf);
    int tFd = ::socket(AF_INET, SOCK_STREAM, 0);
    AccTcpLinkDefaultPtr linnk = AccMakeRef<AccTcpLinkDefault>(tFd, "127.0.0.1:8100", AccTcpLinkDefault::NewId());
    int32_t ret = linnk->BlockRecv(data, 0);
    ASSERT_TRUE(ACC_OK != ret);
}

TEST_F(TestAccTcpClient, test_tcp_link_EnableNoBlocking_fd_should_return_error)
{
    AccTcpLinkDefaultPtr linnk = AccMakeRef<AccTcpLinkDefault>(-1, "127.0.0.1:8100", AccTcpLinkDefault::NewId());
    int32_t ret = linnk->EnableNoBlocking();
    ASSERT_TRUE(ACC_OK != ret);
}

TEST_F(TestAccTcpClient, link_send_post_process)
{
    int tFd = ::socket(AF_INET, SOCK_STREAM, 0);
    SSL *ssl = nullptr;
    AccTcpLinkComplexDefaultPtr tmpLink =
        AccMakeRef<AccTcpLinkComplexDefault>(tFd, "127.0.0.1", AccTcpLinkDefault::NewId(), ssl);
    auto retEconnreset = tmpLink->SendPostProcess(ECONNRESET);
    ASSERT_TRUE(ACC_LINK_ERROR == retEconnreset);
    auto retEagain = tmpLink->SendPostProcess(EAGAIN);
    ASSERT_TRUE(ACC_LINK_EAGAIN == retEagain);
    auto retLinkError = tmpLink->SendPostProcess(0);
    ASSERT_TRUE(ACC_LINK_ERROR == retLinkError);
}

TEST_F(TestAccTcpClient, fileUtils_regularFilePath_voidPath)
{
    std::string errMsg;
    auto ret = FileValidator::RegularFilePath("", "/", errMsg);
    ASSERT_EQ(ret, false);
    ret = FileValidator::RegularFilePath("/", "", errMsg);
    ASSERT_EQ(ret, false);
}

TEST_F(TestAccTcpClient, fileUtils_regularFilePath_pathTooLong)
{
    std::string errMsg;
    std::string pathTooLong(PATH_MAX + 1, 'A');
    auto ret = FileValidator::RegularFilePath(pathTooLong, "/", errMsg);
    ASSERT_EQ(ret, false);
    ret = FileValidator::RegularFilePath("/", pathTooLong, errMsg);
    ASSERT_EQ(ret, false);
}

TEST_F(TestAccTcpClient, fileUtils_checkDataSize)
{
    long oversize = g_defaultMaxDataSize + 10;
    auto ret = CheckDataSize(oversize);
    ASSERT_EQ(ret, false);
}

TEST_F(TestAccTcpClient, fileUtils_file_not_exist)
{
    std::string invalidPath = "/path/with/invalid*chars";
    auto ret = ock::FileUtil::GetFileSize(invalidPath);
    ASSERT_EQ(ret, 0);
}

TEST_F(TestAccTcpClient, listener_bad_start)
{
    AccTcpListenerPtr mListener = new (std::nothrow) AccTcpListener("127.0.0.1", 9966L, true, false, nullptr);

    mListener->started_ = true;
    auto result = mListener->Start();
    ASSERT_EQ(result, ACC_OK);

    mListener->started_ = false;
    mListener->connHandler_ = nullptr;
    result = mListener->Start();
    ASSERT_EQ(result, ACC_INVALID_PARAM);

    mListener->Stop();
}

TEST_F(TestAccTcpClient, LoadDynamicLib)
{
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", LISTEN_PORT);
    ASSERT_TRUE(mClient != nullptr);
    int32_t result = mClient->Connect(req);
    ASSERT_EQ(ACC_OK, result);

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    uint8_t *data = reinterpret_cast<uint8_t *>(buf);
    result = mClient->Send(TTP_OP_HEARTBEAT_SEND, data, BUFF_SIZE);
    ASSERT_EQ(ACC_OK, result);
    result = mClient->LoadDynamicLib("/");
    ASSERT_EQ(ACC_ERROR, result);
    mClient->SetLocalIp("/");

    sleep(1);
    mClient->Disconnect();
}

TEST_F(TestAccTcpClient, test_server_LoadDynamicLib)
{
    const std::string nextIp = "127.0.0.1";
    uint16_t nextPort = LISTEN_PORT;
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpLinkComplexPtr nextLink;
    int32_t ret = mServer->ConnectToPeerServer(nextIp, nextPort, req, nextLink);
    ASSERT_EQ(ACC_OK, ret);
    auto result = mServer->LoadDynamicLib("/");
    ASSERT_EQ(ACC_ERROR, result);
}

TEST_F(TestAccTcpClient, link_complex_EnqueueFront)
{
    AccLinkedMessageQueuePtr mQueue = new (std::nothrow) AccLinkedMessageQueue(100);
    AccLinkedMessageNode *nullnode = nullptr;
    auto res = mQueue->EnqueueFront(nullnode);
    ASSERT_EQ(res, ACC_INVALID_PARAM);
}

TEST_F(TestAccTcpClient, ssl_shutdown_test_nullptr)
{
    SSL *ssl = nullptr;
    ASSERT_EQ(AccCommonUtil::SslShutdownHelper(ssl), ACC_ERROR);
}

TEST_F(TestAccTcpClient, ssl_shutdown_test_invalid_ret_val)
{
    const int invalidRetVal = 3;
    SSL *ssl = nullptr;
    ssl = reinterpret_cast<SSL *>(&ssl);
    MOCKER_CPP(&OpenSslApiWrapper::SslShutdown, int (*)(SSL *)).stubs().will(returnValue(invalidRetVal));
    ASSERT_EQ(AccCommonUtil::SslShutdownHelper(ssl), ACC_ERROR);
}

TEST_F(TestAccTcpClient, ssl_shutdown_test_shutdown_retry)
{
    SSL *ssl = nullptr;
    ssl = reinterpret_cast<SSL *>(&ssl);
    MOCKER_CPP(&OpenSslApiWrapper::SslShutdown, int (*)(SSL *)).stubs().will(returnValue(0));
    ASSERT_EQ(AccCommonUtil::SslShutdownHelper(ssl), ACC_ERROR);
}

TEST_F(TestAccTcpClient, ssl_shutdown_test_shutdown_fail)
{
    const int failRetVal = -5;
    SSL *ssl = nullptr;
    ssl = reinterpret_cast<SSL *>(&ssl);
    MOCKER_CPP(&OpenSslApiWrapper::SslShutdown, int (*)(SSL *)).stubs().will(returnValue(failRetVal));
    MOCKER_CPP(&OpenSslApiWrapper::SslGetError, int (*)(const SSL *, int)).stubs().will(returnValue(-1));
    ASSERT_EQ(AccCommonUtil::SslShutdownHelper(ssl), ACC_ERROR);
}
}