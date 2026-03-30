/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <unistd.h>

#define private   public
#define protected public
#include "smem_trans/smem_trans_store_helper.h"
#undef protected
#undef private
#include "smem_store_factory.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace ock::smem;

namespace {

using SmemResult = ock::smem::Result;

constexpr uint16_t K_ENTITY_ID = 7;
constexpr int32_t K_MAX_RETRY = 5;
constexpr int32_t K_RECONNECT_RETRY_TIMES = 3;
constexpr uint16_t K_HIGH_BYTE_SHIFT = 8;
constexpr uint32_t K_LOW_BYTE_MASK = 0xffU;
constexpr size_t K_ID_BYTE_COUNT = 2;
constexpr size_t K_STATUS_FIELD_SIZE = 1;
constexpr uint16_t K_RECOVERED_RANK_ID = 4;
constexpr uint16_t K_RECOVERED_DEVICE_ID = 2;
constexpr uint16_t K_RECOVERED_SLICE_ID = 3;
constexpr uint32_t K_DEVICE_RECORD_COUNT = 3;
constexpr uint32_t K_SLICE_RECORD_COUNT = 4;
constexpr uint32_t K_RANK_RECORD_COUNT = 3;
constexpr uint64_t K_SLICE_SIZE = 64;
constexpr uintptr_t K_SLICE_ADDRESS_SEED = 0x1000UL;
constexpr uint16_t K_WORKER_PORT = 10086;
constexpr uint32_t K_RANK_ID_FOR_SLICE = 6;
constexpr size_t K_INVALID_VALUE_SIZE = 1;
constexpr uint32_t K_INITIAL_CALL_INDEX = 0;
constexpr uint32_t K_SECOND_CALL_INDEX = 1;
constexpr uint16_t K_STORE_PORT_BASE = 19000;
constexpr uint16_t K_STORE_PORT_SPAN = 1000;
constexpr uint16_t K_FAILED_STORE_PORT_BASE = 21000;
constexpr char K_STORE_URL[] = "tcp://127.0.0.1:19090";
constexpr char K_REG_STORE_URL[] = "reg://127.0.0.1:2379#clusterA";
constexpr char K_INVALID_STORE_URL[] = "tcp://invalid";
constexpr char K_ENGINE_NAME[] = "trans-engine";
const std::string K_DEVICE_DESC_A = "devA";
const std::string K_DEVICE_DESC_B = "devB";
const std::string K_DEVICE_DESC_C = "devC";
const std::string K_DEVICE_DESC_D = "devD";
const std::string K_SLICE_DESC_A = "slc001";
const std::string K_SLICE_DESC_B = "slc002";
const std::string K_SLICE_DESC_C = "slc003";
const std::string K_SLICE_DESC_D = "slc004";

class FakeConfigStoreManager final : public ConfigStoreManager {
public:
    std::function<SmemResult(const std::string &, const std::vector<uint8_t> &)> setHook;
    std::function<SmemResult(const std::string &, int64_t, int64_t &)> addHook;
    std::function<SmemResult(const std::string &, bool)> removeHook;
    std::function<SmemResult(const std::string &, const std::vector<uint8_t> &, uint64_t &)> appendHook;
    std::function<SmemResult(const std::string &, const std::vector<uint8_t> &, const std::vector<uint8_t> &,
                             std::vector<uint8_t> &)>
        casHook;
    std::function<SmemResult(const std::string &, std::vector<uint8_t> &, int64_t)> getHook;
    std::function<SmemResult(const std::string &, const std::vector<uint8_t> &, uint32_t)> writeHook;
    std::function<SmemResult(const std::string &,
                             const std::function<void(int, const std::string &, const std::vector<uint8_t> &)> &,
                             uint32_t &)>
        watchHook;
    std::function<SmemResult(WatchRankType, const std::function<void(WatchRankType, uint32_t)> &, uint32_t &)>
        watchRankHook;
    std::function<SmemResult(uint32_t)> unwatchHook;
    std::function<void(ConfigStoreReconnectHandler)> reconnectHandlerHook;
    std::function<SmemResult(int)> reconnectHook;

    std::unordered_map<std::string, std::vector<uint8_t>> values;
    std::unordered_map<std::string, int64_t> integerValues;
    std::vector<ConfigStoreClientBrokenHandler> brokenHandlers;
    std::vector<ConfigStoreServerBrokenHandler> serverBrokenHandlers;
    std::unordered_map<int16_t, ConfigStoreServerOpHandler> serverOpHandlers;
    ConfigStoreReconnectHandler reconnectHandler;

    std::string lastSetKey;
    std::vector<uint8_t> lastSetValue;
    std::string lastGetKey;
    int64_t lastGetTimeoutMs = -1;
    std::string lastAppendKey;
    std::vector<uint8_t> lastAppendValue;
    std::string lastWriteKey;
    std::vector<uint8_t> lastWriteValue;
    uint32_t lastWriteOffset = 0;
    std::string lastAddKey;
    int64_t lastAddIncrement = 0;
    int reconnectCallCount = 0;
    bool connectStatus = false;
    int32_t rankId = -1;

    SmemResult Set(const std::string &key, const std::vector<uint8_t> &value) noexcept override
    {
        lastSetKey = key;
        lastSetValue = value;
        if (setHook != nullptr) {
            return setHook(key, value);
        }
        values[key] = value;
        return SUCCESS;
    }

    SmemResult Add(const std::string &key, int64_t increment, int64_t &value) noexcept override
    {
        lastAddKey = key;
        lastAddIncrement = increment;
        if (addHook != nullptr) {
            return addHook(key, increment, value);
        }
        integerValues[key] += increment;
        value = integerValues[key];
        return SUCCESS;
    }
    ock::smem::Result QueryAlive(uint32_t rank, uint32_t &alive) noexcept override
    {
        alive = true;
        return SM_OK;
    }
    ock::smem::Result PrefixGet(const std::string &key,
                                std::unordered_map<std::string, std::string> &value) noexcept override
    {
        return SM_OK;
    }
    SmemResult Remove(const std::string &key, bool) noexcept override
    {
        if (removeHook != nullptr) {
            return removeHook(key, true);
        }
        values.erase(key);
        integerValues.erase(key);
        return SUCCESS;
    }

    SmemResult Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept override
    {
        lastAppendKey = key;
        lastAppendValue = value;
        if (appendHook != nullptr) {
            return appendHook(key, value, newSize);
        }
        auto &storedValue = values[key];
        storedValue.insert(storedValue.end(), value.begin(), value.end());
        newSize = storedValue.size();
        return SUCCESS;
    }

    SmemResult Cas(const std::string &key, const std::vector<uint8_t> &expect, const std::vector<uint8_t> &value,
                   std::vector<uint8_t> &exists) noexcept override
    {
        if (casHook != nullptr) {
            return casHook(key, expect, value, exists);
        }
        exists = values[key];
        if (exists == expect) {
            values[key] = value;
        }
        return SUCCESS;
    }

    SmemResult Watch(const std::string &key,
                     const std::function<void(int, const std::string &, const std::vector<uint8_t> &)> &notify,
                     uint32_t &wid) noexcept override
    {
        if (watchHook != nullptr) {
            return watchHook(key, notify, wid);
        }
        wid = K_STATUS_FIELD_SIZE;
        return SUCCESS;
    }

    SmemResult Watch(WatchRankType type, const std::function<void(WatchRankType, uint32_t)> &notify,
                     uint32_t &wid) noexcept override
    {
        if (watchRankHook != nullptr) {
            return watchRankHook(type, notify, wid);
        }
        wid = K_STATUS_FIELD_SIZE;
        return SUCCESS;
    }

    SmemResult Unwatch(uint32_t wid) noexcept override
    {
        if (unwatchHook != nullptr) {
            return unwatchHook(wid);
        }
        return SUCCESS;
    }

    SmemResult Write(const std::string &key, const std::vector<uint8_t> &value, const uint32_t offset) noexcept override
    {
        lastWriteKey = key;
        lastWriteValue = value;
        lastWriteOffset = offset;
        if (writeHook != nullptr) {
            return writeHook(key, value, offset);
        }

        auto &storedValue = values[key];
        const size_t requiredSize = static_cast<size_t>(offset) + value.size();
        if (storedValue.size() < requiredSize) {
            storedValue.resize(requiredSize, 0);
        }
        std::copy(value.begin(), value.end(), storedValue.begin() + offset);
        return SUCCESS;
    }

    std::string GetCompleteKey(const std::string &key) noexcept override
    {
        return key;
    }

    std::string GetCommonPrefix() noexcept override
    {
        return "";
    }

    SmRef<ConfigStore> GetCoreStore() noexcept override
    {
        return nullptr;
    }

    void RegisterReconnectHandler(ConfigStoreReconnectHandler callback) noexcept override
    {
        reconnectHandler = std::move(callback);
        if (reconnectHandlerHook != nullptr) {
            reconnectHandlerHook(reconnectHandler);
        }
    }

    SmemResult ReConnectAfterBroken(int reconnectRetryTimes) noexcept override
    {
        reconnectCallCount++;
        if (reconnectHook != nullptr) {
            return reconnectHook(reconnectRetryTimes);
        }
        return SUCCESS;
    }

    bool GetConnectStatus() noexcept override
    {
        return connectStatus;
    }

    void SetConnectStatus(bool status) noexcept override
    {
        connectStatus = status;
    }

    void RegisterClientBrokenHandler(const ConfigStoreClientBrokenHandler &handler) noexcept override
    {
        brokenHandlers.emplace_back(handler);
    }

    void RegisterServerBrokenHandler(const ConfigStoreServerBrokenHandler &handler) noexcept override
    {
        serverBrokenHandlers.emplace_back(handler);
    }

    void RegisterServerOpHandler(int16_t opCode, const ConfigStoreServerOpHandler &handler) noexcept override
    {
        serverOpHandlers.emplace(opCode, handler);
    }

    void SetRankId(const int32_t &newRankId) noexcept override
    {
        rankId = newRankId;
    }

protected:
    SmemResult GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept override
    {
        lastGetKey = key;
        lastGetTimeoutMs = timeoutMs;
        if (getHook != nullptr) {
            return getHook(key, value, timeoutMs);
        }

        const auto iterator = values.find(key);
        if (iterator == values.end()) {
            return NOT_EXIST;
        }
        value = iterator->second;
        return SUCCESS;
    }
};

SmRef<FakeConfigStoreManager> MakeFakeStore()
{
    return SmMakeRef<FakeConfigStoreManager>();
}

StorePtr ToStorePtr(const SmRef<FakeConfigStoreManager> &store)
{
    return Convert<FakeConfigStoreManager, ConfigStore>(store);
}

StoreManagerPtr ToStoreManagerPtr(const SmRef<FakeConfigStoreManager> &store)
{
    return Convert<FakeConfigStoreManager, ConfigStoreManager>(store);
}

smem_trans_config_t MakeConfig(smem_trans_role_t role = SMEM_TRANS_SENDER)
{
    smem_trans_config_t config{};
    config.role = role;
    config.initTimeout = K_MAX_RETRY;
    config.deviceId = K_STATUS_FIELD_SIZE;
    config.flags = 0;
    config.dataOpType = SMEMB_DATA_OP_HOST_TCP;
    config.startConfigServer = false;
    return config;
}

hybm_exchange_info MakeExchangeInfo(const std::string &desc)
{
    hybm_exchange_info info{};
    info.descLen = static_cast<uint32_t>(desc.size());
    std::copy(desc.begin(), desc.end(), info.desc);
    return info;
}

StoredSliceInfo MakeStoredSliceInfo(uint32_t rankId, uint64_t size, uintptr_t addressSeed)
{
    WorkerUniqueId uniqueId{};
    uniqueId.port = K_WORKER_PORT;
    return StoredSliceInfo(uniqueId, reinterpret_cast<const void *>(addressSeed), size, rankId);
}

std::vector<uint8_t> EncodeId(uint32_t id)
{
    std::vector<uint8_t> value(K_ID_BYTE_COUNT);
    value[K_INITIAL_CALL_INDEX] = static_cast<uint8_t>(id & K_LOW_BYTE_MASK);
    value[K_SECOND_CALL_INDEX] = static_cast<uint8_t>(id >> K_HIGH_BYTE_SHIFT);
    return value;
}

std::vector<uint8_t> BuildDeviceRecord(uint8_t status, const std::string &desc)
{
    const hybm_exchange_info info = MakeExchangeInfo(desc);
    std::vector<uint8_t> value(K_STATUS_FIELD_SIZE + info.descLen);
    value[K_INITIAL_CALL_INDEX] = status;
    std::copy_n(info.desc, info.descLen, value.data() + K_STATUS_FIELD_SIZE);
    return value;
}

std::vector<uint8_t> BuildSliceRecord(uint8_t status, const StoredSliceInfo &sliceInfo, const std::string &desc)
{
    const hybm_exchange_info info = MakeExchangeInfo(desc);
    std::vector<uint8_t> value(K_STATUS_FIELD_SIZE + sizeof(StoredSliceInfo) + info.descLen);
    value[K_INITIAL_CALL_INDEX] = status;
    std::memcpy(value.data() + K_STATUS_FIELD_SIZE, &sliceInfo, sizeof(StoredSliceInfo));
    std::copy_n(info.desc, info.descLen, value.data() + K_STATUS_FIELD_SIZE + sizeof(StoredSliceInfo));
    return value;
}

std::vector<uint8_t> JoinRecords(std::initializer_list<std::vector<uint8_t>> records)
{
    size_t totalSize = 0;
    for (const auto &record : records) {
        totalSize += record.size();
    }

    std::vector<uint8_t> result;
    result.reserve(totalSize);
    for (const auto &record : records) {
        result.insert(result.end(), record.begin(), record.end());
    }
    return result;
}

class SmemTransStoreHelperTest : public testing::Test {
public:
    void SetUp() override
    {
        GlobalMockObject::reset();
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
    }
};

TEST_F(SmemTransStoreHelperTest, InitializeReturnsInvalidParamForInvalidRole)
{
    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_BOTH);

    EXPECT_EQ(SM_INVALID_PARAM, helper.Initialize(K_ENTITY_ID, K_MAX_RETRY));
}

TEST_F(SmemTransStoreHelperTest, InitializeReturnsInvalidParamForInvalidUrl)
{
    SmemStoreHelper helper(K_ENGINE_NAME, K_INVALID_STORE_URL, SMEM_TRANS_SENDER);

    EXPECT_EQ(SM_INVALID_PARAM, helper.Initialize(K_ENTITY_ID, K_MAX_RETRY));
}

TEST_F(SmemTransStoreHelperTest, InitializeSucceedsWithRealLocalStore)
{
    const uint16_t port = K_STORE_PORT_BASE + static_cast<uint16_t>(getpid() % K_STORE_PORT_SPAN);
    const std::string storeUrl = std::string("tcp://127.0.0.1:") + std::to_string(port);
    SmemStoreHelper helper(K_ENGINE_NAME, storeUrl, SMEM_TRANS_SENDER);

    ASSERT_EQ(SM_OK, helper.Initialize(K_ENTITY_ID, K_MAX_RETRY, true));
    EXPECT_EQ(SENDER_COUNT_KEY, helper.localKeys_.deviceCount);
    EXPECT_EQ(RECEIVER_COUNT_KEY, helper.remoteKeys_.deviceCount);
    EXPECT_NE(nullptr, helper.store_.Get());
    helper.Destroy();
}

TEST_F(SmemTransStoreHelperTest, InitializeReturnsNewObjectFailedWhenClientStoreCreationFails)
{
    const uint16_t port = K_FAILED_STORE_PORT_BASE + static_cast<uint16_t>(getpid() % K_STORE_PORT_SPAN);
    const std::string storeUrl = std::string("tcp://127.0.0.1:") + std::to_string(port);
    SmemStoreHelper helper(K_ENGINE_NAME, storeUrl, SMEM_TRANS_RECEIVER);

    EXPECT_EQ(SM_NEW_OBJECT_FAILED, helper.Initialize(K_ENTITY_ID, 0));
}

TEST_F(SmemTransStoreHelperTest, InitializeSucceedsWithRegUrlWhenSharedFactoryAcceptsIt)
{
    auto fakeStore = MakeFakeStore();
    auto storePtr = ToStorePtr(fakeStore);
    ASSERT_NE(nullptr, storePtr.Get());
    MOCKER_CPP(&ock::smem::StoreFactory::CreateStoreByUrl,
               ock::smem::StorePtr(*)(const std::string &, bool, uint32_t, int32_t, int32_t))
        .stubs()
        .will(returnValue(storePtr));

    SmemStoreHelper helper(K_ENGINE_NAME, K_REG_STORE_URL, SMEM_TRANS_SENDER);
    EXPECT_EQ(SM_OK, helper.Initialize(K_ENTITY_ID, K_MAX_RETRY));
    EXPECT_NE(nullptr, helper.store_.Get());
    helper.Destroy();
}

TEST_F(SmemTransStoreHelperTest, DestroyResetsStoreAndDestroysConfiguredUrl)
{
    auto fakeStore = MakeFakeStore();
    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);

    helper.Destroy();

    EXPECT_EQ(nullptr, helper.store_.Get());
}

TEST_F(SmemTransStoreHelperTest, ServerControlApisForwardToStore)
{
    auto fakeStore = MakeFakeStore();
    fakeStore->connectStatus = false;
    fakeStore->reconnectHook = [](int retryTimes) { return retryTimes == K_RECONNECT_RETRY_TIMES ? SM_OK : SM_ERROR; };

    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);

    helper.AlterServerStatus(true);
    EXPECT_EQ(1, helper.CheckServerStatus());
    EXPECT_EQ(SM_RECONNECT, helper.ReConnect());

    helper.RegisterBrokenHandler([]() { return 0; });
    EXPECT_EQ(K_STATUS_FIELD_SIZE, fakeStore->brokenHandlers.size());
}

TEST_F(SmemTransStoreHelperTest, GenerateRankIdAppendsConfigAndStoresAutoRankKey)
{
    auto fakeStore = MakeFakeStore();
    fakeStore->appendHook = [](const std::string &, const std::vector<uint8_t> &, uint64_t &newSize) {
        newSize = sizeof(smem_trans_config_t) * K_RANK_RECORD_COUNT;
        return SUCCESS;
    };

    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);
    const smem_trans_config_t config = MakeConfig();
    uint32_t rankId = 0;

    ASSERT_EQ(SM_OK, helper.GenerateRankId(config, rankId));
    EXPECT_EQ(K_RANK_RECORD_COUNT - K_STATUS_FIELD_SIZE, rankId);
    EXPECT_EQ(CLUSTER_RANKS_INFO_KEY, fakeStore->lastAppendKey);
    EXPECT_EQ(AUTO_RANK_KEY_PREFIX + std::string(K_ENGINE_NAME), fakeStore->lastSetKey);
    EXPECT_EQ(EncodeId(rankId), fakeStore->lastSetValue);
    EXPECT_EQ(rankId, helper.storeRankIdInfo_.first);
    EXPECT_EQ(sizeof(config), helper.storeRankIdInfo_.second.size());
}

TEST_F(SmemTransStoreHelperTest, GenerateRankIdReturnsErrorWhenAppendFails)
{
    auto fakeStore = MakeFakeStore();
    fakeStore->appendHook = [](const std::string &, const std::vector<uint8_t> &, uint64_t &) { return ERROR; };

    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);
    const smem_trans_config_t config = MakeConfig();
    uint32_t rankId = 0;

    EXPECT_EQ(SM_ERROR, helper.GenerateRankId(config, rankId));
}

TEST_F(SmemTransStoreHelperTest, GenerateRankIdRestoresRankInformationWhenStoreReturnsRestore)
{
    auto fakeStore = MakeFakeStore();
    fakeStore->getHook = [](const std::string &, std::vector<uint8_t> &value, int64_t) {
        value = EncodeId(K_RECOVERED_RANK_ID);
        return RESTORE;
    };

    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);
    const smem_trans_config_t config = MakeConfig();
    uint32_t rankId = 0;

    ASSERT_EQ(SM_OK, helper.GenerateRankId(config, rankId));
    EXPECT_EQ(K_RECOVERED_RANK_ID, rankId);
    EXPECT_EQ(CLUSTER_RANKS_INFO_KEY, fakeStore->lastWriteKey);
    EXPECT_EQ(sizeof(config) * K_RECOVERED_RANK_ID, fakeStore->lastWriteOffset);
    EXPECT_EQ(AUTO_RANK_KEY_PREFIX + std::string(K_ENGINE_NAME), fakeStore->lastSetKey);
    EXPECT_EQ(EncodeId(K_RECOVERED_RANK_ID), fakeStore->lastSetValue);
}

TEST_F(SmemTransStoreHelperTest, GenerateRankIdReturnsErrorForInvalidExistingValueSize)
{
    auto fakeStore = MakeFakeStore();
    fakeStore->getHook = [](const std::string &, std::vector<uint8_t> &value, int64_t) {
        value.assign(K_INVALID_VALUE_SIZE, 0);
        return SUCCESS;
    };

    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);
    const smem_trans_config_t config = MakeConfig();
    uint32_t rankId = 0;

    EXPECT_EQ(SM_ERROR, helper.GenerateRankId(config, rankId));
}

TEST_F(SmemTransStoreHelperTest, StoreDeviceInfoSupportsAppendAndRestorePaths)
{
    auto fakeStore = MakeFakeStore();
    bool firstCall = true;
    fakeStore->getHook = [&firstCall](const std::string &, std::vector<uint8_t> &value, int64_t) {
        if (firstCall) {
            firstCall = false;
            return NOT_EXIST;
        }
        value = EncodeId(K_RECOVERED_DEVICE_ID);
        return RESTORE;
    };
    fakeStore->appendHook = [](const std::string &, const std::vector<uint8_t> &value, uint64_t &newSize) {
        newSize = value.size() * K_DEVICE_RECORD_COUNT;
        return SUCCESS;
    };
    fakeStore->addHook = [](const std::string &, int64_t increment, int64_t &value) {
        value = increment;
        return SUCCESS;
    };

    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);
    helper.localKeys_ = StoreKeys(SENDER_COUNT_KEY, SENDER_TOTAL_SLICE_COUNT_KEY, SENDER_DEVICE_INFO_KEY,
                                  SENDER_SLICES_INFO_KEY, SENDER_GET_DEVICE_ID_KEY, SENDER_GET_SLICES_ID_KEY);
    const hybm_exchange_info info = MakeExchangeInfo(K_DEVICE_DESC_A);

    ASSERT_EQ(SM_OK, helper.StoreDeviceInfo(info));
    EXPECT_EQ(K_DEVICE_RECORD_COUNT - K_STATUS_FIELD_SIZE, helper.storeDeviceInfo_.first);
    EXPECT_EQ(SENDER_DEVICE_INFO_KEY, fakeStore->lastAppendKey);

    ASSERT_EQ(SM_OK, helper.StoreDeviceInfo(info));
    EXPECT_EQ(K_RECOVERED_DEVICE_ID, helper.storeDeviceInfo_.first);
    EXPECT_EQ(SENDER_DEVICE_INFO_KEY, fakeStore->lastWriteKey);
    EXPECT_EQ((K_STATUS_FIELD_SIZE + info.descLen) * K_RECOVERED_DEVICE_ID, fakeStore->lastWriteOffset);
}

TEST_F(SmemTransStoreHelperTest, ReStoreDeviceInfoWritesStoredValueBack)
{
    auto fakeStore = MakeFakeStore();
    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);
    helper.localKeys_ = StoreKeys(SENDER_COUNT_KEY, SENDER_TOTAL_SLICE_COUNT_KEY, SENDER_DEVICE_INFO_KEY,
                                  SENDER_SLICES_INFO_KEY, SENDER_GET_DEVICE_ID_KEY, SENDER_GET_SLICES_ID_KEY);
    helper.storeDeviceInfo_ = {K_RECOVERED_DEVICE_ID, BuildDeviceRecord(DataStatusType::NORMAL, K_DEVICE_DESC_B)};

    ASSERT_EQ(SM_OK, helper.ReStoreDeviceInfo());
    EXPECT_EQ(SENDER_DEVICE_INFO_KEY, fakeStore->lastWriteKey);
    EXPECT_EQ(helper.storeDeviceInfo_.second.size() * K_RECOVERED_DEVICE_ID, fakeStore->lastWriteOffset);
}

TEST_F(SmemTransStoreHelperTest, StoreSliceInfoSupportsAppendAndRestorePaths)
{
    auto fakeStore = MakeFakeStore();
    bool firstCall = true;
    fakeStore->getHook = [&firstCall](const std::string &, std::vector<uint8_t> &value, int64_t) {
        if (firstCall) {
            firstCall = false;
            return NOT_EXIST;
        }
        value = EncodeId(K_RECOVERED_SLICE_ID);
        return RESTORE;
    };
    fakeStore->appendHook = [](const std::string &, const std::vector<uint8_t> &value, uint64_t &newSize) {
        newSize = value.size() * K_SLICE_RECORD_COUNT;
        return SUCCESS;
    };
    fakeStore->addHook = [](const std::string &, int64_t increment, int64_t &value) {
        value = increment;
        return SUCCESS;
    };

    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);
    helper.localKeys_ = StoreKeys(SENDER_COUNT_KEY, SENDER_TOTAL_SLICE_COUNT_KEY, SENDER_DEVICE_INFO_KEY,
                                  SENDER_SLICES_INFO_KEY, SENDER_GET_DEVICE_ID_KEY, SENDER_GET_SLICES_ID_KEY);
    const hybm_exchange_info info = MakeExchangeInfo(K_SLICE_DESC_A);
    const StoredSliceInfo sliceInfo = MakeStoredSliceInfo(K_RANK_ID_FOR_SLICE, K_SLICE_SIZE, K_SLICE_ADDRESS_SEED);

    ASSERT_EQ(SM_OK, helper.StoreSliceInfo(info, sliceInfo));
    EXPECT_EQ(K_SLICE_RECORD_COUNT - K_STATUS_FIELD_SIZE, helper.storeSliceInfo_[K_INITIAL_CALL_INDEX].first);
    EXPECT_EQ(SENDER_SLICES_INFO_KEY, fakeStore->lastAppendKey);

    ASSERT_EQ(SM_OK, helper.StoreSliceInfo(info, sliceInfo));
    EXPECT_EQ(K_RECOVERED_SLICE_ID, helper.storeSliceInfo_.back().first);
    EXPECT_EQ(SENDER_SLICES_INFO_KEY, fakeStore->lastWriteKey);
}

TEST_F(SmemTransStoreHelperTest, ReStoreSliceInfoWritesEveryCachedSliceBack)
{
    auto fakeStore = MakeFakeStore();
    int addCallCount = 0;
    fakeStore->addHook = [&addCallCount](const std::string &, int64_t increment, int64_t &value) {
        addCallCount++;
        value = addCallCount * increment;
        return SUCCESS;
    };

    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);
    helper.localKeys_ = StoreKeys(SENDER_COUNT_KEY, SENDER_TOTAL_SLICE_COUNT_KEY, SENDER_DEVICE_INFO_KEY,
                                  SENDER_SLICES_INFO_KEY, SENDER_GET_DEVICE_ID_KEY, SENDER_GET_SLICES_ID_KEY);
    helper.storeSliceInfo_.emplace_back(
        K_STATUS_FIELD_SIZE,
        BuildSliceRecord(DataStatusType::NORMAL,
                         MakeStoredSliceInfo(K_RANK_ID_FOR_SLICE, K_SLICE_SIZE, K_SLICE_ADDRESS_SEED), K_SLICE_DESC_A));
    helper.storeSliceInfo_.emplace_back(
        K_RECOVERED_SLICE_ID,
        BuildSliceRecord(DataStatusType::NORMAL,
                         MakeStoredSliceInfo(K_RANK_ID_FOR_SLICE, K_SLICE_SIZE, K_SLICE_ADDRESS_SEED + 1),
                         K_SLICE_DESC_B));

    ASSERT_EQ(SM_OK, helper.ReStoreSliceInfo());
    EXPECT_EQ(SENDER_SLICES_INFO_KEY, fakeStore->lastWriteKey);
    EXPECT_EQ(static_cast<int>(helper.storeSliceInfo_.size()), addCallCount);
}

TEST_F(SmemTransStoreHelperTest, ExtraDeviceChangeInfoDetectsChangedRecoveredAndExpandedDevices)
{
    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.deviceExpSize_ = K_DEVICE_DESC_A.size();
    helper.remoteDeviceInfoLastTime_ = JoinRecords({BuildDeviceRecord(DataStatusType::NORMAL, K_DEVICE_DESC_A),
                                                    BuildDeviceRecord(DataStatusType::ABNORMAL, K_DEVICE_DESC_B)});
    std::vector<uint8_t> values = JoinRecords({BuildDeviceRecord(DataStatusType::NORMAL, K_DEVICE_DESC_C),
                                               BuildDeviceRecord(DataStatusType::NORMAL, K_DEVICE_DESC_B),
                                               BuildDeviceRecord(DataStatusType::NORMAL, K_DEVICE_DESC_D)});
    std::vector<hybm_exchange_info> addInfo;

    helper.ExtraDeviceChangeInfo(values, addInfo);

    ASSERT_EQ(K_ID_BYTE_COUNT + K_STATUS_FIELD_SIZE, addInfo.size());
    EXPECT_EQ(K_DEVICE_DESC_C.size(), addInfo[K_INITIAL_CALL_INDEX].descLen);
    EXPECT_EQ(K_DEVICE_DESC_B.size(), addInfo[K_SECOND_CALL_INDEX].descLen);
    EXPECT_EQ(K_DEVICE_DESC_D.size(), addInfo.back().descLen);
    EXPECT_EQ(values, helper.remoteDeviceInfoLastTime_);
}

TEST_F(SmemTransStoreHelperTest, ExtraSliceChangeInfoDetectsChangedRemovedAndExpandedSlices)
{
    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.sliceExpSize_ = K_SLICE_DESC_A.size();
    const StoredSliceInfo oldSlice = MakeStoredSliceInfo(K_RANK_ID_FOR_SLICE, K_SLICE_SIZE, K_SLICE_ADDRESS_SEED);
    const StoredSliceInfo removedSlice =
        MakeStoredSliceInfo(K_RANK_ID_FOR_SLICE, K_SLICE_SIZE, K_SLICE_ADDRESS_SEED + 1);
    const StoredSliceInfo changedSlice =
        MakeStoredSliceInfo(K_RANK_ID_FOR_SLICE + 1, K_SLICE_SIZE, K_SLICE_ADDRESS_SEED + 2);
    const StoredSliceInfo newSlice =
        MakeStoredSliceInfo(K_RANK_ID_FOR_SLICE + 2, K_SLICE_SIZE, K_SLICE_ADDRESS_SEED + 3);
    helper.remoteSlicesInfoLastTime_ =
        JoinRecords({BuildSliceRecord(DataStatusType::NORMAL, oldSlice, K_SLICE_DESC_A),
                     BuildSliceRecord(DataStatusType::NORMAL, removedSlice, K_SLICE_DESC_B)});

    std::vector<uint8_t> values = JoinRecords({BuildSliceRecord(DataStatusType::NORMAL, changedSlice, K_SLICE_DESC_C),
                                               BuildSliceRecord(DataStatusType::ABNORMAL, removedSlice, K_SLICE_DESC_B),
                                               BuildSliceRecord(DataStatusType::NORMAL, newSlice, K_SLICE_DESC_D)});
    std::vector<hybm_exchange_info> addInfo;
    std::vector<StoredSliceInfo> addStoreSlices;
    std::vector<StoredSliceInfo> removeStoreSlices;

    helper.ExtraSliceChangeInfo(values, addInfo, addStoreSlices, removeStoreSlices);

    EXPECT_EQ(K_ID_BYTE_COUNT, addInfo.size());
    EXPECT_EQ(K_ID_BYTE_COUNT, addStoreSlices.size());
    EXPECT_EQ(K_STATUS_FIELD_SIZE, removeStoreSlices.size());
    EXPECT_EQ(values, helper.remoteSlicesInfoLastTime_);
}

TEST_F(SmemTransStoreHelperTest, FindNewRemoteRanksSkipsEmptyRemoteStateAndInvokesCallbackForNewDevices)
{
    auto fakeStore = MakeFakeStore();
    bool returnEmpty = true;
    fakeStore->addHook = [&returnEmpty](const std::string &, int64_t, int64_t &value) {
        value = returnEmpty ? 0 : K_ID_BYTE_COUNT;
        return SUCCESS;
    };
    fakeStore->getHook = [&returnEmpty](const std::string &, std::vector<uint8_t> &value, int64_t) {
        if (returnEmpty) {
            return NOT_EXIST;
        }
        value = JoinRecords({BuildDeviceRecord(DataStatusType::NORMAL, K_DEVICE_DESC_A),
                             BuildDeviceRecord(DataStatusType::NORMAL, K_DEVICE_DESC_B)});
        return SUCCESS;
    };

    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);
    helper.deviceExpSize_ = K_DEVICE_DESC_A.size();
    helper.remoteKeys_ = StoreKeys(RECEIVER_COUNT_KEY, RECEIVER_TOTAL_SLICE_COUNT_KEY, RECEIVER_DEVICE_INFO_KEY,
                                   RECEIVER_SLICES_INFO_KEY, RECEIVER_GET_DEVICE_ID_KEY, RECEIVER_GET_SLICES_ID_KEY);

    int callbackCount = 0;
    helper.FindNewRemoteRanks([&callbackCount](const std::vector<hybm_exchange_info> &info) {
        callbackCount += static_cast<int>(info.size());
        return SM_OK;
    });
    EXPECT_EQ(0, callbackCount);

    returnEmpty = false;
    helper.FindNewRemoteRanks([&callbackCount](const std::vector<hybm_exchange_info> &info) {
        callbackCount += static_cast<int>(info.size());
        return SM_OK;
    });
    EXPECT_EQ(K_ID_BYTE_COUNT, callbackCount);
}

TEST_F(SmemTransStoreHelperTest, FindNewRemoteSlicesInvokesCallbackForAddedSlices)
{
    auto fakeStore = MakeFakeStore();
    fakeStore->addHook = [](const std::string &, int64_t, int64_t &value) {
        value = K_STATUS_FIELD_SIZE;
        return SUCCESS;
    };
    fakeStore->getHook = [](const std::string &, std::vector<uint8_t> &value, int64_t) {
        value = JoinRecords({BuildSliceRecord(
            DataStatusType::NORMAL, MakeStoredSliceInfo(K_RANK_ID_FOR_SLICE, K_SLICE_SIZE, K_SLICE_ADDRESS_SEED),
            K_SLICE_DESC_A)});
        return SUCCESS;
    };

    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);
    helper.sliceExpSize_ = K_SLICE_DESC_A.size();
    helper.remoteKeys_ = StoreKeys(RECEIVER_COUNT_KEY, RECEIVER_TOTAL_SLICE_COUNT_KEY, RECEIVER_DEVICE_INFO_KEY,
                                   RECEIVER_SLICES_INFO_KEY, RECEIVER_GET_DEVICE_ID_KEY, RECEIVER_GET_SLICES_ID_KEY);

    int addInfoCount = 0;
    int addSliceCount = 0;
    int removeSliceCount = 0;
    helper.FindNewRemoteSlices(
        [&addInfoCount, &addSliceCount, &removeSliceCount](const std::vector<hybm_exchange_info> &info,
                                                           const std::vector<StoredSliceInfo> &addSlices,
                                                           const std::vector<StoredSliceInfo> &removeSlices) {
            addInfoCount = static_cast<int>(info.size());
            addSliceCount = static_cast<int>(addSlices.size());
            removeSliceCount = static_cast<int>(removeSlices.size());
            return SM_OK;
        });

    EXPECT_EQ(K_STATUS_FIELD_SIZE, addInfoCount);
    EXPECT_EQ(K_STATUS_FIELD_SIZE, addSliceCount);
    EXPECT_EQ(0, removeSliceCount);
}

TEST_F(SmemTransStoreHelperTest, ReRegisterToServerWritesMissingRankInfoBack)
{
    auto fakeStore = MakeFakeStore();
    fakeStore->getHook = [](const std::string &, std::vector<uint8_t> &, int64_t) { return NOT_EXIST; };

    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);
    helper.storeRankIdInfo_.first = K_RECOVERED_RANK_ID;
    helper.storeRankIdInfo_.second.assign(sizeof(smem_trans_config_t), K_STATUS_FIELD_SIZE);

    ASSERT_EQ(SM_OK, helper.ReRegisterToServer(K_RECOVERED_RANK_ID));
    EXPECT_EQ(CLUSTER_RANKS_INFO_KEY, fakeStore->lastWriteKey);
    EXPECT_EQ(sizeof(smem_trans_config_t) * K_RECOVERED_RANK_ID, fakeStore->lastWriteOffset);
    EXPECT_EQ(AUTO_RANK_KEY_PREFIX + std::string(K_ENGINE_NAME), fakeStore->lastSetKey);
}

TEST_F(SmemTransStoreHelperTest, ReRegisterToServerRejectsInvalidExistingValueSize)
{
    auto fakeStore = MakeFakeStore();
    fakeStore->getHook = [](const std::string &, std::vector<uint8_t> &value, int64_t) {
        value.assign(K_INVALID_VALUE_SIZE, 0);
        return SUCCESS;
    };

    SmemStoreHelper helper(K_ENGINE_NAME, K_STORE_URL, SMEM_TRANS_SENDER);
    helper.store_ = ToStoreManagerPtr(fakeStore);

    EXPECT_EQ(SM_ERROR, helper.ReRegisterToServer(K_RECOVERED_RANK_ID));
}

} // namespace
