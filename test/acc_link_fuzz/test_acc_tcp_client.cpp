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
#include "secodefuzz/secodeFuzz.h"
#include "dt_fuzz.h"

namespace {
const int BUFF_SIZE = 32;
const int LISTEN_PORT = 8100;
const int LINK_SEND_QUEUE_SIZE = 100;
const int WORKER_COUNT = 4;
const int TIMEOUT_TIME = 10000;
using namespace ock::acc;

enum OpCode : int32_t {
    TTP_OP_REGISTER = 0,  // register
    TTP_OP_REGISTER_REPLY,
    TTP_OP_HEARTBEAT_SEND,  // processor send heart beat to controller
    TTP_OP_HEARTBEAT_REPLY,
    TTP_OP_CKPT_SEND,    // controller send ckpt action to processor
    TTP_OP_CKPT_REPLY,   // processor reply result of ckpt to controller
    TTP_OP_CTRL_NOTIFY,  // controller send back up controller to processor
    TTP_OP_RENAME,       // controller send rename request to processor
    TTP_OP_RENAME_REPLY,
    TTP_OP_EXIT,
    TTP_OP_BUTT,
};

static std::unordered_map<int32_t, AccTcpLinkComplexPtr> g_rankLinkMap;
uint32_t connectCnt{0};
static void *g_cbCtx = nullptr;
std::string dynLibPath;
AccTcpServerOptions opts;

class TestAccTcpClient : public testing::Test {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();

public:
    void SetUp() override;
    void TearDown() override;

public:
    int32_t HandleHeartBeat(const AccTcpRequestContext &context)
    {
        if (context.DataLen() != BUFF_SIZE) {
            return 1;
        }
        std::cout << "receive data len match" << std::endl;
        AccDataBufferPtr buffer = AccMakeRef<AccDataBuffer>(0);
        if (buffer == nullptr) {
            std::cout << "data buffer is nullptr" << std::endl;
            return 1;
        }
        return context.Reply(0, buffer);
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

    void ClientHandleCtrlNotify(uint8_t *data, uint32_t len)
    {
        clientRecvLen = len;
        isClientRecv.store(true);
    }

protected:
    static AccTcpServerPtr mServer;
    std::atomic<bool> isClientRecv{false};
    uint32_t clientRecvLen;
};

AccTcpServerPtr TestAccTcpClient::mServer = nullptr;

void TestAccTcpClient::SetUpTestSuite()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);
    mServer = AccTcpServer::Create();
    ASSERT_TRUE(mServer != nullptr);
}

void TestAccTcpClient::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

void TestAccTcpClient::SetUp()
{
    // add server handler
    auto hbMethod = [this](const AccTcpRequestContext &context) {
        return HandleHeartBeat(context);
    };
    mServer->RegisterNewRequestHandler(TTP_OP_HEARTBEAT_SEND, hbMethod);
    auto dumpReplyMethod = [this](const AccTcpRequestContext &context) {
        return HandleDumpReply(context);
    };
    mServer->RegisterNewRequestHandler(TTP_OP_CKPT_REPLY, dumpReplyMethod);
    auto renameReplyMethod = [this](const AccTcpRequestContext &context) {
        return HandleRenameReply(context);
    };
    mServer->RegisterNewRequestHandler(TTP_OP_RENAME_REPLY, renameReplyMethod);
    auto registerMethod = [this](const AccTcpRequestContext &context) {
        return HandleRegister(context);
    };
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

    auto linkBrokenMethod = [this](const AccTcpLinkComplexPtr &link) {
        return HandleLinkBroken(link);
    };
    mServer->RegisterLinkBrokenHandler(linkBrokenMethod);

    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORKER_COUNT;

    char buff[1024];
    if (getcwd(buff, sizeof(buff)) != nullptr) {
        std::cout << "Current directory: " << buff << std::endl;
    } else {
        perror("getcwd() error");
    }
    dynLibPath = buff;
    dynLibPath.append("/../../../output/3rdparty/openssl/lib/");
}

void TestAccTcpClient::TearDown()
{
    GlobalMockObject::verify();
}

int32_t LinkClose(AccTcpLinkDefault *link, void *data, uint32_t len)
{
    std::cout << "link close stub" << std::endl;
    link->Close();
    return ACC_LINK_NEED_RECONN;
}

TEST_F(TestAccTcpClient, test_client_connect_send_should_return_ok)
{
    char fuzzName[] = "test_client_connect_send_should_return_ok";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_EQ(mServer->Start(opts), ACC_OK);
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
        mServer->Stop();
        g_rankLinkMap.clear();
    }
    DT_FUZZ_END()
}

TEST_F(TestAccTcpClient, test_server_send_should_return_ok)
{
    char fuzzName[] = "test_server_send_should_return_ok";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_EQ(mServer->Start(opts), ACC_OK);
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
        if (buffer == nullptr) {
            free(data);
            ASSERT_TRUE(buffer != nullptr);
        }

        for (auto it = g_rankLinkMap.begin(); it != g_rankLinkMap.end(); ++it) {
            AccTcpLinkComplexPtr link = it->second;
            result = link->NonBlockSend(TTP_OP_CTRL_NOTIFY, buffer, nullptr);
            ASSERT_EQ(ACC_OK, result);
        }
        std::cout << "server send msg" << std::endl;
        sleep(2);
        mClient->Disconnect();
        mServer->Stop();
        g_rankLinkMap.clear();
    }
    DT_FUZZ_END()
}

TEST_F(TestAccTcpClient, test_client_recv_by_polling)
{
    char fuzzName[] = "test_client_recv_by_polling";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_EQ(mServer->Start(opts), ACC_OK);
        AccConnReq req{};
        req.rankId = 0;
        req.magic = 0;
        req.version = 1;
        AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", LISTEN_PORT);
        ASSERT_TRUE(mClient != nullptr);
        int32_t result = mClient->Connect(req);
        ASSERT_EQ(ACC_OK, result);

        auto notifyMethod = [this](uint8_t *data, uint32_t len) {
            return ClientHandleCtrlNotify(data, len);
        };
        mClient->RegisterNewRequestHandler(TTP_OP_CTRL_NOTIFY, notifyMethod);

        mClient->SetReceiveTimeout(TIMEOUT_TIME);  // 1s
        mClient->StartPolling();

        void *data = malloc(BUFF_SIZE);
        ASSERT_TRUE(data != nullptr);
        memset(data, 0, BUFF_SIZE);
        AccDataBufferPtr buffer = AccMakeRef<AccDataBuffer>(reinterpret_cast<uint8_t *>(data), BUFF_SIZE);
        if (buffer == nullptr) {
            free(data);
            ASSERT_TRUE(buffer != nullptr);
        }

        for (auto it = g_rankLinkMap.begin(); it != g_rankLinkMap.end(); ++it) {
            AccTcpLinkComplexPtr link = it->second;
            result = link->NonBlockSend(TTP_OP_CTRL_NOTIFY, buffer, nullptr);
            ASSERT_EQ(ACC_OK, result);
        }
        std::cout << "server send msg" << std::endl;
        sleep(2);
        ASSERT_TRUE(isClientRecv.load());
        ASSERT_TRUE(clientRecvLen == BUFF_SIZE);
        mClient->Disconnect();
        mClient->Destroy();
        mServer->Stop();
        g_rankLinkMap.clear();
    }
    DT_FUZZ_END()
}

TEST_F(TestAccTcpClient, test_client_change_ip)
{
    char fuzzName[] = "test_client_change_ip";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_EQ(mServer->Start(opts), ACC_OK);
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
        mServer->Stop();
        g_rankLinkMap.clear();
    }
    DT_FUZZ_END()
}

TEST_F(TestAccTcpClient, test_server_connect_to_peer_server_should_return_ok)
{
    char fuzzName[] = "test_server_connect_to_peer_server_should_return_ok";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_EQ(mServer->Start(opts), ACC_OK);
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
        mServer->Stop();
        g_rankLinkMap.clear();
    }
    DT_FUZZ_END()
}

TEST_F(TestAccTcpClient, test_client_LoadDynamicLib)
{
    char fuzzName[] = "test_client_LoadDynamicLib";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_EQ(mServer->Start(opts), ACC_OK);
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
        result = mClient->LoadDynamicLib(dynLibPath);
        ASSERT_EQ(ACC_OK, result);
        mClient->SetLocalIp("/");

        sleep(1);
        mClient->Disconnect();
        mServer->Stop();
        g_rankLinkMap.clear();
    }
    DT_FUZZ_END()
}

TEST_F(TestAccTcpClient, test_server_LoadDynamicLib)
{
    char fuzzName[] = "test_server_LoadDynamicLib";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_EQ(mServer->Start(opts), ACC_OK);
        const std::string nextIp = "127.0.0.1";
        uint16_t nextPort = LISTEN_PORT;
        AccConnReq req{};
        req.rankId = 0;
        req.magic = 0;
        req.version = 1;
        AccTcpLinkComplexPtr nextLink;
        int32_t ret = mServer->ConnectToPeerServer(nextIp, nextPort, req, nextLink);
        ASSERT_EQ(ACC_OK, ret);
        auto result = mServer->LoadDynamicLib(dynLibPath);
        ASSERT_EQ(ACC_OK, result);
        mServer->Stop();
        g_rankLinkMap.clear();
    }
    DT_FUZZ_END()
}
}  // namespace