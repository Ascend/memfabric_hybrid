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
#include "openssl_api_dl.cpp"
#include "acc_tcp_client.h"
#include "acc_tcp_worker.h"
#include "acc_tcp_link_complex_default.h"
#include "acc_tcp_ssl_helper.h"
#include "acc_tcp_ssl_helper.cpp"
#include "acc_tcp_server.h"

#undef private
#undef protected
namespace {
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
using namespace ock::acc;
const int BUFF_SIZE = 32;
const int LISTEN_PORT = 8100;
const int LINK_SEND_QUEUE_SIZE = 100;
const int WORK_COUNT = 4;
const int PATH_MAX_TEST = 248;
class TestAccTcpSslHelper : public testing::Test {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();

public:
    void SetUp() override;
    void TearDown() override;
};

int decrypt_handler_for_test(const std::string &cipherText, char *plainText, size_t &plainTextLen)
{
    const char* decryptText = cipherText.c_str();   // pk pwd is empty, copy cipher directy
    if (cipherText.length() >= plainTextLen) {
        return ACC_ERROR;
    }
    strncpy(plainText, decryptText, cipherText.length());
    plainText[cipherText.length()] = '\0';
    return ACC_OK;
}

void print_tls_option(AccTlsOption &tlsOption)
{
    std::cout << "enableTls:" << tlsOption.enableTls << std::endl;
    std::cout << "tlsTopPath:" << tlsOption.tlsTopPath << std::endl;
    std::cout << "tlsCert:" << tlsOption.tlsCert << std::endl;
    std::cout << "tlsCrlPath:" << tlsOption.tlsCrlPath << std::endl;
    std::cout << "tlsCaPath:" << tlsOption.tlsCaPath << std::endl;
    if (!tlsOption.tlsCaFile.empty()) {
        std::cout << "tlsCaFile:" << *tlsOption.tlsCaFile.begin() << std::endl;
    }
    if (!tlsOption.tlsCrlFile.empty()) {
        std::cout << "tlsCrlFile:" << *tlsOption.tlsCrlFile.begin() << std::endl;
    }
    std::cout << "tlsPk:" << tlsOption.tlsPk << std::endl;
    std::cout << "tlsPkPwd:" << tlsOption.tlsPkPwd << std::endl;
}

void TestAccTcpSslHelper::SetUpTestSuite() {}

void TestAccTcpSslHelper::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

void TestAccTcpSslHelper::SetUp() {}

void TestAccTcpSslHelper::TearDown()
{
    GlobalMockObject::verify();
}

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

// *********************************TEST_F*************************

TEST_F(TestAccTcpSslHelper, start)
{
    std::string certPath;

    GetCertPath(certPath);

    AccTcpSslHelperPtr tmpHelperPtr = AccMakeRef<AccTcpSslHelper>();
    ASSERT_TRUE(tmpHelperPtr != nullptr);

    char buff[1024];
    if (getcwd(buff, sizeof(buff)) != nullptr) {
        std::cout << "Current directory: " << buff << std::endl;
    } else {
        perror("getcwd() error");
    }
    std::string dynLibPath = buff;
    dynLibPath.append("/../3rdparty/openssl/lib/");
    OpenSslApiWrapper::Load(dynLibPath);
    auto tmpSslCtx = OpenSslApiWrapper::SslCtxNew(OpenSslApiWrapper::TlsMethod());
    ASSERT_TRUE(tmpSslCtx != nullptr);

    AccTlsOption tlsOption;
    tlsOption.enableTls = true;
    std::string errStr;
    ASSERT_FALSE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);

    tlsOption.tlsTopPath = certPath;
    ASSERT_FALSE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);

    tlsOption.tlsCert = "/cert/cert.pem";
    ASSERT_FALSE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);

    tlsOption.tlsCaPath = "/CA/";
    ASSERT_FALSE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);

    tlsOption.tlsCaFile.insert("ca_cert.pem");
    ASSERT_FALSE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);

    tlsOption.tlsPk = "/cert/key.pem";
    ASSERT_FALSE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);

    tlsOption.tlsCrlFile.insert("crl.pem");
    ASSERT_FALSE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);

    tlsOption.tlsCrlPath = "/crl/";
    ASSERT_FALSE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);

    tlsOption.tlsPkPwd = "/key_pwd.txt";
    ASSERT_TRUE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);

    print_tls_option(tlsOption);

    const char *days = "9";
    const char *checkPeriod = "24";
    ::setenv("ACCLINK_CERT_CHECK_AHEAD_DAYS", days, 1);
    ::setenv("ACCLINK_CHECK_PERIOD_HOURS", checkPeriod, 1);

    tmpHelperPtr->RegisterDecryptHandler(decrypt_handler_for_test);
    auto result = tmpHelperPtr->Start(tmpSslCtx, tlsOption);
    ASSERT_TRUE(result == ACC_OK);

    unsetenv("ACCLINK_CERT_CHECK_AHEAD_DAYS");
    unsetenv("ACCLINK_CHECK_PERIOD_HOURS");

    std::string invalidIpv4 = "2666.6666.6666.6666";
    bool ipCheck = AccCommonUtil::IsValidIPv4(invalidIpv4);
    ASSERT_EQ(ipCheck, false);

    tmpHelperPtr->Stop();
}

TEST_F(TestAccTcpSslHelper, load_crl)
{
    std::string certPath;
    GetCertPath(certPath);

    AccTcpSslHelperPtr tmpHelperPtr = AccMakeRef<AccTcpSslHelper>();
    ASSERT_TRUE(tmpHelperPtr != nullptr);

    char buff[1024];
    if (getcwd(buff, sizeof(buff)) != nullptr) {
        std::cout << "Current directory: " << buff << std::endl;
    } else {
        perror("getcwd() error");
    }
    std::string dynLibPath = buff;
    dynLibPath.append("/../3rdparty/openssl/lib/");
    OpenSslApiWrapper::Load(dynLibPath);
    auto tmpSslCtx = OpenSslApiWrapper::SslCtxNew(OpenSslApiWrapper::TlsMethod());
    ASSERT_TRUE(tmpSslCtx != nullptr);

    AccTlsOption tlsOption;
    tlsOption.enableTls = true;
    tlsOption.tlsTopPath = certPath;
    tlsOption.tlsCert = "/cert/cert.pem";
    tlsOption.tlsCaPath = "/CA/";
    tlsOption.tlsCaFile.insert("ca_cert.pem");
    tlsOption.tlsPk = "/cert/key.pem";
    tlsOption.tlsCrlFile.insert("crl.pem");
    tlsOption.tlsCrlPath = "/crl/";
    tlsOption.tlsPkPwd = "/key_pwd.txt";
    print_tls_option(tlsOption);
    std::string errStr;
    ASSERT_TRUE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);

    const char *days = "2";
    const char *checkPeriod = "33";
    ::setenv("ACCLINK_CERT_CHECK_AHEAD_DAYS", days, 1);
    ::setenv("ACCLINK_CHECK_PERIOD_HOURS", checkPeriod, 1);
    tmpHelperPtr->RegisterDecryptHandler(decrypt_handler_for_test);
    auto result = tmpHelperPtr->Start(tmpSslCtx, tlsOption);
    unsetenv("ACCLINK_CERT_CHECK_AHEAD_DAYS");
    unsetenv("ACCLINK_CHECK_PERIOD_HOURS");
    ASSERT_TRUE(result == ACC_OK);

    tmpHelperPtr->Stop();
}

TEST_F(TestAccTcpSslHelper, bad_PkPwd)
{
    std::string certPath;
    GetCertPath(certPath);

    AccTcpSslHelperPtr tmpHelperPtr = AccMakeRef<AccTcpSslHelper>();
    ASSERT_TRUE(tmpHelperPtr != nullptr);

    char buff[1024];
    if (getcwd(buff, sizeof(buff)) != nullptr) {
        std::cout << "Current directory: " << buff << std::endl;
    } else {
        perror("getcwd() error");
    }
    std::string dynLibPath = buff;
    dynLibPath.append("/../3rdparty/openssl/lib/");
    OpenSslApiWrapper::Load(dynLibPath);
    auto tmpSslCtx = OpenSslApiWrapper::SslCtxNew(OpenSslApiWrapper::TlsMethod());
    ASSERT_TRUE(tmpSslCtx != nullptr);

    AccTlsOption tlsOption;
    tlsOption.enableTls = true;
    tlsOption.tlsTopPath = certPath;
    tlsOption.tlsCert = "/cert/cert.pem";
    tlsOption.tlsCaPath = "/CA/";
    tlsOption.tlsCaFile.insert("ca_cert.pem");
    tlsOption.tlsPk = "/cert/key.pem";
    tlsOption.tlsPkPwd = "/key_pwd.txt";
    print_tls_option(tlsOption);
    std::string errStr;
    ASSERT_TRUE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);

    const char *days = "error_val";
    const char *checkPeriod = "illegal_val";
    ::setenv("ACCLINK_CERT_CHECK_AHEAD_DAYS", days, 1);
    ::setenv("ACCLINK_CHECK_PERIOD_HOURS", checkPeriod, 1);
    tmpHelperPtr->RegisterDecryptHandler(
        [&](const std::string &cipherText, char *plainText, size_t &plainTextLen) -> int {
            const char* decryptText = "Hello World";
            size_t required_len = strlen(decryptText) + 1;
            if (required_len < plainTextLen) {
                return ACC_ERROR;
            }
            strncpy(plainText, decryptText, plainTextLen - 1);
            plainText[plainTextLen - 1] = '\0';
            return ACC_OK;
        });
    auto result = tmpHelperPtr->Start(tmpSslCtx, tlsOption);
    unsetenv("ACCLINK_CERT_CHECK_AHEAD_DAYS");
    unsetenv("ACCLINK_CHECK_PERIOD_HOURS");

    ASSERT_TRUE(result == ACC_ERROR);
    tmpHelperPtr->EraseDecryptData();

    tmpHelperPtr->Stop();
}

TEST_F(TestAccTcpSslHelper, difftime_less_than_zero)
{
    std::string certPath;
    GetCertPath(certPath);

    AccTcpSslHelperPtr tmpHelperPtr = AccMakeRef<AccTcpSslHelper>();
    ASSERT_TRUE(tmpHelperPtr != nullptr);

    char buff[1024];
    if (getcwd(buff, sizeof(buff)) != nullptr) {
        std::cout << "Current directory: " << buff << std::endl;
    } else {
        perror("getcwd() error");
    }
    std::string dynLibPath = buff;
    dynLibPath.append("/../3rdparty/openssl/lib/");
    OpenSslApiWrapper::Load(dynLibPath);
    auto tmpSslCtx = OpenSslApiWrapper::SslCtxNew(OpenSslApiWrapper::TlsMethod());
    ASSERT_TRUE(tmpSslCtx != nullptr);

    AccTlsOption tlsOption;
    tlsOption.enableTls = true;
    tlsOption.tlsTopPath = certPath;
    tlsOption.tlsCert = "/cert/cert.pem";
    tlsOption.tlsCaPath = "/CA/";
    tlsOption.tlsCaFile.insert("ca_cert.pem");
    tlsOption.tlsPk = "/cert/key.pem";
    tlsOption.tlsPkPwd = "/key_pwd.txt";
    print_tls_option(tlsOption);
    std::string errStr;
    ASSERT_TRUE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);
    double diff = -100.0;
    MOCKER_CPP(&difftime, double (*)(time_t, time_t)).expects(atLeast(1)).will(returnValue(diff));
    tmpHelperPtr->RegisterDecryptHandler(decrypt_handler_for_test);
    auto result = tmpHelperPtr->Start(tmpSslCtx, tlsOption);
    ASSERT_TRUE(result == ACC_ERROR);

    tmpHelperPtr->Stop();
}

TEST_F(TestAccTcpSslHelper, difftime_less_than_required)
{
    std::string certPath;
    GetCertPath(certPath);

    AccTcpSslHelperPtr tmpHelperPtr = AccMakeRef<AccTcpSslHelper>();
    ASSERT_TRUE(tmpHelperPtr != nullptr);

    char buff[1024];
    if (getcwd(buff, sizeof(buff)) != nullptr) {
        std::cout << "Current directory: " << buff << std::endl;
    } else {
        perror("getcwd() error");
    }
    std::string dynLibPath = buff;
    dynLibPath.append("/../3rdparty/openssl/lib/");
    OpenSslApiWrapper::Load(dynLibPath);
    auto tmpSslCtx = OpenSslApiWrapper::SslCtxNew(OpenSslApiWrapper::TlsMethod());
    ASSERT_TRUE(tmpSslCtx != nullptr);

    AccTlsOption tlsOption;
    tlsOption.enableTls = true;
    tlsOption.tlsTopPath = certPath;
    tlsOption.tlsCert = "/cert/cert.pem";
    tlsOption.tlsCaPath = "/CA/";
    tlsOption.tlsCaFile.insert("ca_cert.pem");
    tlsOption.tlsPk = "/cert/key.pem";
    tlsOption.tlsPkPwd = "/key_pwd.txt";
    print_tls_option(tlsOption);
    std::string errStr;
    ASSERT_TRUE(AccCommonUtil::CheckTlsOptions(tlsOption) == ACC_OK);

    MOCKER_CPP(&difftime, double (*)(time_t, time_t)).expects(atLeast(1)).will(returnValue(1.0));
    tmpHelperPtr->RegisterDecryptHandler(decrypt_handler_for_test);
    auto result = tmpHelperPtr->Start(tmpSslCtx, tlsOption);
    ASSERT_TRUE(result == ACC_OK);

    tmpHelperPtr->Stop();
}

// **********************************************************
enum OpCode : int32_t {
    TTP_OP_HEARTBEAT_SEND = 0, // processor send heart beat to controller
    TTP_OP_HEARTBEAT_REPLY,
};

static std::unordered_map<int32_t, AccTcpLinkComplexPtr> g_rankLinkMap;
static void *g_cbCtx = nullptr;
class TestAccTcpSslClient : public testing::Test {
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
            std::cout << "receive data len mis match" << std::endl;
            return 1;
        }
        std::cout << "receive data len match" << std::endl;
        AccDataBufferPtr buffer = AccMakeRef<AccDataBuffer>(0);
        if (buffer == nullptr || buffer->data_ == nullptr) {
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

    int32_t BroadcastCallBack(AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx)
    {
        g_cbCtx = (cbCtx == nullptr) ? nullptr : cbCtx->DataPtrVoid();
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

AccTcpServerPtr TestAccTcpSslClient::mServer = nullptr;

void TestAccTcpSslClient::SetUpTestSuite()
{
    mServer = AccTcpServer::Create();
    ASSERT_TRUE(mServer != nullptr);
}

void TestAccTcpSslClient::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

void TestAccTcpSslClient::SetUp()
{
    // add server handler
    auto hbMethod = [this](const AccTcpRequestContext &context) { return HandleHeartBeat(context); };
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

    auto linkBrokenMethod = [this](const AccTcpLinkComplexPtr &link) { return HandleLinkBroken(link); };
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
    dynLibPath.append("/../3rdparty/openssl/lib/");
    OpenSslApiWrapper::Load(dynLibPath);

    std::string certPath;
    GetCertPath(certPath);
    AccTlsOption tlsOption;
    tlsOption.enableTls = false;
    (void)mServer->Start(opts, tlsOption);
}

void TestAccTcpSslClient::TearDown()
{
    mServer->Stop();
    g_rankLinkMap.clear();
    GlobalMockObject::verify();
}


// *********************************TEST_F*************************
TEST_F(TestAccTcpSslClient, test_client_connect_send_should_return_ok)
{
    AccConnReq req{};
    req.rankId = 0;
    req.magic = 0;
    req.version = 1;
    AccTcpClientPtr mClient = AccTcpClient::Create("127.0.0.1", 8100);
    ASSERT_TRUE(mClient != nullptr);

    std::string certPath;
    GetCertPath(certPath);
    AccTlsOption tlsOption;
    tlsOption.enableTls = false;
    mClient->SetSslOption(tlsOption);

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

// **********************************************************

class TestOPENSSLAPIDL : public testing::Test {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();

public:
    void SetUp() override;
    void TearDown() override;
};

void TestOPENSSLAPIDL::SetUpTestSuite() {}

void TestOPENSSLAPIDL::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

void TestOPENSSLAPIDL::SetUp() {}

void TestOPENSSLAPIDL::TearDown()
{
    GlobalMockObject::verify();
}

class N {
public:
    static const int n2 = 2;
    static const int n3 = 3;
    static const int n4 = 4;
    static const int n5 = 5;
    static const int n6 = 6;
    static const int n7 = 7;
    static const int n8 = 8;
    static const int n9 = 9;
    static const int n10 = 10;
    static const int n11 = 11;
    static const int n12 = 12;
    static const int n13 = 13;
    static const int n14 = 14;
    static const int n15 = 15;
    static const int n16 = 16;
    static const int n17 = 17;
    static const int n18 = 18;
    static const int n19 = 19;
    static const int n20 = 20;
    static const int n21 = 21;
    static const int n22 = 22;
    static const int n23 = 23;
    static const int n24 = 24;
    static const int n25 = 25;
    static const int n26 = 26;
    static const int n27 = 27;
    static const int n28 = 28;
    static const int n29 = 29;
    static const int n30 = 30;
    static const int n31 = 31;
    static const int n32 = 32;
    static const int n33 = 33;
    static const int n34 = 34;
    static const int n35 = 35;
    static const int n36 = 36;
    static const int n37 = 37;
};

TEST_F(TestOPENSSLAPIDL, FakeSslLibPath)
{
    std::string fakePath = "./fakePath";
    auto ret = OPENSSLAPIDL::GetLibPath(fakePath, fakePath, fakePath);
    ASSERT_EQ(ret, -1);
}

TEST_F(TestOPENSSLAPIDL, LoadSSLSymbols_return_error)
{
    int branchTestRepeat = 37; // LoadSSLSymbols函数中语句行数
    char buff[1024];
    if (getcwd(buff, sizeof(buff)) != nullptr) {
        std::cout << "Current directory: " << buff << std::endl;
    } else {
        perror("getcwd() error");
    }
    std::string dynLibPath = buff;
    dynLibPath.append("/../3rdparty/openssl/lib/");

    std::string libDir = dynLibPath;
    std::string libSslPath;
    std::string libCryptoPath;
    auto ret = OPENSSLAPIDL::GetLibPath(libDir, libSslPath, libCryptoPath);
    ASSERT_EQ(ret, 0);
    auto sslHandle = dlopen(libSslPath.c_str(), RTLD_NOW | RTLD_GLOBAL);

    void *nuPtr = nullptr;
    auto mPtr = dlsym((sslHandle), ("OPENSSL_init_ssl"));
    ASSERT_FALSE(mPtr == nullptr);

    int i = 1;
    MOCKER_CPP(&dlsym, void *(*)(void *, const char *))
        .expects(atLeast(1))
        .will(returnValue(nuPtr))
        .then(returnValue(mPtr))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n2))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n3))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n4))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n5))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n6))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n7))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n8))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n9))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n10))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n11))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n12))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n13))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n14))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n15))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n16))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n17))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n18))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n19))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n20))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n21))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n22))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n23))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n24))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n25))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n26))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n27))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n28))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n29))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n30))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n31))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n32))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n33))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n34))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n35))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n36))
        .then(returnValue(nuPtr));
    for (int k = 0; k < branchTestRepeat; k++) {
        ret = OPENSSLAPIDL::LoadSSLSymbols(sslHandle);
        ASSERT_EQ(ret, -1);
    }
}


TEST_F(TestOPENSSLAPIDL, LoadCryptoSymbols_return_error)
{
    int branchTestRepeat = 38; // LoadCryptoSymbols函数中语句行数
    char buff[1024];
    auto buffret = getcwd(buff, sizeof(buff));
    std::string dynLibPath = buff;
    dynLibPath.append("/../3rdparty/openssl/lib/");

    std::string libDir = dynLibPath;
    std::string libSslPath;
    std::string libCryptoPath;
    auto ret = OPENSSLAPIDL::GetLibPath(libDir, libSslPath, libCryptoPath);
    // CheckPermission : fail branch coverage
    std::string errMsg;
    bool permRet = FileValidator::CheckPermission(libSslPath, 0b000000000, false, errMsg);
    ASSERT_EQ(permRet, false);
    permRet = FileValidator::CheckPermission(libCryptoPath, 0b000000000, true, errMsg);
    ASSERT_EQ(permRet, false);

    auto cryptoHandle = dlopen(libCryptoPath.c_str(), RTLD_NOW | RTLD_GLOBAL);

    void *nuPtr = nullptr;
    auto mPtr = dlsym((cryptoHandle), ("EVP_CIPHER_CTX_new"));

    int i = 1;
    MOCKER_CPP(&dlsym, void *(*)(void *, const char *))
        .expects(atLeast(1))
        .will(returnValue(nuPtr))
        .then(returnValue(mPtr))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n2))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n3))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n4))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n5))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n6))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n7))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n8))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n9))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n10))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n11))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n12))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n13))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n14))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n15))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n16))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n17))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n18))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n19))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n20))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n21))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n22))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n23))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n24))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n25))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n26))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n27))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n28))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n29))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n30))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n31))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n32))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n33))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n34))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n35))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n36))
        .then(returnValue(nuPtr))
        .then(repeat(mPtr, N::n37))
        .then(returnValue(nuPtr));
    for (int k = 0; k < branchTestRepeat; k++) {
        ret = OPENSSLAPIDL::LoadCryptoSymbols(cryptoHandle);
        ASSERT_EQ(ret, -1);
    }
}
}