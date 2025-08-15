/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>

#include "smem_config_store.h"


using namespace ock::smem;

namespace ock{
    namespace smem{
        extern Result ParseTlsInfo(const char* tlsInput, AcclinkTlsOption &tlsOption);
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
    auto ret = ParseTlsInfo(info.c_str(), option);
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
    auto ret = ParseTlsInfo(info.c_str(), option);
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
    auto ret = ParseTlsInfo(info.c_str(), option);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(option.tlsCaFile.empty());
}