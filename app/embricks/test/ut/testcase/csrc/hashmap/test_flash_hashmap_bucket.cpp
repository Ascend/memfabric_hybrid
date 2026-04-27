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

#include "emb_flash_hashmap_bucket.h"

using namespace ock::emb;
using namespace ock::emb::hashmap;

class TestFlashHashmapBucket : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(TestFlashHashmapBucket, TestFlashHashBucketSize)
{
    EM_LOG_DEBUG("size of FlashHashBucket: " << sizeof(FlashHashBucket));

    EXPECT_TRUE(sizeof(FlashHashBucket) == UN64);

    return;
}

TEST_F(TestFlashHashmapBucket, TestFlashHashBucketConstructor)
{
    FlashHashBucket bucket;
    EXPECT_TRUE(bucket.key[UN0] == INVALID_MAP_KEY);
    EXPECT_TRUE(bucket.key[UN1] == INVALID_MAP_KEY);
    EXPECT_TRUE(bucket.key[UN2] == INVALID_MAP_KEY);
    EXPECT_TRUE(bucket.value[UN0] == INVALID_MAP_VALUE);
    EXPECT_TRUE(bucket.value[UN1] == INVALID_MAP_VALUE);
    EXPECT_TRUE(bucket.value[UN2] == INVALID_MAP_VALUE);

    EXPECT_TRUE(bucket.bucketBaseAddress == 0);
    EXPECT_TRUE(bucket.hasNext == 0);
    EXPECT_TRUE(bucket.HasNextWithoutLock() == 0);
    EXPECT_TRUE(bucket.offset2BaseAddress == 0);
    EXPECT_TRUE(bucket.lock == 0);

    EM_LOG_DEBUG(bucket);

    bzero(&bucket, sizeof(FlashHashBucket));

    EXPECT_TRUE(bucket.HasNextWithoutLock() == false);

    uint64_t value = INVALID_MAP_VALUE;
    EXPECT_TRUE(bucket.Get(UN1, value) == EM_HASHMAP_KEY_NOT_FOUND);

    return;
}

TEST_F(TestFlashHashmapBucket, BucketInsertAndGet)
{
    EM_LOG_DEBUG("BucketInsertAndGet");

    FlashHashBucket bucket;
    EXPECT_TRUE(bucket.key[UN0] == INVALID_MAP_KEY);
    EXPECT_TRUE(bucket.key[UN1] == INVALID_MAP_KEY);
    EXPECT_TRUE(bucket.key[UN2] == INVALID_MAP_KEY);
    EXPECT_TRUE(bucket.value[UN0] == INVALID_MAP_VALUE);
    EXPECT_TRUE(bucket.value[UN1] == INVALID_MAP_VALUE);
    EXPECT_TRUE(bucket.value[UN2] == INVALID_MAP_VALUE);

    EXPECT_TRUE(bucket.HasNextWithoutLock() == false);

    EXPECT_TRUE(bucket.Insert(INVALID_MAP_KEY, UN1) == EM_HASHMAP_INVALID_KEY);
    /* will be */
    EXPECT_TRUE(bucket.Insert(UN1, UN2) == EM_OK);
    EXPECT_TRUE(bucket.Insert(UN2, UN3) == EM_OK);
    EXPECT_TRUE(bucket.Insert(UN3, UN4) == EM_OK);
    /* verify keys */
    EXPECT_TRUE(bucket.key[UN0] == UN1);
    EXPECT_TRUE(bucket.key[UN1] == UN2);
    EXPECT_TRUE(bucket.key[UN2] == UN3);

    EM_LOG_DEBUG(bucket);

    EXPECT_TRUE(bucket.Insert(11L, UN9) == EM_HASHMAP_BUCKET_FULL);

    uint64_t value = INVALID_MAP_VALUE;
    EXPECT_TRUE(bucket.Get(INVALID_MAP_KEY, value) == EM_HASHMAP_INVALID_KEY);

    EXPECT_TRUE(bucket.Get(UN1, value) == EM_OK && value == UN2);
    EXPECT_TRUE(bucket.Get(UN2, value) == EM_OK && value == UN3);
    EXPECT_TRUE(bucket.Get(UN3, value) == EM_OK && value == UN4);
}

TEST_F(TestFlashHashmapBucket, BucketGetOrInsert)
{
    FlashHashBucket bucket;

    uint64_t value = INVALID_MAP_VALUE;
    uintptr_t valueAddress = 0;

    /* insert invalid key */
    EXPECT_TRUE(bucket.GetOrInsert(INVALID_MAP_KEY, value, valueAddress) == EM_HASHMAP_INVALID_KEY);

    EXPECT_TRUE(bucket.GetOrInsert(UN1, value, valueAddress) == EM_HASHMAP_NOT_FOUND_BUT_INSERTED);
    EXPECT_TRUE(reinterpret_cast<uintptr_t>(&(bucket.value[UN0])) == valueAddress);
    *(reinterpret_cast<uint64_t *>(valueAddress)) = UN1024;
    EXPECT_TRUE(bucket.GetOrInsert(UN1, value, valueAddress) == EM_OK && value == UN1024);

    EXPECT_TRUE(bucket.GetOrInsert(UN2, value, valueAddress) == EM_HASHMAP_NOT_FOUND_BUT_INSERTED);
    EXPECT_TRUE(reinterpret_cast<uintptr_t>(&(bucket.value[UN1])) == valueAddress);
    *(reinterpret_cast<uint64_t *>(valueAddress)) = UN16;
    EXPECT_TRUE(bucket.GetOrInsert(UN2, value, valueAddress) == EM_OK && value == UN16);

    EXPECT_TRUE(bucket.GetOrInsert(UN3, value, valueAddress) == EM_HASHMAP_NOT_FOUND_BUT_INSERTED);
    EXPECT_TRUE(reinterpret_cast<uintptr_t>(&(bucket.value[UN2])) == valueAddress);
    *(reinterpret_cast<uint64_t *>(valueAddress)) = UN64;
    EXPECT_TRUE(bucket.GetOrInsert(UN3, value, valueAddress) == EM_OK && value == UN64);

    EM_LOG_DEBUG(bucket);
}

TEST_F(TestFlashHashmapBucket, BucketGetWithoutLock)
{
    EM_LOG_DEBUG("BucketGetWithoutLock");

    FlashHashBucket bucket;

    EXPECT_TRUE(bucket.HasNextWithoutLock() == false);

    EXPECT_TRUE(bucket.Insert(INVALID_MAP_KEY, UN1) == EM_HASHMAP_INVALID_KEY);
    /* will be */
    EXPECT_TRUE(bucket.Insert(UN1, UN2) == EM_OK);
    EXPECT_TRUE(bucket.Insert(UN2, UN3) == EM_OK);
    EXPECT_TRUE(bucket.Insert(UN3, UN4) == EM_OK);

    EXPECT_TRUE(bucket.Insert(11L, UN9) == EM_HASHMAP_BUCKET_FULL);

    uint64_t value = INVALID_MAP_VALUE;

    EXPECT_TRUE(bucket.Get(INVALID_MAP_KEY, value) == EM_HASHMAP_INVALID_KEY);

    EXPECT_TRUE(bucket.GetWithoutLock(UN1, value) == EM_OK && value == UN2);
    EXPECT_TRUE(bucket.GetWithoutLock(UN2, value) == EM_OK && value == UN3);
    EXPECT_TRUE(bucket.GetWithoutLock(UN3, value) == EM_OK && value == UN4);
}

TEST_F(TestFlashHashmapBucket, BucketGetAndEraseLast)
{
    EM_LOG_DEBUG("BucketGetAndEraseLast");

    FlashHashBucket buck;
    uint64_t key = INVALID_MAP_KEY;
    uint64_t value = INVALID_MAP_VALUE;

    /* no key value */
    EXPECT_TRUE(buck.GetAndEraseLast(key, value) == EM_HASHMAP_KEY_NOT_FOUND);

    /* insert one and get one */
    buck.Insert(UN1, UN2);
    EXPECT_TRUE(buck.GetAndEraseLast(key, value) == EM_OK);
    EXPECT_TRUE(key == UN1 && value == UN2);
    EXPECT_TRUE(buck.key[UN0] == INVALID_MAP_KEY && buck.value[UN0] == INVALID_MAP_KEY);

    /* insert two and get one */
    buck.key[UN0] = INVALID_MAP_KEY;
    buck.key[UN1] = INVALID_MAP_KEY;
    buck.key[UN2] = INVALID_MAP_KEY;
    buck.Insert(UN1, UN2);
    buck.Insert(UN2, UN3);
    EXPECT_TRUE(buck.GetAndEraseLast(key, value) == EM_OK);
    EXPECT_TRUE(key == UN2 && value == UN3);
    EXPECT_TRUE(buck.key[UN0] == UN1 && buck.value[UN0] == UN2);
    EXPECT_TRUE(buck.key[UN1] == INVALID_MAP_KEY && buck.value[UN1] == INVALID_MAP_KEY);

    /* insert three and get one */
    buck.key[UN0] = INVALID_MAP_KEY;
    buck.key[UN1] = INVALID_MAP_KEY;
    buck.key[UN2] = INVALID_MAP_KEY;
    buck.Insert(UN1, UN2);
    buck.Insert(UN2, UN3);
    buck.Insert(UN3, UN4);
    EXPECT_TRUE(buck.GetAndEraseLast(key, value) == EM_OK);
    EXPECT_TRUE(key == UN3 && value == UN4);
    EXPECT_TRUE(buck.key[UN0] == UN1 && buck.value[UN0] == UN2);
    EXPECT_TRUE(buck.key[UN1] == UN2 && buck.value[UN1] == UN3);
    EXPECT_TRUE(buck.key[UN2] == INVALID_MAP_KEY && buck.value[UN2] == INVALID_MAP_KEY);
}

TEST_F(TestFlashHashmapBucket, BucketLinkBucketAndGetLast)
{
    EM_LOG_DEBUG("BucketLinkBucket");

    FlashHashBucket buck;
    uint64_t key = INVALID_MAP_KEY;
    uint64_t value = INVALID_MAP_VALUE;

    buck.Insert(UN1, UN2);
    buck.Insert(UN2, UN3);
    buck.Insert(UN3, UN4);

    /* link invalid one */
    EXPECT_TRUE(buck.LinkNextWithoutLock(0, 0) == EM_INVALID_PARAM);

    /* create overflowed bucket */
    FlashHashBucket overflowBuckets[UN2] = {};
    /* setup base address */
    uint64_t baseAddress = reinterpret_cast<uint64_t>(&overflowBuckets[UN0]);

    /* link one */
    EXPECT_TRUE(buck.LinkNextWithoutLock(baseAddress, 0) == EM_OK);
    EXPECT_TRUE(buck.HasNextWithoutLock() == true);
    EXPECT_TRUE(buck.bucketBaseAddress == baseAddress);
    EXPECT_TRUE(buck.offset2BaseAddress == 0);

    auto buck1 = buck.Next();
    EXPECT_TRUE(buck1->HasNextWithoutLock() == false);
    buck1->Insert(UN4, UN5);
    buck1->Insert(UN5, UN6);
    buck1->Insert(UN6, UN7);

    /* link second one */
    EXPECT_TRUE(buck1->LinkNextWithoutLock(baseAddress, sizeof(FlashHashBucket)) == EM_OK);
    EXPECT_TRUE(buck1->HasNextWithoutLock() == true);
    EXPECT_TRUE(buck1->bucketBaseAddress == baseAddress);
    EXPECT_TRUE(buck1->offset2BaseAddress == sizeof(FlashHashBucket));

    auto buck2 = buck1->Next();
    EXPECT_TRUE(buck2->HasNextWithoutLock() == false);
    buck2->Insert(UN7, UN8);
    buck2->Insert(UN8, UN9);
    buck2->Insert(UN9, UN10);

    EM_LOG_DEBUG("buck0 " << buck << ", buck1 " << (*buck1) << ", buck2 " << (*buck2));

    EXPECT_TRUE(buck.GetAndEraseLast(key, value) == EM_OK);
    EXPECT_TRUE(key == UN9 && value == UN10);
    EXPECT_TRUE(buck2->key[UN2] == INVALID_MAP_KEY && buck2->value[UN2] == INVALID_MAP_VALUE);
    EXPECT_TRUE(buck2->key[UN1] != INVALID_MAP_KEY && buck2->value[UN1] != INVALID_MAP_VALUE);

    EXPECT_TRUE(buck.GetAndEraseLast(key, value) == EM_OK);
    EXPECT_TRUE(key == UN8 && value == UN9);
    EXPECT_TRUE(buck2->key[UN2] == INVALID_MAP_KEY && buck2->value[UN2] == INVALID_MAP_VALUE);
    EXPECT_TRUE(buck2->key[UN1] == INVALID_MAP_KEY && buck2->value[UN1] == INVALID_MAP_VALUE);
    EXPECT_TRUE(buck2->key[UN0] != INVALID_MAP_KEY && buck2->value[UN0] != INVALID_MAP_VALUE);

    EXPECT_TRUE(buck.GetAndEraseLast(key, value) == EM_OK);
    EXPECT_TRUE(key == UN7 && value == UN8);
    EXPECT_TRUE(buck2->key[UN2] == INVALID_MAP_KEY && buck2->value[UN2] == INVALID_MAP_VALUE);
    EXPECT_TRUE(buck2->key[UN1] == INVALID_MAP_KEY && buck2->value[UN1] == INVALID_MAP_VALUE);
    EXPECT_TRUE(buck2->key[UN0] == INVALID_MAP_KEY && buck2->value[UN0] == INVALID_MAP_VALUE);

    EXPECT_TRUE(buck.GetAndEraseLast(key, value) == EM_OK);
    EXPECT_TRUE(key == UN6 && value == UN7);
    EXPECT_TRUE(buck2->key[UN2] == INVALID_MAP_KEY && buck2->value[UN2] == INVALID_MAP_VALUE);
    EXPECT_TRUE(buck2->key[UN1] == INVALID_MAP_KEY && buck2->value[UN1] == INVALID_MAP_VALUE);
    EXPECT_TRUE(buck2->key[UN0] == INVALID_MAP_KEY && buck2->value[UN0] == INVALID_MAP_VALUE);

    EXPECT_TRUE(buck1->key[UN2] == INVALID_MAP_KEY && buck1->value[UN2] == INVALID_MAP_VALUE);
    EXPECT_TRUE(buck1->key[UN1] != INVALID_MAP_KEY && buck1->value[UN1] != INVALID_MAP_VALUE);

    EM_LOG_DEBUG("buck0 " << buck << ", buck1 " << (*buck1) << ", buck2 " << (*buck2));
}