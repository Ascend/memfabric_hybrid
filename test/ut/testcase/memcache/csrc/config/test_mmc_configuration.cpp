/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"

#define private public
#include "mmc_configuration.h"
#undef private

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestMmcConfiguration : public testing::Test {
public:
    TestMmcConfiguration();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestMmcConfiguration::TestMmcConfiguration() = default;

void TestMmcConfiguration::SetUp() {}

void TestMmcConfiguration::TearDown() {}

TEST_F(TestMmcConfiguration, ValidateTLSConfigTest)
{
    mmc_tls_config tlsConfig {};
    tlsConfig.tlsEnable = true;
    // 1. caPath is empty
    auto ret = Configuration::ValidateTLSConfig(tlsConfig);
    ASSERT_NE(ret, MMC_OK);

    // 2. certPath is empty
    SafeCopy("./", tlsConfig.caPath, TLS_PATH_SIZE);
    ret = Configuration::ValidateTLSConfig(tlsConfig);
    ASSERT_NE(ret, MMC_OK);

    // 3. keyPath is empty
    SafeCopy("./", tlsConfig.certPath, TLS_PATH_SIZE);
    ret = Configuration::ValidateTLSConfig(tlsConfig);
    ASSERT_NE(ret, MMC_OK);

    // 4. crlPath is error
    SafeCopy("./", tlsConfig.keyPath, TLS_PATH_SIZE);
    SafeCopy("nonPath", tlsConfig.crlPath, TLS_PATH_SIZE);
    ret = Configuration::ValidateTLSConfig(tlsConfig);
    ASSERT_NE(ret, MMC_OK);

    // 4. keyPassPath is error
    SafeCopy("./", tlsConfig.crlPath, TLS_PATH_SIZE);
    SafeCopy("nonPath", tlsConfig.keyPassPath, TLS_PATH_SIZE);
    ret = Configuration::ValidateTLSConfig(tlsConfig);
    ASSERT_NE(ret, MMC_OK);

    // 5. packagePath is error
    SafeCopy("./", tlsConfig.keyPassPath, TLS_PATH_SIZE);
    SafeCopy("nonPath", tlsConfig.packagePath, TLS_PATH_SIZE);
    ret = Configuration::ValidateTLSConfig(tlsConfig);
    ASSERT_NE(ret, MMC_OK);

    // 6. decrypterLibPath is error
    SafeCopy("./", tlsConfig.packagePath, TLS_PATH_SIZE);
    SafeCopy("nonPath", tlsConfig.decrypterLibPath, TLS_PATH_SIZE);
    ret = Configuration::ValidateTLSConfig(tlsConfig);
    ASSERT_NE(ret, MMC_OK);

    SafeCopy("./", tlsConfig.decrypterLibPath, TLS_PATH_SIZE);
    ret = Configuration::ValidateTLSConfig(tlsConfig);
    ASSERT_EQ(ret, MMC_OK);
}


TEST_F(TestMmcConfiguration, GetLogPathTest)
{
    Configuration configuration;

    auto logPath = configuration.GetLogPath("");
    ASSERT_NE(logPath, "");

    logPath = configuration.GetLogPath("/");
    ASSERT_EQ(logPath, "/");

    logPath = configuration.GetLogPath("./");
    ASSERT_NE(logPath, "/");
}

TEST_F(TestMmcConfiguration, SetTest)
{
    Configuration configuration;
    std::string testKey = "testKey";

    configuration.Set(testKey, 1);
    ASSERT_EQ(configuration.GetInt({testKey.c_str(), 0}), 0);
    configuration.AddIntConf({testKey, 0});
    configuration.Set(testKey, 1);
    ASSERT_EQ(configuration.GetInt({testKey.c_str(), 0}), 1);

    configuration.Set(testKey, 1.0f);
    ASSERT_EQ(configuration.GetFloat({testKey.c_str(), 0.0f}), 0.0f);
    configuration.mFloatItems[testKey] = 0.0;
    configuration.Set(testKey, 1.0f);
    ASSERT_EQ(configuration.GetFloat({testKey.c_str(), 0.0f}), 1.0f);

    configuration.Set(testKey, testKey);
    ASSERT_EQ(configuration.GetString({testKey.c_str(), ""}), "");
    configuration.AddStrConf({testKey, ""});
    configuration.Set(testKey, testKey);
    ASSERT_EQ(configuration.GetString({testKey.c_str(), testKey.c_str()}), testKey);

    configuration.Set(testKey, true);
    ASSERT_EQ(configuration.GetBool({testKey.c_str(), false}), false);
    configuration.AddBoolConf({testKey, false});
    configuration.Set(testKey, true);
    ASSERT_EQ(configuration.GetBool({testKey.c_str(), false}), true);

    configuration.Set(testKey, 1LU);
    ASSERT_EQ(configuration.GetUInt64({testKey.c_str(), 0LU}), 0LU);
    configuration.AddUInt64Conf({testKey, 0LU});
    configuration.Set(testKey, 1LU);
    ASSERT_EQ(configuration.GetUInt64({testKey.c_str(), 0LU}), 1LU);
}

TEST_F(TestMmcConfiguration, ParseMemSizeTest)
{
    Configuration configuration;
    auto ret = configuration.ParseMemSize("");
    ASSERT_EQ(ret, UINT64_MAX);

    ret = configuration.ParseMemSize("aB");
    ASSERT_EQ(ret, UINT64_MAX);

    ret = configuration.ParseMemSize("1B");
    ASSERT_EQ(ret, 1);

    ret = configuration.ParseMemSize("1KB");
    ASSERT_EQ(ret, KB_MEM_BYTES);

    ret = configuration.ParseMemSize("1MB");
    ASSERT_EQ(ret, MB_MEM_BYTES);

    ret = configuration.ParseMemSize("1GB");
    ASSERT_EQ(ret, GB_MEM_BYTES);

    ret = configuration.ParseMemSize("1TB");
    ASSERT_EQ(ret, TB_MEM_BYTES);
}