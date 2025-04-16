/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <thread>
#include <mutex>
#include <condition_variable>
#include "gtest/gtest.h"
#include "smem_message_packer.h"
#include "smem_tcp_config_store.h"
#include "smem_store_factory.h"

class AccConfigStoreTest : public testing::Test {
public:
    void SetUp() override
    {
        uint16_t port = 10000U + getpid() % 1000U;
        std::cout << "port is : " << port << std::endl;
        server = ock::smem::StoreFactory::CreateStore("127.0.0.1", port, true, 0);
        client = ock::smem::StoreFactory::CreateStore("127.0.0.1", port, false, 1);
    }

    void TearDown() override
    {
        // client.reset();
        // server.reset();
    }

protected:
    ock::smem::StorePtr client;
    ock::smem::StorePtr server;
};

TEST_F(AccConfigStoreTest, set_get_check)
{
    std::string key = "set_get_check_key1";
    std::string value = "set_get_check_value1";
    auto ret = client->Set(key, std::vector<uint8_t>(value.begin(), value.end()));
    ASSERT_EQ(0, ret) << "client set key failed.";

    std::vector<uint8_t> valueOut;
    ret = server->Get(key, valueOut);
    ASSERT_EQ(0, ret) << "server get key failed.";
    ASSERT_EQ(value, std::string(valueOut.begin(), valueOut.end()));
}

TEST_F(AccConfigStoreTest, get_block_check)
{
    std::string key = "get_block_check_key1";
    std::string value = "get_block_check_value1";
    int getRet = -1;
    std::vector<uint8_t> valueOut;
    auto task = [&]() {
        getRet = client->Get(key, valueOut);
    };

    std::thread getThread{task};

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto ret = client->Set(key, std::vector<uint8_t>(value.begin(), value.end()));
    ASSERT_EQ(0, ret) << "client set key failed.";

    getThread.join();
}

TEST_F(AccConfigStoreTest, add_check)
{
    std::string key = "add_check_key1";
    int64_t value;
    auto ret = client->Add(key, 1, value);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1L, value);

    ret = server->Add(key, 1, value);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(2L, value);
}

TEST_F(AccConfigStoreTest, get_timeout)
{
    std::string key = "get_timeout_key1";
    std::vector<uint8_t> value;

    auto ret = client->Get(key, value, 100);
    ASSERT_EQ(ock::smem::StoreErrorCode::TIMEOUT, ret);
}

TEST_F(AccConfigStoreTest, get_not_exist_imm)
{
    std::string key = "get_not_exist_imm_key1";
    std::vector<uint8_t> value;

    auto ret = client->Get(key, value, 0);
    ASSERT_EQ(ock::smem::StoreErrorCode::NOT_EXIST, ret);
}

TEST_F(AccConfigStoreTest, append_key_check)
{
    std::string key = "append_key_check_key";
    std::string value1 = "hello";
    std::string value2 = " world";
    std::string value3 = " 3ks!";

    uint64_t size = 0;
    uint64_t expectSize = value1.length();
    auto ret = client->Append(key, value1, size);
    ASSERT_EQ(ock::smem::StoreErrorCode::SUCCESS, ret);
    ASSERT_EQ(expectSize, size);

    expectSize += value2.size();
    ret = server->Append(key, value2, size);
    ASSERT_EQ(ock::smem::StoreErrorCode::SUCCESS, ret);
    ASSERT_EQ(expectSize, size);

    expectSize += value3.size();
    ret = client->Append(key, value3, size);
    ASSERT_EQ(ock::smem::StoreErrorCode::SUCCESS, ret);
    ASSERT_EQ(expectSize, size);

    std::string lastValue;
    ret = client->Get(key, lastValue, 0);
    ASSERT_EQ(ock::smem::StoreErrorCode::SUCCESS, ret);
    ASSERT_EQ(std::string(value1).append(value2).append(value3), lastValue);
}

TEST_F(AccConfigStoreTest, prefix_store_check)
{
    std::string prefix = "/init/";
    auto store = ock::smem::StoreFactory::PrefixStore(client, prefix);

    std::string key = "prefix_store_check_key";
    std::string value = "this is value";
    auto ret = server->Set(prefix + key, value);
    ASSERT_EQ(0, ret);

    std::string getValue;
    ret = store->Get(key, getValue, 0);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(value, getValue);
}