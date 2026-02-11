/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <sys/vfs.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <gtest/gtest.h>
#include <functional>
#include <random>
#include "hybm.h"
#include "hybm_big_mem.h"
#include "hybm_logger.h"

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

static const uint64_t DRAM_SIZE = 32U * 1024U * 1024U;

class HybmEntityShmFdTest : public testing::Test {
public:
    HybmEntityShmFdTest()
    {
        options.bmType = HYBM_TYPE_HOST_INITIATE;
        options.memType = HYBM_MEM_TYPE_HOST;
        options.bmDataOpType = HYBM_DOP_TYPE_SDMA;
        options.rankCount = 2U;
        options.rankId = 0;
        options.devId = -1;
        options.maxHBMSize = 0;
        options.maxDRAMSize = DRAM_SIZE;
        options.deviceVASpace = 0;
        options.hostVASpace = DRAM_SIZE;
        options.scene = HYBM_SCENE_DEFAULT;
        options.globalUniqueAddress = true;
        options.role = HYBM_ROLE_PEER;
        options.flags = HYBM_FLAG_CREATE_WITH_SHM;
        options.tag[0] = '\0';
        options.tagOpInfo[0] = '\0';
    }

    static void SetUpTestSuite()
    {
        struct statfs fsBuf{};
        auto ret = statfs("/dev/shm", &fsBuf);
        ASSERT_EQ(0, ret) << "statfs for /dev/shm failed: " << errno << ":" << strerror(errno);

        auto freeSize = static_cast<uint64_t>(fsBuf.f_bsize) * static_cast<uint64_t>(fsBuf.f_bfree);
        shmSupported = (freeSize >= 2U * DRAM_SIZE);
    }

    void SetUp() override
    {
        if (!shmSupported) {
            return;
        }
        auto fd = memfd_create("HybmEntityShmFdTest", MFD_CLOEXEC);
        ASSERT_TRUE(fd >= 0) << "memfd_create failed : " << errno << " : " << strerror(errno);

        auto ret = ftruncate(fd, DRAM_SIZE);
        if (ret != 0) {
            close(fd);
            ASSERT_EQ(0, ret) << "ftruncate failed: " << errno << ":" << strerror(errno);
        }

        mappingAddr = mmap(nullptr, DRAM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (mappingAddr == MAP_FAILED) {
            close(fd);
            ASSERT_NE(MAP_FAILED, mappingAddr) << "mapping failed: " << errno << ":" << strerror(errno);
        }
        memFd = fd;
        options.dramShmFd = memFd;
    }

    void TearDown() override
    {
        if (mappingAddr != nullptr && mappingAddr != MAP_FAILED) {
            munmap(mappingAddr, DRAM_SIZE);
            mappingAddr = nullptr;
        }

        if (memFd >= 0) {
            auto fd = memFd;
            memFd = -1;
            close(fd);
        }
    }

    void TestWrapper(const std::function<int(hybm_entity_t entity)> &test) noexcept
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
            ts.tv_sec += 2U;
            sem_timedwait(&connCtx->sem, &ts);
            pthread_spin_lock(&connCtx->lock);
            auto ret = connCtx->result;
            pthread_spin_unlock(&connCtx->lock);
            ASSERT_EQ(0, ret);
            return;
        }

        auto childProcess = [&test, this]() -> int {
            if (mappingAddr != nullptr && mappingAddr != MAP_FAILED) {
                munmap(mappingAddr, DRAM_SIZE);
                mappingAddr = nullptr;
            }

            auto ret = hybm_init(0, 0);
            if (ret != 0) {
                BM_LOG_ERROR("init hybm failed: " << ret);
                return ret;
            }
            AutoHandleCloser<int> hybmCloser([](int id) { hybm_uninit(); }, 0);

            auto entity = hybm_create_entity(0, &options, 0);
            if (entity == nullptr) {
                BM_LOG_ERROR("create entity failed.");
                return -1;
            }
            AutoHandleCloser<hybm_entity_t> entityCloser([](hybm_entity_t e) { hybm_destroy_entity(e, 0); }, entity);

            ret = hybm_reserve_mem_space(entity, 0);
            if (ret != 0) {
                BM_LOG_ERROR("reserve hybm failed: " << ret);
                return ret;
            }

            return test(entity);
        };
        auto testRet = childProcess();
        pthread_spin_lock(&connCtx->lock);
        connCtx->result = testRet;
        pthread_spin_unlock(&connCtx->lock);
        sem_post(&connCtx->sem);
        exit(0);
    }

protected:
    void RandomFill(uint64_t *begin, uint64_t count)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint64_t> dis(0UL, std::numeric_limits<uint64_t>::max());
        for (auto i = 0U; i < count; i++) {
            begin[i] = dis(gen);
        }
    }

protected:
    static bool shmSupported;
    hybm_options options{};
    int memFd{-1};
    void *mappingAddr{nullptr};
};

bool HybmEntityShmFdTest::shmSupported = false;

TEST_F(HybmEntityShmFdTest, add_check_result)
{
    if (!shmSupported) {
        return;
    }

    auto count = 8192UL;
    auto *xx = (uint64_t *)mappingAddr;
    auto *yy = xx + count;
    RandomFill(xx, count * 2U);

    TestWrapper([count](hybm_entity_t e) {
        auto slice = hybm_alloc_local_memory(e, HYBM_MEM_TYPE_HOST, DRAM_SIZE, 0);
        if (slice == nullptr) {
            BM_LOG_ERROR("allocate local memory failed.");
            return -1;
        }

        auto va = hybm_get_slice_va(e, slice);
        if (va == nullptr) {
            BM_LOG_ERROR("get slice va failed.");
            return -1;
        }

        auto *xx = (uint64_t *)va;
        auto *yy = xx + count;
        auto *zz = yy + count;
        for (auto i = 0UL; i < count; i++) {
            zz[i] = xx[i] + yy[i];
        }

        return 0;
    });

    auto *zz = yy + count;
    for (auto i = 0UL; i < count; i++) {
        ASSERT_EQ(zz[i], xx[i] + yy[i]) << "i=" << i << ", not equals.";
    }
}
