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
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <functional>
#include "smem.h"
#include "smem_bm.h"
#include "smem_logger.h"
#include "smem_types.h"
#include "hybm_data_op.h"
#include "hybm_types.h"
#include "network_endpoint_util.h"

template<class R>
class AutoHandleCloser {
public:
    AutoHandleCloser(std::function<void(R &)> closer, R h) noexcept : closer_{std::move(closer)}, handle_{h} {}
    ~AutoHandleCloser()
    {
        closer_(handle_);
    }

private:
    std::function<void(R &)> closer_;
    R handle_;
};

class SmemBmDataCopyTest : public testing::Test {
protected:
    void TestWrapper(const std::function<int(smem_bm_t)> &test)
    {
        struct ConnContext {
            sem_t sem;
            pthread_spinlock_t lock;
            int result;
        };

        auto fd = memfd_create("process_conn", MFD_CLOEXEC);
        ASSERT_TRUE(fd >= 0) << "memfd_create failed : " << errno << " : " << strerror(errno);

        AutoHandleCloser<int> fdCloser{[](int f) { close(f); }, fd};
        auto ret = ftruncate(fd, 4096U);
        ASSERT_EQ(0, ret) << "ftruncate failed : " << errno << " : " << strerror(errno);

        auto connAddr = mmap(nullptr, 4096U, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        ASSERT_TRUE(connAddr != MAP_FAILED) << "map() failed : " << errno << " : " << strerror(errno);
        AutoHandleCloser<void *> memCloser{[](void *p) { munmap(p, 4096U); }, connAddr};

        auto connCtx = (ConnContext *)connAddr;
        ret = sem_init(&connCtx->sem, 1, 0);
        ASSERT_EQ(0, ret) << "sem_init failed : " << errno << " : " << strerror(errno);

        ret = pthread_spin_init(&connCtx->lock, 1);
        ASSERT_EQ(0, ret) << "pthread_spin_init failed : " << errno << " : " << strerror(errno);

        auto pid = fork();
        ASSERT_TRUE(pid >= 0) << "fork() failed : " << errno << " : " << strerror(errno);

        if (pid > 0) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 120U;
            sem_timedwait(&connCtx->sem, &ts);
            pthread_spin_lock(&connCtx->lock);
            auto ret = connCtx->result;
            pthread_spin_unlock(&connCtx->lock);
            EXPECT_EQ(0, ret);
            return;
        }

        auto childProcess = [&test, this]() -> int {
            uint16_t configStorePort = 0;
            uint16_t hcomPort = 0;
            if (!ock::smem::NetworkEndpointUtil::FindAvailablePort(configStorePort, false)) {
                SM_LOG_ERROR("Failed to find available port for config store");
                return -1;
            }
            if (!ock::smem::NetworkEndpointUtil::FindAvailablePort(hcomPort, false)) {
                SM_LOG_ERROR("Failed to find available port for hcom");
                return -1;
            }

            smem_bm_config_t config{};
            config.initTimeout = 120;
            config.createTimeout = 120;
            config.controlOperationTimeout = 120;
            config.startConfigStoreServer = true;
            config.unifiedAddressSpace = true;
            snprintf(config.hcomUrl, sizeof(config.hcomUrl), "tcp://127.0.0.1:%u", hcomPort);

            char configStoreUrl[128];
            snprintf(configStoreUrl, sizeof(configStoreUrl), "tcp://127.0.0.1:%u", configStorePort);
            auto ret = smem_bm_init(configStoreUrl, 2, 0, &config);
            if (ret != 0) {
                SM_LOG_ERROR("smem_bm_init failed: " << ret);
                return ret;
            }
            AutoHandleCloser<int> bmInitCloser{[](int no) { smem_bm_uninit(0); }, 0};

            smem_bm_create_option_t option{};
            option.maxDramSize = option.localDRAMSize = 32U * 1024UL * 1024UL;
            option.dataOpType = SMEMB_DATA_OP_SDMA;
            auto handle = smem_bm_create2(0, &option);
            if (handle == nullptr) {
                SM_LOG_ERROR("create bm handle failed.");
                return -1;
            }
            AutoHandleCloser<smem_bm_t> handleCloser{[](smem_bm_t hd) { smem_bm_destroy(hd); }, handle};

            return test(handle);
        };

        auto testRet = childProcess();
        pthread_spin_lock(&connCtx->lock);
        connCtx->result = testRet;
        pthread_spin_unlock(&connCtx->lock);
        sem_post(&connCtx->sem);
        exit(0);
    }

    static int32_t hybm_data_copy_all_success_mocker(hybm_entity_t e, hybm_copy_params *params,
                                                     hybm_data_copy_direction direction, void *stream, uint32_t flags)
    {
        return ock::mf::BM_OK;
    }

    static int32_t hybm_data_copy_non_align_failed_mocker(hybm_entity_t e, hybm_copy_params *params,
                                                          hybm_data_copy_direction direction, void *stream,
                                                          uint32_t flags)
    {
        int ret;
        static std::mutex mutex;

        /*
         * 由于mockcpp机制，并行时返回值是发生错乱，这里采用锁加sleep来避免多线程同时返回发生的错乱
         */
        std::unique_lock<std::mutex> locker{mutex};
        std::this_thread::sleep_for(std::chrono::milliseconds(100U));
        auto ptr = (uint64_t)(ptrdiff_t)(void *)params->src;
        if (ptr % sizeof(uint64_t) != 0) {
            ret = ock::mf::BM_ERROR;
        } else {
            ret = ock::mf::BM_OK;
        }
        locker.unlock();

        return ret;
    }
};

TEST_F(SmemBmDataCopyTest, half_failed)
{
    auto testBody = [](smem_bm_t handle) -> int {
        MOCKER(hybm_data_copy).stubs().will(invoke(hybm_data_copy_non_align_failed_mocker));
        auto ptr = (uint8_t *)smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_HOST, 0);
        smem_batch_copy_params copyParams;
        copyParams.batchSize = 8UL;
        std::vector<void *> sources;
        std::vector<void *> destinations;
        std::vector<uint64_t> sizes;
        std::vector<int32_t> results(copyParams.batchSize);
        for (auto i = 0U; i < copyParams.batchSize; i++) {
            auto source = ptr + i * 4096UL;
            if (i % 2U != 0U) {
                source += 1;
            }
            sources.emplace_back(source);
            destinations.emplace_back(ptr + i * 4096UL + 4096UL * 1024UL);
            sizes.emplace_back((4096UL * 4096UL));
        }
        copyParams.sources = sources.data();
        copyParams.destinations = destinations.data();
        copyParams.dataSizes = sizes.data();
        smem_batch_copy_result copyResult;
        copyResult.batchSize = copyParams.batchSize;
        copyResult.results = results.data();
        auto ret = smem_bm_copy_batch_partial_succeed(handle, &copyParams, SMEMB_COPY_G2G, 0, &copyResult);
        if (ret != ock::smem::SM_PARTIAL_FAILED) {
            SM_LOG_ERROR("copy result should be " << ock::smem::SM_PARTIAL_FAILED << ", but return : " << ret);
            return -1;
        }
        for (auto i = 0U; i < copyParams.batchSize; i++) {
            int32_t expectRes = 0;
            if (i % 2U != 0) {
                expectRes = ock::mf::BM_ERROR;
            }
            if (copyResult.results[i] != expectRes) {
                SM_LOG_ERROR("copy result of(" << i << ") should be " << expectRes
                                               << ", but return : " << copyResult.results[i]);
                return -1;
            }
        }
        return 0;
    };
    TestWrapper(testBody);
}