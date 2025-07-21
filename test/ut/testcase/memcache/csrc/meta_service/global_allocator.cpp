/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "mmc_global_allocator.h"
#include "mmc_ref.h"
#include "gtest/gtest.h"
#include <iostream>

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestMmcGlobalAllocator : public testing::Test {
public:
    TestMmcGlobalAllocator();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestMmcGlobalAllocator::TestMmcGlobalAllocator() {}

void TestMmcGlobalAllocator::SetUp()
{
    cout << "this is Allocator TEST_F setup:";
}

void TestMmcGlobalAllocator::TearDown()
{
    cout << "this is Allocator TEST_F teardown";
}

TEST_F(TestMmcGlobalAllocator, AllocOne)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    uint64_t size = SIZE_32K * 10;
    for (int i = 0; i < 10; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_DRAM;
        loc.rank_ = i;
        info.bm_ = size * i;
        info.capacity_ = size;
        allocator->Mount(loc, info);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bm_ = size * i;
        info.capacity_ = size;
        allocator->Mount(loc, info);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 1;
    allocReq.preferredRank_ = 5;
    allocReq.mediaType_ = MEDIA_DRAM;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 1U);
    EXPECT_EQ(blobs[0]->Rank(), allocReq.preferredRank_);
    EXPECT_EQ(blobs[0]->Size(), allocReq.blobSize_);
    EXPECT_EQ(blobs[0]->Type(), allocReq.mediaType_);
    EXPECT_EQ(blobs[0]->Gva(), size * allocReq.preferredRank_);
}

TEST_F(TestMmcGlobalAllocator, AllocMulti)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    uint64_t size = SIZE_32K * 10;
    for (int i = 0; i < 10; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_DRAM;
        loc.rank_ = i;
        info.bm_ = size * i;
        info.capacity_ = size;
        allocator->Mount(loc, info);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bm_ = size * i;
        info.capacity_ = size;
        allocator->Mount(loc, info);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 10;
    allocReq.preferredRank_ = 5;
    allocReq.mediaType_ = 0;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 10U);
    for (size_t i = 0; i < blobs.size(); i++) {
        EXPECT_EQ(blobs[i]->Rank(), allocReq.preferredRank_);
        EXPECT_EQ(blobs[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs[i]->Gva(), size * allocReq.preferredRank_ + i * allocReq.blobSize_);
    }
}

TEST_F(TestMmcGlobalAllocator, AllocCrossRank)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    uint64_t size = SIZE_32K * 10;
    for (int i = 0; i < 10; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_DRAM;
        loc.rank_ = i;
        info.bm_ = size * i;
        info.capacity_ = size;
        allocator->Mount(loc, info);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bm_ = size * i;
        info.capacity_ = size;
        allocator->Mount(loc, info);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 12;
    allocReq.preferredRank_ = 5;
    allocReq.mediaType_ = 0;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 12U);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(blobs[i]->Rank(), allocReq.preferredRank_);
        EXPECT_EQ(blobs[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs[i]->Gva(), size * allocReq.preferredRank_ + i * allocReq.blobSize_);
    }
    for (int i = 10; i < 12; i++) {
        EXPECT_EQ(blobs[i]->Rank(), allocReq.preferredRank_ + 1);
        EXPECT_EQ(blobs[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs[i]->Gva(), size * (allocReq.preferredRank_ + 1) + (i - 10) * allocReq.blobSize_);
    }
}

TEST_F(TestMmcGlobalAllocator, FreeOne)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    uint64_t size = SIZE_32K * 10;
    for (int i = 0; i < 10; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_DRAM;
        loc.rank_ = i;
        info.bm_ = size * i;
        info.capacity_ = size;
        allocator->Mount(loc, info);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bm_ = size * i;
        info.capacity_ = size;
        allocator->Mount(loc, info);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 1;
    allocReq.preferredRank_ = 5;
    allocReq.mediaType_ = 0;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 1U);
    EXPECT_EQ(blobs[0]->Rank(), allocReq.preferredRank_);
    EXPECT_EQ(blobs[0]->Size(), allocReq.blobSize_);
    EXPECT_EQ(blobs[0]->Type(), allocReq.mediaType_);
    EXPECT_EQ(blobs[0]->Gva(), size * allocReq.preferredRank_);

    ret = allocator->Free(blobs[0]);
    EXPECT_EQ(ret, MMC_OK);
    blobs.clear();

    ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 1U);
    EXPECT_EQ(blobs[0]->Rank(), allocReq.preferredRank_);
    EXPECT_EQ(blobs[0]->Size(), allocReq.blobSize_);
    EXPECT_EQ(blobs[0]->Type(), allocReq.mediaType_);
    EXPECT_EQ(blobs[0]->Gva(), size * allocReq.preferredRank_);
}

TEST_F(TestMmcGlobalAllocator, FreeCrossRank)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    uint64_t size = SIZE_32K * 10;
    for (int i = 0; i < 10; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_DRAM;
        loc.rank_ = i;
        info.bm_ = size * i;
        info.capacity_ = size;
        allocator->Mount(loc, info);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bm_ = size * i;
        info.capacity_ = size;
        allocator->Mount(loc, info);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 12;
    allocReq.preferredRank_ = 5;
    allocReq.mediaType_ = 0;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 12U);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(blobs[i]->Rank(), allocReq.preferredRank_);
        EXPECT_EQ(blobs[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs[i]->Gva(), size * allocReq.preferredRank_ + i * allocReq.blobSize_);
    }
    for (int i = 10; i < 12; i++) {
        EXPECT_EQ(blobs[i]->Rank(), allocReq.preferredRank_ + 1);
        EXPECT_EQ(blobs[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs[i]->Gva(), size * (allocReq.preferredRank_ + 1) + (i - 10) * allocReq.blobSize_);
    }

    ret = allocator->Free(blobs[3]);
    EXPECT_EQ(ret, MMC_OK);
    blobs.clear();

    allocReq.numBlobs_ = 1;
    ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 1U);
    EXPECT_EQ(blobs[0]->Rank(), allocReq.preferredRank_);
    EXPECT_EQ(blobs[0]->Size(), allocReq.blobSize_);
    EXPECT_EQ(blobs[0]->Type(), allocReq.mediaType_);
    EXPECT_EQ(blobs[0]->Gva(), size * allocReq.preferredRank_ + 3 * allocReq.blobSize_);
}

TEST_F(TestMmcGlobalAllocator, MountUnmount)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    uint64_t size = SIZE_32K * 10;
    for (int i = 0; i < 10; i++) {
        if (i == 6) {
            continue;
        }
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_DRAM;
        loc.rank_ = i;
        info.bm_ = size * i;
        info.capacity_ = size;
        allocator->Mount(loc, info);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bm_ = size * i;
        info.capacity_ = size;
        allocator->Mount(loc, info);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 12;
    allocReq.preferredRank_ = 5;
    allocReq.mediaType_ = 0;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 12U);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(blobs[i]->Rank(), allocReq.preferredRank_);
        EXPECT_EQ(blobs[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs[i]->Gva(), size * allocReq.preferredRank_ + i * allocReq.blobSize_);
    }
    for (int i = 10; i < 12; i++) {
        EXPECT_EQ(blobs[i]->Rank(), allocReq.preferredRank_ + 2);
        EXPECT_EQ(blobs[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs[i]->Gva(), size * (allocReq.preferredRank_ + 2) + (i - 10) * allocReq.blobSize_);
    }

    EXPECT_TRUE(allocator->TouchedThreshold(8));
    EXPECT_FALSE(allocator->TouchedThreshold(9));

    ret = allocator->Free(blobs[3]);
    EXPECT_EQ(ret, MMC_OK);

    std::vector<MmcMemBlobPtr> blobs1;

    allocReq.numBlobs_ = 1;
    ret = allocator->Alloc(allocReq, blobs1);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs1.size(), 1U);
    EXPECT_EQ(blobs1[0]->Rank(), allocReq.preferredRank_);
    EXPECT_EQ(blobs1[0]->Size(), allocReq.blobSize_);
    EXPECT_EQ(blobs1[0]->Type(), allocReq.mediaType_);
    EXPECT_EQ(blobs1[0]->Gva(), size * allocReq.preferredRank_ + 3 * allocReq.blobSize_);

    MmcLocation loc;
    MmcLocalMemlInitInfo info;
    loc.mediaType_ = MEDIA_DRAM;
    loc.rank_ = 6;
    info.bm_ = size * 6;
    info.capacity_ = size;
    ret = allocator->Mount(loc, info);
    EXPECT_EQ(ret, MMC_OK);

    std::vector<MmcMemBlobPtr> blobs2;

    allocReq.numBlobs_ = 10;
    ret = allocator->Alloc(allocReq, blobs2);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs2.size(), 10U);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(blobs2[i]->Rank(), allocReq.preferredRank_ + 1);
        EXPECT_EQ(blobs2[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs2[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs2[i]->Gva(), size * (allocReq.preferredRank_ + 1) + i * allocReq.blobSize_);
    }

    EXPECT_TRUE(allocator->TouchedThreshold(14));
    EXPECT_FALSE(allocator->TouchedThreshold(15));

    loc.rank_ = 7;
    ret = allocator->Unmount(loc);
    EXPECT_EQ(ret, MMC_INVALID_PARAM);

    for (int i = 10; i < 12; i++) {
        allocator->Free(blobs[i]);
    }

    ret = allocator->Unmount(loc);
    EXPECT_EQ(ret, MMC_OK);

    blobs.clear();

    ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 10U);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(blobs[i]->Rank(), allocReq.preferredRank_ + 3);
        EXPECT_EQ(blobs[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs[i]->Gva(), size * (allocReq.preferredRank_ + 3) + i * allocReq.blobSize_);
    }

    EXPECT_TRUE(allocator->TouchedThreshold(21));
    EXPECT_FALSE(allocator->TouchedThreshold(22));
}