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
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 1;
    allocReq.preferredRank_.push_back(5);
    allocReq.mediaType_ = MEDIA_DRAM;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 1U);
    EXPECT_EQ(blobs[0]->Rank(), allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]);
    EXPECT_EQ(blobs[0]->Size(), allocReq.blobSize_);
    EXPECT_EQ(blobs[0]->Type(), allocReq.mediaType_);
    EXPECT_EQ(blobs[0]->Gva(), size * (allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]));
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
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 10;
    allocReq.preferredRank_.push_back(5);
    allocReq.mediaType_ = 0;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 10U);
    for (size_t i = 0; i < blobs.size(); i++) {
        uint32_t rank = ((allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]) + i) % 10;
        EXPECT_EQ(blobs[i]->Rank(), rank);
        EXPECT_EQ(blobs[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs[i]->Gva(), size * rank);
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
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 12;
    allocReq.preferredRank_.push_back(5);
    allocReq.mediaType_ = 0;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, -1);
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
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 1;
    allocReq.preferredRank_.push_back(5);
    allocReq.mediaType_ = 0;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 1U);
    EXPECT_EQ(blobs[0]->Rank(), allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]);
    EXPECT_EQ(blobs[0]->Size(), allocReq.blobSize_);
    EXPECT_EQ(blobs[0]->Type(), allocReq.mediaType_);
    EXPECT_EQ(blobs[0]->Gva(), size * (allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]));

    ret = allocator->Free(blobs[0]);
    EXPECT_EQ(ret, MMC_OK);
    blobs.clear();

    ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 1U);
    EXPECT_EQ(blobs[0]->Rank(), allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]);
    EXPECT_EQ(blobs[0]->Size(), allocReq.blobSize_);
    EXPECT_EQ(blobs[0]->Type(), allocReq.mediaType_);
    EXPECT_EQ(blobs[0]->Gva(), size * (allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]));
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
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 10;
    allocReq.preferredRank_.push_back(5);
    allocReq.mediaType_ = 0;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), allocReq.numBlobs_);
    for (int i = 0; i < 10; i++) {
        uint32_t rank = ((allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]) + i) % 10;
        EXPECT_EQ(blobs[i]->Rank(), rank);
        EXPECT_EQ(blobs[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs[i]->Gva(), size * rank);
    }

    ret = allocator->Free(blobs[3]);
    EXPECT_EQ(ret, MMC_OK);
    blobs.clear();

    allocReq.numBlobs_ = 1;
    ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 1U);
    EXPECT_EQ(blobs[0]->Rank(), allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]);
    EXPECT_EQ(blobs[0]->Size(), allocReq.blobSize_);
    EXPECT_EQ(blobs[0]->Type(), allocReq.mediaType_);
    EXPECT_EQ(blobs[0]->Gva(),
              size * (allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]) + 1 * allocReq.blobSize_);
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
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 9;
    allocReq.preferredRank_.push_back(5);
    allocReq.mediaType_ = 0;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 9);

    ret = allocator->Free(blobs[3]);
    EXPECT_EQ(ret, MMC_OK);

    std::vector<MmcMemBlobPtr> blobs1;

    allocReq.numBlobs_ = 1;
    ret = allocator->Alloc(allocReq, blobs1);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs1.size(), 1U);
    EXPECT_EQ(blobs1[0]->Rank(), allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]);
    EXPECT_EQ(blobs1[0]->Size(), allocReq.blobSize_);
    EXPECT_EQ(blobs1[0]->Type(), allocReq.mediaType_);
    EXPECT_EQ(blobs1[0]->Gva(),
              size * (allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]) + 1 * allocReq.blobSize_);

    MmcLocation loc;
    MmcLocalMemlInitInfo info;
    loc.mediaType_ = MEDIA_DRAM;
    loc.rank_ = 6;
    info.bmAddr_ = size * 6;
    info.capacity_ = size;
    std::map<std::string, MmcMemBlobDesc> blobMap;
    ret = allocator->Mount(loc, info);
    EXPECT_EQ(ret, MMC_OK);
    ret = allocator->BuildFromBlobs(loc, blobMap);
    EXPECT_EQ(ret, MMC_OK);
    allocator->Start(loc);

    std::vector<MmcMemBlobPtr> blobs2;

    allocReq.numBlobs_ = 10;
    ret = allocator->Alloc(allocReq, blobs2);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs2.size(), 10U);
    for (int i = 0; i < 10; i++) {
        uint32_t rank = ((allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]) + i) % 10;
        EXPECT_EQ(blobs2[i]->Rank(), rank);
        EXPECT_EQ(blobs2[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs2[i]->Type(), allocReq.mediaType_);
    }
}

TEST_F(TestMmcGlobalAllocator, AllocWhenEmpty)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 1;
    allocReq.preferredRank_.push_back(0);
    allocReq.mediaType_ = MEDIA_DRAM;
    
    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_ERROR);
    EXPECT_TRUE(blobs.empty());
}

TEST_F(TestMmcGlobalAllocator, MountDuplicateLocation)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    uint64_t size = SIZE_32K * 10;

    MmcLocation loc;
    MmcLocalMemlInitInfo info;
    loc.mediaType_ = MEDIA_DRAM;
    loc.rank_ = 0;
    info.capacity_ = size;
    std::map<std::string, MmcMemBlobDesc> blobMap;
    Result ret = allocator->Mount(loc, info);
    EXPECT_EQ(ret, MMC_OK);
    ret = allocator->BuildFromBlobs(loc, blobMap);
    EXPECT_EQ(ret, MMC_OK);
    ret = allocator->Mount(loc, info);
    EXPECT_EQ(ret, MMC_OK);
}

TEST_F(TestMmcGlobalAllocator, UnmountInUseLocation)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    uint64_t size = SIZE_32K * 10;

    MmcLocation loc;
    loc.mediaType_ = MEDIA_DRAM;
    loc.rank_ = 0;
    MmcLocalMemlInitInfo info;
    info.capacity_ = size;
    std::map<std::string, MmcMemBlobDesc> blobMap;
    allocator->Mount(loc, info);
    allocator->BuildFromBlobs(loc, blobMap);
    allocator->Start(loc);
    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;
    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 1;
    allocReq.preferredRank_.push_back(0);
    allocReq.mediaType_ = MEDIA_DRAM;
    Result ret = allocator->Alloc(allocReq, blobs);
    ASSERT_EQ(ret, MMC_OK);

    ret = allocator->Unmount(loc);
    EXPECT_EQ(ret, MMC_INVALID_PARAM);

    allocator->Free(blobs[0]);
    ret = allocator->Unmount(loc);
    EXPECT_EQ(ret, MMC_OK);
}

TEST_F(TestMmcGlobalAllocator, StopInvalidLocation)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    MmcLocation loc;
    loc.mediaType_ = MEDIA_DRAM;
    loc.rank_ = 0;
    Result ret = allocator->Stop(loc);
    EXPECT_EQ(ret, MMC_INVALID_PARAM);
}

TEST_F(TestMmcGlobalAllocator, ReBuild)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    uint64_t size = SIZE_32K * 10;

    MmcLocation loc;
    MmcLocalMemlInitInfo info;
    loc.mediaType_ = MEDIA_DRAM;
    loc.rank_ = 0;
    info.bmAddr_ = 0;
    info.capacity_ = size;
    std::map<std::string, MmcMemBlobDesc> blobMap;
    allocator->Mount(loc, info);
    allocator->BuildFromBlobs(loc, blobMap);
    allocator->Start(loc);

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;


    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 1;
    allocReq.preferredRank_.push_back(0);
    allocReq.mediaType_ = MEDIA_DRAM;

    Result ret;
    for (size_t i = 0; i < 10; i++) {
        ret = allocator->Alloc(allocReq, blobs);
        EXPECT_EQ(ret, MMC_OK);

    }
    EXPECT_EQ(blobs.size(), 10U);
    for (size_t i = 0; i < blobs.size(); i++) {
        EXPECT_EQ(blobs[i]->Rank(), 0);
        EXPECT_EQ(blobs[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs[i]->Gva(), i * allocReq.blobSize_);
    }

    for (size_t i = 0; i < blobs.size(); i++) {
        ret = allocator->Free(blobs[i]);
        EXPECT_EQ(ret, MMC_OK);
    }
    allocator->Stop(loc);

    blobMap["1"] = blobs[1]->GetDesc();
    blobMap["4"] = blobs[4]->GetDesc();
    blobMap["7"] = blobs[7]->GetDesc();

    blobs.erase(blobs.begin() + 7);
    blobs.erase(blobs.begin() + 4);
    blobs.erase(blobs.begin() + 1);

    ret = allocator->BuildFromBlobs(loc, blobMap);
    EXPECT_EQ(ret, MMC_OK);
    allocator->Start(loc);

    std::vector<MmcMemBlobPtr> blobsCompare;
    for (size_t i = 0; i < 7; i++) {
        ret = allocator->Alloc(allocReq, blobsCompare);
        EXPECT_EQ(ret, MMC_OK);
    }
    EXPECT_EQ(blobs.size(), 7U);
    for (size_t i = 0; i < blobs.size(); i++) {
        EXPECT_EQ(blobs[i]->Rank(), 0);
        EXPECT_EQ(blobs[i]->Size(), blobsCompare[i]->Size());
        EXPECT_EQ(blobs[i]->Type(), blobsCompare[i]->Type());
        EXPECT_EQ(blobs[i]->Gva(), blobsCompare[i]->Gva());
    }

}

TEST_F(TestMmcGlobalAllocator, AllocForce)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    uint64_t size = SIZE_32K * 10;
    for (int i = 0; i < 10; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_DRAM;
        loc.rank_ = i;
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 10;
    allocReq.preferredRank_.push_back(5);
    allocReq.mediaType_ = 0;
    allocReq.flags_ = ALLOC_FORCE_BY_RANK;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_ERROR);
    EXPECT_EQ(blobs.size(), 0U);

    allocReq.numBlobs_ = 1;
    allocReq.preferredRank_.clear();
    allocReq.preferredRank_.push_back(10);
    ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_ERROR);
    EXPECT_EQ(blobs.size(), 0U);

    allocReq.preferredRank_.clear();
    allocReq.preferredRank_.push_back(5);
    ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 1U);
    EXPECT_EQ(blobs[0]->Rank(), allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]);
    EXPECT_EQ(blobs[0]->Size(), allocReq.blobSize_);
    EXPECT_EQ(blobs[0]->Type(), allocReq.mediaType_);
    EXPECT_EQ(blobs[0]->Gva(), size * (allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]));
}

TEST_F(TestMmcGlobalAllocator, AllocRandom)
{
    MmcGlobalAllocatorPtr allocator = MmcMakeRef<MmcGlobalAllocator>();
    uint64_t size = SIZE_32K * 10;
    for (int i = 0; i < 10; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_DRAM;
        loc.rank_ = i;
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }
    for (int i = 0; i < 5; i++) {
        MmcLocation loc;
        MmcLocalMemlInitInfo info;
        loc.mediaType_ = MEDIA_HBM;
        loc.rank_ = i;
        info.bmAddr_ = size * i;
        info.capacity_ = size;
        std::map<std::string, MmcMemBlobDesc> blobMap;
        allocator->Mount(loc, info);
        allocator->BuildFromBlobs(loc, blobMap);
        allocator->Start(loc);
    }

    AllocOptions allocReq;
    std::vector<MmcMemBlobPtr> blobs;

    allocReq.blobSize_ = SIZE_32K;
    allocReq.numBlobs_ = 10;
    allocReq.preferredRank_.push_back(0);
    allocReq.mediaType_ = 1;
    allocReq.flags_ = ALLOC_RANDOM;

    Result ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_ERROR);
    EXPECT_EQ(blobs.size(), 0U);

    allocReq.mediaType_ = 0;

    ret = allocator->Alloc(allocReq, blobs);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs.size(), 10U);
    for (size_t i = 0; i < blobs.size(); i++) {
        cout << "blob[" << i << "] rank: " << blobs[i]->Rank() << endl;
        EXPECT_EQ(blobs[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs[i]->Gva(), size * blobs[i]->Rank());
    }

    std::vector<MmcMemBlobPtr> blobs1;
    ret = allocator->Alloc(allocReq, blobs1);
    EXPECT_EQ(ret, MMC_OK);
    EXPECT_EQ(blobs1.size(), 10U);
    bool isEqual = true;
    for (size_t i = 0; i < blobs1.size(); i++) {
        cout << "blob[" << i << "] rank: " << blobs1[i]->Rank() << endl;
        if (blobs[i] != blobs1[i]) {
            isEqual = false;
        }
        EXPECT_EQ(blobs1[i]->Size(), allocReq.blobSize_);
        EXPECT_EQ(blobs1[i]->Type(), allocReq.mediaType_);
        EXPECT_EQ(blobs1[i]->Gva(), size * blobs1[i]->Rank() + SIZE_32K);
    }
    EXPECT_FALSE(isEqual);
}