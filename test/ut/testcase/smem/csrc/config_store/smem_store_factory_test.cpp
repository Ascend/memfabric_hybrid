/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>

#include "smem_config_store.h"
#include "smem_store_factory.h"


using namespace ock::smem;

namespace ock {
namespace smem {
    extern Result ParseTlsInfo(const std::string &tlsInput, AcclinkTlsOption &tlsOption);
}
}

namespace {
int smem_decrypt_handler_stub(const char *cipherText, size_t cipherTextLen, char *plainText, size_t &plainTextLen)
{
    return 0;
}
}

class SmemStoreFactoryTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(SmemStoreFactoryTest, ParseTlsInfo_test1)
{
    std::string info =
        "tlsCaPath: /etc/ssl/certs/;"
        "tlsCert:    ;"
        "tlsPk:;"
        "tlsPkPwd: /etc/ssl/private/key_pwd.txt;"
        "tlsCrlPath: /etc/ssl/crl/;"
        "tlsCrlFile: server_crl1.pem,server_crl2.pem;"
        "tlsCaFile: ca.pem1,ca.pem2;"
        "packagePath: a";
    AcclinkTlsOption option;
    auto ret = ParseTlsInfo(info, option);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(option.tlsCaPath.length(), 15);
    EXPECT_EQ(option.packagePath.length(), 1);
    EXPECT_TRUE(!option.tlsCaPath.empty());
    EXPECT_TRUE(option.tlsCert.empty());
    EXPECT_TRUE(option.tlsPk.empty());
    EXPECT_TRUE(!option.tlsCrlFile.empty());
}

TEST_F(SmemStoreFactoryTest, ParseTlsInfo_test2)
{
    std::string info =
        "tlsCaPath /etc/ssl/certs/;"
        "tlsCert: /etc/ssl/certs/server.crt;"
        "tlsPk: /etc/ssl/private/server.key;"
        "tlsPkPwd: /etc/ssl/private/key_pwd.txt;"
        "tlsCrlPath: /etc/ssl/crl/;"
        "tlsCrlFile: server_crl1.pem,server_crl2.pem;"
        "tlsCaFile: ca.pem1,ca.pem2;"
        "packagePath: /etc/lib";
    AcclinkTlsOption option;
    auto ret = ParseTlsInfo(info, option);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(option.tlsCaPath.empty());
}

TEST_F(SmemStoreFactoryTest, ParseTlsInfo_test3)
{
    std::string info =
        "tlsCaPath: /etc/ssl/certs/;"
        "tlsCert: /etc/ssl/certs/server.crt;"
        "tlsPk: /etc/ssl/private/server.key;"
        "tlsPkPwd: /etc/ssl/private/key_pwd.txt;"
        "tlsCrlPath: /etc/ssl/crl/;"
        "tlsCrlFile: server_crl1.pem,server_crl2.pem;"
        "tlsCaFile:,,;"
        "packagePath: /etc/lib";
    AcclinkTlsOption option;
    auto ret = ParseTlsInfo(info, option);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(option.tlsCaFile.empty());
}

TEST_F(SmemStoreFactoryTest, SetTlsInfo_test1)
{
    std::string info =
        "tlsCaPath: /etc/ssl/certs/;"
        "tlsCert: /etc/ssl/certs/server.crt;"
        "tlsPk: /etc/ssl/private/server.key;"
        "tlsPkPwd: /etc/ssl/private/key_pwd.txt;"
        "tlsCrlPath: /etc/ssl/crl/;"
        "tlsCrlFile: server_crl1.pem,server_crl2.pem;"
        "tlsCaFile: ca.pem1,ca.pem2;"
        "packagePath: /etc/lib";
    int32_t ret = ock::smem::StoreFactory::SetTlsInfo(true, info.c_str(), info.length());
    EXPECT_EQ(ret, 0);
}

TEST_F(SmemStoreFactoryTest, SetTlsInfo_test2)
{
    std::string info =
        "tlsCaPath: /etc/ssl/certs/;"
        "tlsCert: /etc/ssl/certs/server.crt;"
        "tlsPk: /etc/ssl/private/server.key;"
        "tlsPkPwd: /etc/ssl/private/key_pwd.txt;"
        "tlsCrlPath: /etc/ssl/crl/;"
        "tlsCrlFile: server_crl1.pem,server_crl2.pem;"
        "tlsCaFile: ca.pem1,ca.pem2;"
        "packagePath: /etc/lib";
    const size_t unexpect_size = 102400;
    int32_t ret = ock::smem::StoreFactory::SetTlsInfo(true, info.c_str(), unexpect_size);
    EXPECT_EQ(ret, StoreErrorCode::ERROR);
}

TEST_F(SmemStoreFactoryTest, InitTlsOption_test_OK)
{
    std::string info =
        "tlsCaPath: /etc/ssl/certs/;"
        "tlsCert: /etc/ssl/certs/server.crt;"
        "tlsPk: /etc/ssl/private/server.key;"
        "tlsPkPwd: /etc/ssl/private/key_pwd.txt;"
        "tlsCrlPath: /etc/ssl/crl/;"
        "tlsCrlFile: server_crl1.pem,server_crl2.pem;"
        "tlsCaFile: ca.pem1,ca.pem2;"
        "packagePath: /etc/lib";
    int32_t ret = ock::smem::StoreFactory::SetTlsInfo(true, info.c_str(), info.length());
    EXPECT_EQ(ret, 0);

    std::string pk_stub = "private key";
    std::string pk_pwd_stb = "private key password stub";
    ret = ock::smem::StoreFactory::SetTlsPkInfo(pk_stub.c_str(), pk_stub.size(),
        pk_pwd_stb.c_str(), pk_pwd_stb.size(), smem_decrypt_handler_stub);
    EXPECT_EQ(ret, 0);

    // set again
    ret = ock::smem::StoreFactory::SetTlsPkInfo(pk_stub.c_str(), pk_stub.size(),
        pk_pwd_stb.c_str(), pk_pwd_stb.size(), smem_decrypt_handler_stub);
    EXPECT_EQ(ret, 0);

    ock::smem::StoreFactory::ShutDownCleanupThread();
}

TEST_F(SmemStoreFactoryTest, InitTlsOption_test_exceptions)
{
    std::string pk_stub = "private key";
    std::string pk_pwd_stb = "private key password stub";

    // unexpect pk size
    const size_t unexpect_size = 102400;
    int32_t ret = ock::smem::StoreFactory::SetTlsPkInfo(pk_stub.c_str(), unexpect_size,
        pk_pwd_stb.c_str(), pk_pwd_stb.size(), smem_decrypt_handler_stub);
    EXPECT_EQ(ret, StoreErrorCode::ERROR);
    ock::smem::StoreFactory::ShutDownCleanupThread();

    // unexpect pk pwd size
    ret = ock::smem::StoreFactory::SetTlsPkInfo(pk_stub.c_str(), pk_stub.size(),
        pk_pwd_stb.c_str(), unexpect_size, smem_decrypt_handler_stub);
    EXPECT_EQ(ret, StoreErrorCode::ERROR);
    ock::smem::StoreFactory::ShutDownCleanupThread();

    // pk pwd is nullptr
    ret = ock::smem::StoreFactory::SetTlsPkInfo(pk_stub.c_str(), pk_stub.size(),
        nullptr, pk_pwd_stb.size(), smem_decrypt_handler_stub);
    EXPECT_EQ(ret, 0);

    ock::smem::StoreFactory::ShutDownCleanupThread();
}