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
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "smem_ha_config_store.h"
#include "smem_tcp_config_store.h"
#include "smem_local_memory_backend.h"
#include "smem_etcd_store_backend.h"
#include "smem_store_factory.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

class SmemStoreFactoryTest : public testing::Test {
public:
    void SetUp() override
    {
    };

    void TearDown() override
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
    };
};

TEST_F(SmemStoreFactoryTest, create_store_success)
{
    MOCKER_CPP(&ock::smem::SmemLocalMemoryBackend::Initialize,
        int32_t (*)(ock::smem::SmemLocalMemoryBackend *, const std::string &, const std::string &,
        const std::string &)).stubs().will(returnValue(0));
    MOCKER_CPP(&ock::smem::TcpConfigStore::Startup,
        int32_t (*)(ock::smem::TcpConfigStore *, const smem_tls_config &, int)).stubs().will(returnValue(0));
    std::string ip = "127.0.0.1";
    uint16_t port = 16888;
    auto tcpStore = ock::smem::StoreFactory::CreateStore(ip, port, 1, 1, 0);
    ASSERT_NE(true, (tcpStore == nullptr));
    auto tcpStore2 = ock::smem::StoreFactory::CreateStore(ip, port, 1, 1, 0);
    ASSERT_NE(true, (tcpStore2 == nullptr));
    ock::smem::StoreFactory::DestroyStore(ip, port);
}

TEST_F(SmemStoreFactoryTest, create_store_failed)
{
    int32_t targetErr = -2009;
    MOCKER_CPP(&ock::smem::SmemLocalMemoryBackend::Initialize,
        int32_t (*)(ock::smem::SmemLocalMemoryBackend *, const std::string &, const std::string &,
        const std::string &)).stubs().will(returnValue(0));
    MOCKER_CPP(&ock::smem::TcpConfigStore::Startup,
        int32_t (*)(ock::smem::TcpConfigStore *, const smem_tls_config &, int))
        .stubs().will(returnValue(targetErr)).then(returnValue(-1));
    std::string ip = "127.0.0.1";
    uint16_t port = 16888;
    auto tcpStore = ock::smem::StoreFactory::CreateStore(ip, port, 1, 1, 0);
    ASSERT_EQ(true, (tcpStore == nullptr));
    auto tcpStore2 = ock::smem::StoreFactory::CreateStore(ip, port, 1, 1, 0);
    ASSERT_EQ(true, (tcpStore2 == nullptr));
}

TEST_F(SmemStoreFactoryTest, create_store_by_url_tcp_failed)
{
    int32_t targetErr = -2009;
    MOCKER_CPP(&ock::smem::SmemLocalMemoryBackend::Initialize,
        int32_t (*)(ock::smem::SmemLocalMemoryBackend *, const std::string &, const std::string &,
        const std::string &)).stubs().will(returnValue(0));
    MOCKER_CPP(&ock::smem::TcpConfigStore::Startup,
        int32_t (*)(ock::smem::TcpConfigStore *, const smem_tls_config &, int))
        .stubs().will(returnValue(targetErr)).then(returnValue(-1));
    std::string url = "tcp://127.0.0.1:16888";
    auto tcpStore = ock::smem::StoreFactory::CreateStoreByUrl(url, true, 1, 0);
    ASSERT_EQ(true, (tcpStore == nullptr));
    auto tcpStore2 = ock::smem::StoreFactory::CreateStoreByUrl(url, true, 1, 0);
    ASSERT_EQ(true, (tcpStore2 == nullptr));
}

TEST_F(SmemStoreFactoryTest, create_store_server_success)
{
    MOCKER_CPP(&ock::smem::SmemLocalMemoryBackend::Initialize,
        int32_t (*)(ock::smem::SmemLocalMemoryBackend *, const std::string &, const std::string &,
        const std::string &)).stubs().will(returnValue(0));
    MOCKER_CPP(&ock::smem::TcpConfigStore::ServerStart,
        int32_t (*)(ock::smem::TcpConfigStore *, const smem_tls_config &, int)).stubs().will(returnValue(0));
    std::string ip = "127.0.0.1";
    uint16_t port = 16888;
    auto tcpStore = ock::smem::StoreFactory::CreateStoreServer(ip, port, 1, 0);
    ASSERT_NE(true, (tcpStore == nullptr));
    auto tcpStore2 = ock::smem::StoreFactory::CreateStoreServer(ip, port, 1, 0);
    ASSERT_NE(true, (tcpStore2 == nullptr));
    ock::smem::StoreFactory::DestroyStore(ip, port);
}

TEST_F(SmemStoreFactoryTest, create_store_server_failed)
{
    int32_t targetErr = -2009;
    MOCKER_CPP(&ock::smem::SmemLocalMemoryBackend::Initialize,
        int32_t (*)(ock::smem::SmemLocalMemoryBackend *, const std::string &, const std::string &,
        const std::string &)).stubs().will(returnValue(0));
    MOCKER_CPP(&ock::smem::TcpConfigStore::ServerStart,
        int32_t (*)(ock::smem::TcpConfigStore *, const smem_tls_config &, int))
        .stubs().will(returnValue(targetErr)).then(returnValue(-1));
    std::string ip = "127.0.0.1";
    uint16_t port = 16888;
    auto tcpStore = ock::smem::StoreFactory::CreateStoreServer(ip, port, 1, 0);
    ASSERT_EQ(true, (tcpStore == nullptr));
    auto tcpStore2 = ock::smem::StoreFactory::CreateStoreServer(ip, port, 1, 0);
    ASSERT_EQ(true, (tcpStore2 == nullptr));
}

TEST_F(SmemStoreFactoryTest, create_store_client_success)
{
    MOCKER_CPP(&ock::smem::SmemLocalMemoryBackend::Initialize,
        int32_t (*)(ock::smem::SmemLocalMemoryBackend *, const std::string &, const std::string &,
        const std::string &)).stubs().will(returnValue(0));
    MOCKER_CPP(&ock::smem::TcpConfigStore::ClientStart,
        int32_t (*)(ock::smem::TcpConfigStore *, const smem_tls_config &, int)).stubs().will(returnValue(0));
    std::string ip = "127.0.0.1";
    uint16_t port = 16888;
    auto tcpStore = ock::smem::StoreFactory::CreateStoreClient(ip, port, 1, 0);
    ASSERT_NE(true, (tcpStore == nullptr));
    auto tcpStore2 = ock::smem::StoreFactory::CreateStoreClient(ip, port, 1, 0);
    ASSERT_NE(true, (tcpStore2 == nullptr));
    ock::smem::StoreFactory::DestroyStore(ip, port);
}

TEST_F(SmemStoreFactoryTest, create_store_client_failed)
{
    int32_t targetErr = -2009;
    MOCKER_CPP(&ock::smem::SmemLocalMemoryBackend::Initialize,
        int32_t (*)(ock::smem::SmemLocalMemoryBackend *, const std::string &, const std::string &,
        const std::string &)).stubs().will(returnValue(0));
    MOCKER_CPP(&ock::smem::TcpConfigStore::ClientStart,
        int32_t (*)(ock::smem::TcpConfigStore *, const smem_tls_config &, int))
        .stubs().will(returnValue(targetErr)).then(returnValue(-1));
    std::string ip = "127.0.0.1";
    uint16_t port = 16888;
    auto tcpStore = ock::smem::StoreFactory::CreateStoreClient(ip, port, 1, 0);
    ASSERT_EQ(true, (tcpStore == nullptr));
    auto tcpStore2 = ock::smem::StoreFactory::CreateStoreClient(ip, port, 1, 0);
    ASSERT_EQ(true, (tcpStore2 == nullptr));
    auto errorCode = ock::smem::StoreFactory::GetFailedReason();
    ASSERT_EQ(-1, errorCode);
}

TEST_F(SmemStoreFactoryTest, destroy_all_store)
{
    MOCKER_CPP(&ock::smem::SmemLocalMemoryBackend::Initialize,
        int32_t (*)(ock::smem::SmemLocalMemoryBackend *, const std::string &, const std::string &,
        const std::string &)).stubs().will(returnValue(0));
    MOCKER_CPP(&ock::smem::TcpConfigStore::Startup,
        int32_t (*)(ock::smem::TcpConfigStore *, const smem_tls_config &, int)).stubs().will(returnValue(0));
    std::string ip = "127.0.0.1";
    uint16_t port = 17888;
    auto tcpStore = ock::smem::StoreFactory::CreateStore(ip, port, 1, 1, 0);
    EXPECT_NE(true, (tcpStore == nullptr));
    ock::smem::StoreFactory::DestroyStoreAll();
}