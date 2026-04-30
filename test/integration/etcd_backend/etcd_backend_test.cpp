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

#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include "smem.h"
#include "smem_etcd_store_backend.h"

// 使用 std::mutex 保护控制台输出，防止多线程打印混乱
std::mutex g_console_mutex;

// 定义日志宏，使用锁保护输出
#undef LOG_INFO
#undef LOG_WARN
#undef LOG_ERROR
#define LOG_INFO(msg)                                                               \
    {                                                                               \
        std::lock_guard<std::mutex> lock(g_console_mutex);                          \
        std::cout << __FILE__ << ":" << __LINE__ << " [INFO] " << msg << std::endl; \
    }
#define LOG_WARN(msg)                                                               \
    {                                                                               \
        std::lock_guard<std::mutex> lock(g_console_mutex);                          \
        std::cout << __FILE__ << ":" << __LINE__ << " [WARN] " << msg << std::endl; \
    }
#define LOG_ERROR(msg)                                                               \
    {                                                                                \
        std::lock_guard<std::mutex> lock(g_console_mutex);                           \
        std::cout << __FILE__ << ":" << __LINE__ << " [ERROR] " << msg << std::endl; \
    }

using namespace ock::smem;
SmemEtcdStoreBackend backend("test-instance-1");

// 线程函数：执行 PrefixGet 操作
void ThreadWorker(int thread_id, const std::string &etcd_addr, const std::string &prefix,
                  std::atomic<int> &success_count)
{
    auto initRet = backend.Initialize(etcd_addr, "", "");
    if (initRet != StoreErrorCode::SUCCESS) {
        LOG_ERROR("Thread " << thread_id << " Initialization Failed");
        return;
    }

    for (int i = 0; i < 5; ++i) {
        PrefixGetMap result;
        auto getRet = backend.PrefixGet(prefix, result);
        {
            std::lock_guard<std::mutex> lock(g_console_mutex);

            if (getRet == StoreErrorCode::SUCCESS) {
                success_count.fetch_add(1);

                std::cout << "   [Thread-" << thread_id << "] SUCCESS! Found " << result.size() << " items."
                          << std::endl;

                if (result.size() > 0) {
                    for (const auto &[key, val_vec] : result) {
                        std::string val_str(val_vec.begin(), val_vec.end());
                        std::cout << "      - Key: " << key << " | Value: " << val_str << std::endl;
                    }
                } else {
                    std::cout << "      (List is empty. Current Prefix being queried: \"" << prefix << "\")"
                              << std::endl;
                }
            } else {
                std::cerr << "   [Thread-" << thread_id << "] FAILED! Error code: " << static_cast<int>(getRet)
                          << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main(int argc, char *argv[])
{
    std::string etcd_addr = "etcd://127.0.0.1:2379";
    int thread_count = 4; // 默认线程数

    // 解析参数：支持 <etcd-address> <thread-count> 或仅 <etcd-address>
    if (argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <etcd-address> [thread-count]" << std::endl;
        std::cerr << "Example: " << argv[0] << " etcd://127.0.0.1:2379 8" << std::endl;
        return -1;
    }

    if (argc >= 2) {
        etcd_addr = argv[1];
    }
    if (argc == 3) {
        thread_count = std::stoi(argv[2]);
    }

    std::cout << "=== SmemEtcdStoreBackend Multi-Thread PrefixGet Test ===" << std::endl;
    std::cout << "ETCD Address: " << etcd_addr << std::endl;
    std::cout << "Thread Count: " << thread_count << std::endl;

    // 预埋数据
    auto initRet = backend.Initialize(etcd_addr, "", "");
    if (initRet != StoreErrorCode::SUCCESS) {
        std::cerr << "[FATAL] Failed to initialize backend for setup." << std::endl;
        return -1;
    }

    std::cout << "\n[Step 1] Writing test data..." << std::endl;
    std::vector<std::pair<std::string, std::string>> testData = {
        {"/memfabric_hybrid/config_store/meta/leader", "rank_0"},
        {"/memfabric_hybrid/config_store/meta/leader_status", "active"},
        {"/memfabric_hybrid/config_store/data/world_size", "8"},
        {"/memfabric_hybrid/config_store/other/random_key", "junk"}};

    for (const auto &[k, v] : testData) {
        std::vector<uint8_t> val(v.begin(), v.end());
        backend.Put(k, val, 0);
        std::cout << "   PUT: " << k << " = " << v << std::endl;
    }

    std::cout << "\n[Step 2] Starting " << thread_count << " threads for PrefixGet test..." << std::endl;

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    std::string prefix = "/memfabric_hybrid/config_store/meta/";

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(ThreadWorker, i, etcd_addr, prefix, std::ref(success_count));
    }

    for (auto &t : threads) {
        t.join();
    }

    std::cout << "\n=== Test Finished ===" << std::endl;
    std::cout << "Total successful PrefixGet operations: " << success_count.load() << std::endl;
    return 0;
}