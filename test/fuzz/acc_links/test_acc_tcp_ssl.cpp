/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <iostream>
#include <cstdlib>
#include <string>
#include <unistd.h>
#define protected public
#define private public
#include "acc_common_util.h"
#include "openssl_api_wrapper.h"
#include "acc_tcp_client.h"
#include "acc_tcp_worker.h"
#include "acc_tcp_link_complex_default.h"
#include "acc_tcp_ssl_helper.h"
#include "acc_tcp_ssl_helper.cpp"
#include "acc_tcp_server.h"
#include "secodefuzz/secodeFuzz.h"
#include "dt_fuzz.h"

#undef private
#undef protected
namespace {
using namespace ock::acc;
const int BUFF_SIZE = 32;
const int LISTEN_PORT = 8100;
const int LINK_SEND_QUEUE_SIZE = 100;
const int WORK_COUNT = 4;
const int PATH_MAX_TEST = 248;

bool GetCertPath(std::string &execPath) noexcept
{
    std::string linkedPath = "/proc/" + std::to_string(getpid()) + "/exe";
    std::string realPath;
    realPath.resize(PATH_MAX_TEST);
    auto size = readlink(linkedPath.c_str(), &realPath[0], realPath.size());
    if (size < 0 || size >= PATH_MAX_TEST) {
        std::cout << "get lib path failed : invalid size " << size << std::endl;
        return false;
    }

    // (base)/build/bin/test_acc_links
    realPath[size] = '\0';
    std::string path{realPath};

    std::string::size_type position = path.find_last_of('/');
    if (position == std::string::npos) {
        std::cout << "get lib path failed : invalid folder path." << std::endl;
        return false;
    }
    // (base)/build/bin
    path = path.substr(0, position);

    position = path.find_last_of('/');
    if (position == std::string::npos) {
        std::cout << "get lib path failed : invalid folder path." << std::endl;
        return false;
    }
    // (base)/build
    path = path.substr(0, position);

    position = path.find_last_of('/');
    if (position == std::string::npos) {
        std::cout << "get lib path failed : invalid folder path." << std::endl;
        return false;
    }
    // (base)/
    path = path.substr(0, position);
    path.append("/test/ut/openssl_cert");

    // (base)/test/ut/openssl_cert
    execPath = path;
    std::cout << "Get cert path " << path << std::endl;
    return true;
}

enum OpCode : int32_t {
    TTP_OP_HEARTBEAT_SEND = 0,  // processor send heart beat to controller
    TTP_OP_HEARTBEAT_REPLY,
};

static std::unordered_map<int32_t, AccTcpLinkComplexPtr> g_rankLinkMap;
static void *g_cbCtx = nullptr;
class TestAccTcpSslClientFuzz : public testing::Test {

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

    int32_t HbReplyCallBack(AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx)
    {
        g_cbCtx = (cbCtx == nullptr) ? nullptr : cbCtx->DataPtrVoid();
        if (result != MSG_SENT) {
            return 1;
        }
        return 0;
    }

    int32_t HandleNewConnection(const AccConnReq &req, const AccTcpLinkComplexPtr &link)
    {
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
protected:
    static AccTcpServerPtr mServer;
};

AccTcpServerPtr TestAccTcpSslClientFuzz::mServer = nullptr;

void TestAccTcpSslClientFuzz::SetUp()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);
    mServer = AccTcpServer::Create();
    ASSERT_TRUE(mServer != nullptr);
    // add server handler
    auto hbMethod = [this](const AccTcpRequestContext &context) {
        return HandleHeartBeat(context);
    };
    mServer->RegisterNewRequestHandler(TTP_OP_HEARTBEAT_SEND, hbMethod);

    // add sent handler
    auto hdSentMethod = [this](AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx) {
        return HbReplyCallBack(result, header, cbCtx);
    };
    mServer->RegisterRequestSentHandler(TTP_OP_HEARTBEAT_REPLY, hdSentMethod);

    // add link handle
    auto linkMethod = [this](const AccConnReq &req, const AccTcpLinkComplexPtr &link) {
        return HandleNewConnection(req, link);
    };
    mServer->RegisterNewLinkHandler(linkMethod);

    auto linkBrokenMethod = [this](const AccTcpLinkComplexPtr &link) {
        return HandleLinkBroken(link);
    };
    mServer->RegisterLinkBrokenHandler(linkBrokenMethod);

    AccTcpServerOptions opts;
    opts.enableListener = true;
    opts.linkSendQueueSize = LINK_SEND_QUEUE_SIZE;
    opts.listenIp = "127.0.0.1";
    opts.listenPort = LISTEN_PORT;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = WORK_COUNT;

    char buff[1024];
    if (getcwd(buff, sizeof(buff)) != nullptr) {
        std::cout << "Current directory: " << buff << std::endl;
    } else {
        perror("getcwd() error");
    }
    std::string dynLibPath = buff;
    dynLibPath.append("/../../../output/3rdparty/openssl/lib/");
    OpenSslApiWrapper::Load(dynLibPath);

    AccTlsOption tslOption;
    tslOption.enableTls = false;
    (void)mServer->Start(opts, tslOption);
}

void TestAccTcpSslClientFuzz::TearDown()
{
    mServer->Stop();
    g_rankLinkMap.clear();
    GlobalMockObject::verify();
}

TEST_F(TestAccTcpSslClientFuzz, test_client_set_ssl)
{
    char fuzzName[] = "test_client_set_ssl";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        AccConnReq req{};
        req.rankId = 0;
        req.magic = 0;
        req.version = 1;
        AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", 8100);
        ASSERT_TRUE(mClient != nullptr);

        AccTlsOption tslOption;
        tslOption.enableTls = false;
        mClient->SetSslOption(tslOption);

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
    DT_FUZZ_END()
}
}  // namespace