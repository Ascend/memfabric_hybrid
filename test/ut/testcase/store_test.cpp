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
        ock::smem::StoreFactory::SetTlsInfo(false, nullptr, 0);
        std::cout << "port is : " << port << std::endl;
        server = ock::smem::StoreFactory::CreateStore("0.0.0.0", port, true, 0);
        client = ock::smem::StoreFactory::CreateStore("127.0.0.1", port, false, 1);
    }

    void TearDown() override
    {
        client = nullptr;
        ock::smem::StoreFactory::DestroyStore("127.0.0.1", port);

        server = nullptr;
        ock::smem::StoreFactory::DestroyStore("0.0.0.0", port);
    }

protected:
    const uint16_t port = 10000U + getpid() % 1000U;
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


TEST_F(AccConfigStoreTest, get_when_server_exit)
{
    std::string prefix = "/server-exit/";
    auto store = ock::smem::StoreFactory::PrefixStore(client, prefix);

    std::atomic<bool> finished{false};
    std::atomic<int> ret{-10000};
    std::string key = "store_check_key_for_server_exit";
    std::string value;

    std::thread child{[&finished, &ret, &store, &key, &value](){
        ret = store->Get(key, value);
        finished = true;
    }};
    child.detach();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    server = nullptr;
    ock::smem::StoreFactory::DestroyStore("0.0.0.0", port);
    std::this_thread::sleep_for(std::chrono::seconds (1));

    ASSERT_TRUE(finished.load());
    ASSERT_NE(0, ret.load());
}

TEST_F(AccConfigStoreTest, watch_one_key_notify_success)
{
    std::string prefix = "/watch/";
    auto store = ock::smem::StoreFactory::PrefixStore(client, prefix);

    uint32_t wid;
    std::string key = "watch_store_check_key";
    std::string value = "this is value";

    struct NotifyContext {
        std::mutex mutex;
        std::condition_variable cond;
        bool finished = false;
        int result = -1;
        std::string key;
        std::string value;
    } notifyContext;

    auto ret = store->Watch(
        key,
        [&notifyContext](int res, const std::string &nKey, const std::string &nValue) {
            std::unique_lock<std::mutex> lockGuard{notifyContext.mutex};
            notifyContext.finished = true;
            notifyContext.result = res;
            notifyContext.key = nKey;
            notifyContext.value = nValue;
            lockGuard.unlock();

            notifyContext.cond.notify_one();
        },
        wid);
    ASSERT_EQ(0, ret) << "client watch key: (" << key << ") failed.";

    ret = server->Set(std::string(prefix).append(key), value);
    ASSERT_EQ(0, ret) << "server set key: (" << key << ") failed.";

    std::unique_lock<std::mutex> lockGuard{notifyContext.mutex};
    notifyContext.cond.wait_for(lockGuard, std::chrono::seconds{1});
    ASSERT_TRUE(notifyContext.finished);
    ASSERT_EQ(key, notifyContext.key);
    ASSERT_EQ(0, notifyContext.result);
    ASSERT_EQ(value, notifyContext.value);
}

TEST_F(AccConfigStoreTest, watch_one_key_unwatch)
{
    std::string prefix = "/watch-unwatch/";
    auto store = ock::smem::StoreFactory::PrefixStore(client, prefix);

    uint32_t wid;
    std::string key = "watch_store_check_key_for_unwatch";
    std::string value = "this is value";

    std::atomic<int64_t> notifyTimes{0L};
    auto ret = store->Watch(
        key, [&notifyTimes](int, const std::string &, const std::string &) { notifyTimes.fetch_add(1L); }, wid);
    ASSERT_EQ(0, ret) << "client watch key: (" << key << ") failed.";

    ret = store->Unwatch(wid);
    ASSERT_EQ(0, ret) << "client unwatch for wid: (" << wid << ") failed.";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(0L, notifyTimes.load());
}