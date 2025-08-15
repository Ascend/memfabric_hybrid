/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "mmc_global_allocator.h"
#include "mmc_ref.h"
#include "gtest/gtest.h"
#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestMmcGlobalAllocatorThread : public testing::Test {
public:
    TestMmcGlobalAllocatorThread();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestMmcGlobalAllocatorThread::TestMmcGlobalAllocatorThread() {}

void TestMmcGlobalAllocatorThread::SetUp()
{
    cout << "this is Allocator TEST_F setup:";
}

void TestMmcGlobalAllocatorThread::TearDown()
{
    cout << "this is Allocator TEST_F teardown";
}

// 被测试的线程函数
int AllocatorTest(const int worldSize, const int rankId, MmcGlobalAllocatorPtr allocator) {
    uint64_t size = SIZE_32K * 10;
    MmcLocation loc;
    MmcLocalMemlInitInfo info;
    loc.mediaType_ = MEDIA_DRAM;
    loc.rank_ = rankId;
    info.bmAddr_ = size * rankId;
    info.capacity_ = size;
    std::map<std::string, MmcMemBlobDesc> blobMap;
    Result result = allocator->Mount(loc, info);
    Result result1 = allocator->BuildFromBlobs(loc, blobMap);
    if (result != MMC_OK || result1 != MMC_OK) {
        std::cout << "allocator mount failed in rank: "<< rankId << std::endl;
        return -1;
    }
    allocator->Start(loc);

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K - 10000;
    allocReq.numBlobs_ = 1;
    allocReq.preferredRank_ = (rankId + 1) % worldSize;
    allocReq.mediaType_ = MEDIA_DRAM;

    result = allocator->Alloc(allocReq, blobs);
    if (result != MMC_OK || blobs.size() != allocReq.numBlobs_) {
        std::cout << "allocator alloc failed in rank: " << rankId << ", ret:" << result << ", size:" << blobs.size()
                  << std::endl;
        return -2;
    }

    std::vector<MmcMemBlobPtr> blobs1;
    allocReq.blobSize_ = SIZE_32K + 5000;
    allocReq.numBlobs_ = 1;

    result = allocator->Alloc(allocReq, blobs1);
    if (result != MMC_OK || blobs1.size() != allocReq.numBlobs_) {
        std::cout << "2 allocator alloc failed in rank: "<< rankId << std::endl;
        return -2;
    }

    for (auto &blob : blobs) {
        result = allocator->Free(blob);
        if (result != MMC_OK) {
            std::cout << "1 allocator free failed in rank: "<< rankId << std::endl;
            return -3;
        }
    }

    for (auto &blob : blobs1) {
        result = allocator->Free(blob);
        if (result != MMC_OK) {
            std::cout << "2 allocator free failed in rank: "<< rankId << std::endl;
            return -3;
        }
    }

    blobs.clear();
    blobs1.clear();

    std::this_thread::sleep_for(std::chrono::seconds(3));

    result = allocator->Unmount(loc);
    if (result != MMC_OK) {
        std::cout << "allocator unmount failed in rank: "<< rankId << std::endl;
        return -4;
    }

    return 0;
}

TEST(TestMmcGlobalAllocatorThread, AllocatorTest) {
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();

    std::vector<std::thread> threads;
    int threadNum = 10;
    std::mutex resultsMutex;
    std::vector<int> results(threadNum);
 
    for (int i = 0; i < threadNum; ++i) {
        threads.emplace_back([&, i] {
            int ret = AllocatorTest(threadNum, i, allocator);

            std::lock_guard<std::mutex> lock(resultsMutex);
            results[i] = ret;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    for (int i = 0; i < threadNum; ++i) {
        EXPECT_EQ(results[i], 0);
    }

}