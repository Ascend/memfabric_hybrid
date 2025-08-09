/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <thread>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <string>
#include <vector>
#include "hybm_big_mem.h"
#include "hybm_data_op.h"
#include "acc_log.h"
#include "hybm_stub.h"
#define private public
#include "smem_bm_entry_manager.h"
#undef private
#include "smem_tcp_config_store.h"
#include "smem_config_store.h"
#include "smem_prefix_config_store.h"
#include "smem_store_factory.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace ock::smem;

const std::string UT_BM_IP_PORT = "tcp://127.0.0.1:7758";

class TestSmemBmEntryManager : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp() override;
    void TearDown() override;

};

void TestSmemBmEntryManager::SetUpTestCase() {}

void TestSmemBmEntryManager::TearDownTestCase() {}

void TestSmemBmEntryManager::SetUp()
{
    mock_hybm_api();
    MOCKER_CPP(&TcpConfigStore::Append, int32_t(*)(TcpConfigStore *, const std::string &,
        const std::vector<uint8_t> &, uint64_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(static_cast<int32_t (ConfigStore::*)(const std::string &,
        std::string &, int64_t)>(&ConfigStore::Get), int32_t(*)(ConfigStore *,
        const std::string &, std::string &, int64_t)).stubs().will(returnValue(0));
    MOCKER_CPP(static_cast<int32_t (ConfigStore::*)(const std::string &,
        std::vector<uint8_t> &, int64_t)>(&ConfigStore::Get), int32_t(*)(ConfigStore *,
        const std::string &, std::vector<uint8_t> &, int64_t)).stubs().will(returnValue(0));
    MOCKER_CPP(&TcpConfigStore::Append, int32_t(*)(TcpConfigStore *, const std::string &,
        const std::vector<uint8_t> &)).stubs().will(returnValue(0));
    MOCKER_CPP(&TcpConfigStore::Add, int32_t(*)(TcpConfigStore *, const std::string &,
        int64_t, int64_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(&TcpConfigStore::Set, int32_t(*)(TcpConfigStore *, const std::string &,
        const std::vector<uint8_t> &)).stubs().will(returnValue(0));
    MOCKER_CPP(&TcpConfigStore::Remove, int32_t(*)(TcpConfigStore *, const std::string &))
        .stubs().will(returnValue(0));
}

void TestSmemBmEntryManager::TearDown()
{
    GlobalMockObject::verify();
    GlobalMockObject::reset();
    SmemBmEntryManager::Instance().inited_ = false;
    SmemBmEntryManager::Instance().ptr2EntryMap_.clear();
    SmemBmEntryManager::Instance().entryIdMap_.clear();
    SmemBmEntryManager::Instance().confStore_ = nullptr;
}

TEST_F(TestSmemBmEntryManager, init_already_inited)
{
    SmemBmEntryManager::Instance().inited_ = true;
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    auto ret = SmemBmEntryManager::Instance().Initialize(UT_BM_IP_PORT, 2, 0, config);
    EXPECT_EQ(ret, 0);
    SmemBmEntryManager::Instance().inited_ = false;
}

TEST_F(TestSmemBmEntryManager, init_failed_invalid_url)
{
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    std::string invalidIpPort = "255.255.255.555:66666";
    auto ret = SmemBmEntryManager::Instance().Initialize(invalidIpPort, 2, 0, config);
    EXPECT_NE(ret, 0);
}

StorePtr UtBmCreateStoreStub(StoreFactory *fac, const std::string &ip, uint16_t port,
    bool isServer, int32_t rankId, int32_t connMaxRetry)
{
    auto tcpStore = SmMakeRef<TcpConfigStore>("127.0.0.1", 8761, false, 0);
    return tcpStore.Get();
}

// TEST_F(TestSmemBmEntryManager, init_auto_ranking_success)
// {
//     smem_bm_config_t config;
//     smem_bm_config_init(&config);
//     config.autoRanking = true;
//     MOCKER_CPP(&StoreFactory::CreateStore, StorePtr(*)(StoreFactory *, const std::string &,
//         uint16_t, bool, int32_t, int32_t)).stubs().will(invoke(UtBmCreateStoreStub));
//     MOCKER_CPP(&StoreFactory::GetFailedReason, int(*)(StoreFactory *)).stubs().will(returnValue(0));
//     auto ret = SmemBmEntryManager::Instance().Initialize(UT_BM_IP_PORT, 2, 0, config);
//     EXPECT_EQ(ret, 0);
// }

TEST_F(TestSmemBmEntryManager, get_entry_by_id_success)
{
    uint32_t id = 10;
    SmemBmEntryOptions options;
    auto tcpStore = SmMakeRef<TcpConfigStore>("127.0.0.1", 8475, false, 0);
    StorePtr store = tcpStore.Get();
    auto tmpEntry = SmMakeRef<SmemBmEntry>(options, store);
    SmemBmEntryManager::Instance().inited_ = true;
    SmemBmEntryManager::Instance().entryIdMap_.insert(std::make_pair(id, tmpEntry));
    SmemBmEntryPtr handle = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryById(id, handle);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(handle, nullptr);
}