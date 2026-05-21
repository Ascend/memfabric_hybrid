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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "smem_local_memory_backend.h"
#include "smem_net_common.h"
#include "smem_net_group_engine.h"
#include "smem_tcp_config_store.h"

using namespace ock::smem;

namespace {
constexpr uint16_t CONCURRENT_GROUP_TEST_PORT = 5678;
constexpr uint16_t ETCD_CLUSTER_TEST_PORT = 2379;
constexpr char ETCD_CLUSTER_URL[] = "etcd://127.0.0.1:2379#cluster-a";
constexpr char ETCD_CLUSTER_IPV6_URL[] = "etcd://[::1]:2379#cluster_a";
constexpr char ETCD_EMPTY_CLUSTER_URL[] = "etcd://127.0.0.1:2379#";
constexpr char REG_CLUSTER_URL[] = "reg://127.0.0.1:2379#cluster-a";
constexpr char REG_CLUSTER_IPV6_URL[] = "reg://[::1]:2379#cluster_a";
constexpr char REG_EMPTY_CLUSTER_URL[] = "reg://127.0.0.1:2379#";
constexpr char TCP_CLUSTER_URL[] = "tcp://127.0.0.1:5678#cluster-a";
constexpr char REG_INVALID_CLUSTER_URL[] = "reg://127.0.0.1:2379#cluster@a";

// Reusable barrier to force all workers enter each phase at the same time.
class SimpleBarrier {
public:
    explicit SimpleBarrier(size_t total) : total_(total) {}

    void ArriveAndWait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        const size_t generation = generation_;
        if (++count_ == total_) {
            count_ = 0;
            ++generation_;
            cond_.notify_all();
            return;
        }
        cond_.wait(lock, [&]() { return generation != generation_; });
    }

private:
    const size_t total_;
    size_t count_ = 0;
    size_t generation_ = 0;
    std::mutex mutex_;
    std::condition_variable cond_;
};

TEST(SmemNetGroupEngineTest, url_extraction_supports_distributed_cluster_fragment)
{
    UrlExtraction parser;

    ASSERT_EQ(parser.ExtractIpPortFromUrl(ETCD_CLUSTER_URL), SM_OK);
    EXPECT_EQ("127.0.0.1", parser.ip);
    EXPECT_EQ(ETCD_CLUSTER_TEST_PORT, parser.port);

    ASSERT_EQ(parser.ExtractIpPortFromUrl(ETCD_CLUSTER_IPV6_URL), SM_OK);
    EXPECT_EQ("::1", parser.ip);
    EXPECT_EQ(ETCD_CLUSTER_TEST_PORT, parser.port);

    ASSERT_EQ(parser.ExtractIpPortFromUrl(REG_CLUSTER_URL), SM_OK);
    EXPECT_EQ("127.0.0.1", parser.ip);
    EXPECT_EQ(ETCD_CLUSTER_TEST_PORT, parser.port);

    ASSERT_EQ(parser.ExtractIpPortFromUrl(REG_CLUSTER_IPV6_URL), SM_OK);
    EXPECT_EQ("::1", parser.ip);
    EXPECT_EQ(ETCD_CLUSTER_TEST_PORT, parser.port);

    EXPECT_EQ(parser.ExtractIpPortFromUrl(ETCD_EMPTY_CLUSTER_URL), SM_INVALID_PARAM);
    EXPECT_EQ(parser.ExtractIpPortFromUrl(REG_EMPTY_CLUSTER_URL), SM_INVALID_PARAM);
    EXPECT_EQ(parser.ExtractIpPortFromUrl(REG_INVALID_CLUSTER_URL), SM_INVALID_PARAM);
    EXPECT_EQ(parser.ExtractIpPortFromUrl(TCP_CLUSTER_URL), SM_INVALID_PARAM);
}

TEST(SmemNetGroupEngineTest, concurrent_clients_join_leave_twice_should_succeed_on_fixed_port)
{
    constexpr int32_t kThreadCount = 8;
    smem_tls_config tlsConfig{};

    // Initialize URL parser state required by acc_links before opening listener/client sockets.
    UrlExtraction parserInit;
    ASSERT_EQ(parserInit.ExtractIpPortFromUrl("tcp://127.0.0.1:5678"), SM_OK);

    auto serverBackend = SmMakeRef<SmemLocalMemoryBackend>();
    ASSERT_NE(serverBackend, nullptr);
    ASSERT_EQ(serverBackend->Initialize("0.0.0.0", "", ""), SUCCESS);

    auto server = SmMakeRef<TcpConfigStore>(Convert<SmemLocalMemoryBackend, ConfigStoreBackend>(serverBackend),
                                            "0.0.0.0", CONCURRENT_GROUP_TEST_PORT, ConfigStoreModel::CSM_SERVER,
                                            true, kThreadCount, -1);
    ASSERT_NE(server, nullptr);
    Result startRet = SM_ERROR;
    constexpr int kServerStartRetry = 10;
    for (int attempt = 0; attempt < kServerStartRetry; ++attempt) {
        startRet = server->Startup(tlsConfig);
        if (startRet == SM_OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    ASSERT_EQ(startRet, SM_OK);
    std::cout << "[concurrent_join_leave] server started at port " << CONCURRENT_GROUP_TEST_PORT << std::endl;

    auto serverGuard = std::unique_ptr<void, std::function<void(void *)>>{reinterpret_cast<void *>(1),
                                                                          [&server](void *) { server->Shutdown(); }};

    std::atomic<int32_t> failedCount{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    // 4 synchronization points: join1 -> leave1 -> join2 -> leave2.
    SimpleBarrier joinRound1Barrier(kThreadCount);
    SimpleBarrier leaveRound1Barrier(kThreadCount);
    SimpleBarrier joinRound2Barrier(kThreadCount);
    SimpleBarrier leaveRound2Barrier(kThreadCount);

    for (int32_t rank = 0; rank < kThreadCount; ++rank) {
        workers.emplace_back([&, rank]() {
            auto markFail = [&](const std::string &reason) {
                std::cout << "[concurrent_join_leave] rank " << rank << " failed: " << reason << std::endl;
                failedCount.fetch_add(1, std::memory_order_relaxed);
            };

            auto clientBackend = SmMakeRef<SmemLocalMemoryBackend>();
            if (clientBackend == nullptr || clientBackend->Initialize("127.0.0.1", "", "") != SUCCESS) {
                markFail("init backend");
                return;
            }

            auto client = SmMakeRef<TcpConfigStore>(Convert<SmemLocalMemoryBackend, ConfigStoreBackend>(clientBackend),
                                                    "127.0.0.1", CONCURRENT_GROUP_TEST_PORT,
                                                    ConfigStoreModel::CSM_CLIENT, true, kThreadCount, rank);
            if (client == nullptr || client->Startup(tlsConfig) != SM_OK) {
                markFail("start client");
                return;
            }

            auto clientGuard = std::unique_ptr<void, std::function<void(void *)>>{
                reinterpret_cast<void *>(1), [&client](void *) { client->Shutdown(); }};

            SmemGroupOption option{};
            option.rankSize = kThreadCount;
            option.rank = static_cast<uint32_t>(rank);
            option.timeoutMs = 10000;
            option.dynamic = true;

            auto group = SmemNetGroupEngine::Create(Convert<TcpConfigStore, ConfigStore>(client), option);
            if (group == nullptr) {
                markFail("create group");
                return;
            }

            std::cout << "[concurrent_join_leave] rank " << rank << " ready" << std::endl;

            joinRound1Barrier.ArriveAndWait();
            if (group->GroupJoin() != SM_OK) {
                markFail("round1 join");
            }

            leaveRound1Barrier.ArriveAndWait();
            if (group->GroupLeave() != SM_OK) {
                markFail("round1 leave");
            }

            joinRound2Barrier.ArriveAndWait();
            if (group->GroupJoin() != SM_OK) {
                markFail("round2 join");
            }

            leaveRound2Barrier.ArriveAndWait();
            if (group->GroupLeave() != SM_OK) {
                markFail("round2 leave");
            }

            std::cout << "[concurrent_join_leave] rank " << rank << " finished all rounds" << std::endl;
        });
    }

    for (auto &worker : workers) {
        worker.join();
    }

    std::cout << "[concurrent_join_leave] all workers finished, failure count="
              << failedCount.load(std::memory_order_relaxed) << std::endl;
    EXPECT_EQ(failedCount.load(std::memory_order_relaxed), 0);
}
} // namespace
