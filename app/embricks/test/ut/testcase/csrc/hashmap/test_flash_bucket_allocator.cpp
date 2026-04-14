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
#include "emb_env_helper.h"
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

TEST_F(TestFlashBucketAllocator, FlashBucketMemPoolInitialize)
{
    auto &pool = FlashBucketMemPool::Instance();
    auto result = pool.Initialize();
    EXPECT_TRUE(result == EM_OK);

    void *tmp6TB = 0x60000000000;
    EXPECT_TRUE(reinterpret_cast<uintptr_t>(tmp6TB) == pool.StartAddress());

    pool.UnInitialize();

    EnvHelper::gHashmapOverflowBucketPoolStartAddrTB = 0;
    result = pool.Initialize();
    EXPECT_TRUE(result != EM_OK);

    EnvHelper::gHashmapOverflowBucketPoolStartAddrTB = 128L;
    result = pool.Initialize();
    EXPECT_TRUE(result != EM_OK);

    EnvHelper::gHashmapOverflowBucketPoolStartAddrTB = UN1;
    result = pool.Initialize();
    EXPECT_TRUE(result == EM_OK);
    pool.UnInitialize();

    EnvHelper::gHashmapOverflowBucketPoolStartAddrTB = 127L;
    result = pool.Initialize();
    EXPECT_TRUE(result == EM_OK);
    pool.UnInitialize();

    EnvHelper::gHashmapOverflowBucketPoolStartAddrTB = UN6;
    EnvHelper::gHashmapOverflowBucketPoolSizeGB = 0;
    result = pool.Initialize();
    EXPECT_TRUE(result != EM_OK);

    EnvHelper::gHashmapOverflowBucketPoolSizeGB = 65L;
    result = pool.Initialize();
    EXPECT_TRUE(result != EM_OK);

    EnvHelper::gHashmapOverflowBucketPoolSizeGB = UN1;
    result = pool.Initialize();
    EXPECT_TRUE(result == EM_OK);
    pool.UnInitialize();

    EnvHelper::gHashmapOverflowBucketPoolSizeGB = UN64;
    result = pool.Initialize();
    EXPECT_TRUE(result == EM_OK);
    pool.UnInitialize();

    EnvHelper::gHashmapOverflowBucketPoolSizeGB = UN1;
}

TEST_F(TestFlashBucketAllocator, FlashBucketMemPoolAllocate)
{
    EnvHelper::gHashmapOverflowBucketPoolSizeGB = UN1;

    auto &pool = FlashBucketMemPool::Instance();
    auto result = pool.Initialize();
    EXPECT_TRUE(result == EM_OK);

    uintptr_t allocated = 0;
    uint32_t count = 0;
    while (pool.Allocate2MB(allocated) == EM_OK) {
        EXPECT_TRUE(allocated == (pool.StartAddress() + count * kMemBlockSize));
        ++count;
    }
    EXPECT_TRUE(count == 512L);

    EM_LOG_DEBUG(pool);
}
