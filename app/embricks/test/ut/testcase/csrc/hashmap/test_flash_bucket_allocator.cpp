/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Embricks is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <gtest/gtest.h>

#include "emb_common_includes.h"
#include "emb_flash_bucket_allocator.h"

using namespace ock::emb;
using namespace ock::emb::hashmap;

class TestFlashBucketAllocator : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(TestFlashBucketAllocator, Initialize)
{
    FlashBucketAllocator allocator;

    /* less than 16 */
    EnvHelper::gHashmapOverflowBucketAllocatorSizeMB = 15L;
    EXPECT_TRUE(allocator.Initialize() == EM_INVALID_PARAM);

    /* larger then 1024 */
    EnvHelper::gHashmapOverflowBucketAllocatorSizeMB = 1025L;
    EXPECT_TRUE(allocator.Initialize() == EM_INVALID_PARAM);

    /* not multiple times of 16 */
    EnvHelper::gHashmapOverflowBucketAllocatorSizeMB = 31L;
    EXPECT_TRUE(allocator.Initialize() == EM_INVALID_PARAM);

    /* low bound */
    EnvHelper::gHashmapOverflowBucketAllocatorSizeMB = UN16;
    EXPECT_TRUE(allocator.Initialize() == EM_OK);

    EXPECT_TRUE(allocator.Capacity() == 16777216L / BUCKET_MEM_SIZE);
    EXPECT_TRUE(allocator.Allocated() == 0);

    allocator.UnInitialize();

    /* upper bound */
    EnvHelper::gHashmapOverflowBucketAllocatorSizeMB = 1024L;
    EXPECT_TRUE(allocator.Initialize() == EM_OK);

    EXPECT_TRUE(allocator.Capacity() == 1073741824L / BUCKET_MEM_SIZE);
    EXPECT_TRUE(allocator.Allocated() == 0);

    allocator.UnInitialize();

    /* upper bound */
    EnvHelper::gHashmapOverflowBucketAllocatorSizeMB = 64L;
    EXPECT_TRUE(allocator.Initialize() == EM_OK);

    EXPECT_TRUE(allocator.Capacity() == 67108864L / BUCKET_MEM_SIZE);
    EXPECT_TRUE(allocator.Allocated() == 0);
}

TEST_F(TestFlashBucketAllocator, AllocateAndFree)
{
    FlashBucketAllocator allocator;

    uint32_t offset1 = 0;
    /* get invalid start address if not initialized */
    EXPECT_TRUE(allocator.StartAddress() == UN0);
    EXPECT_TRUE(allocator.Allocate(offset1) == EM_NOT_INITIALIZED);
    EXPECT_TRUE(allocator.Free(offset1) == EM_NOT_INITIALIZED);

    EnvHelper::gHashmapOverflowBucketAllocatorSizeMB = UN16;
    EXPECT_TRUE(allocator.Initialize() == EM_OK);

    auto baseAddress = allocator.StartAddress();
    EXPECT_TRUE(baseAddress != UN0);

    /* allocate 3 */
    uint32_t offset2 = 0;
    uint32_t offset3 = 0;
    EXPECT_TRUE(allocator.Allocate(offset1) == EM_OK && offset1 == (UN0 * BUCKET_MEM_SIZE));
    EXPECT_TRUE(allocator.Allocate(offset2) == EM_OK && offset2 == (UN1 * BUCKET_MEM_SIZE));
    EXPECT_TRUE(allocator.Allocate(offset3) == EM_OK && offset3 == (UN2 * BUCKET_MEM_SIZE));

    /* free one */
    EXPECT_TRUE(allocator.Free(offset1) == EM_OK);

    /* allocate fourth */
    uint32_t offset4 = 0;
    EXPECT_TRUE(allocator.Allocate(offset4) == EM_OK && offset4 == (UN0 * BUCKET_MEM_SIZE));
}

TEST_F(TestFlashBucketAllocator, AllocateAll)
{
    FlashBucketAllocator allocator;

    EnvHelper::gHashmapOverflowBucketAllocatorSizeMB = UN16;
    EXPECT_TRUE(allocator.Initialize() == EM_OK);

    /* allocate all */
    uint32_t offset = 0;
    auto capacity = allocator.Capacity();
    for (uint32_t i = 0; i < capacity; i++) {
        EXPECT_TRUE(allocator.Allocate(offset) == EM_OK && offset == (i * BUCKET_MEM_SIZE));
    }

    /* cannot allocate */
    EXPECT_TRUE(allocator.Allocate(offset) == EM_NO_MORE_SPACE);

    /* free one */
    EXPECT_TRUE(allocator.Free(UN0) == EM_OK);

    /* allocate one, to test the reverse search start position */
    EXPECT_TRUE(allocator.Allocate(offset) == EM_OK && offset == UN0);

    /* un-initialize */
    allocator.UnInitialize();

    EXPECT_TRUE(allocator.Capacity() == UN0);
    EXPECT_TRUE(allocator.StartAddress() == UN0);
}
