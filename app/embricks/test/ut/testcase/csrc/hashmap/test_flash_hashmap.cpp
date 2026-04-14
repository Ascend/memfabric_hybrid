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
#include "emb_flash_hashmap.h"

using namespace ock::emb;
using namespace ock::emb::hashmap;

class TestFlashHashmap : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(TestFlashHashmap, StructSize)
{
    EM_LOG_DEBUG("size of BucketSpinLock: " << sizeof(BucketSpinLock) << ", size of HashBucket: " << sizeof(HashBucket)
                                            << ", size of FlashHashmap: "
                                            << sizeof(FlashHashmap<NaiveBucketAllocator>));

    EXPECT_TRUE(sizeof(BucketSpinLock) == UN8);
    EXPECT_TRUE(sizeof(FlashHashmap<NaiveBucketAllocator>) == UN64);
    EXPECT_TRUE(sizeof(HashBucket) == UN64);
}

TEST_F(TestFlashHashmap, Initialize)
{
    FlashHashmapPtr flashMap = EmMakeRef<FlashHashmap<NaiveBucketAllocator>>();
    auto result = flashMap->Initialize(UN3);
    EXPECT_TRUE(result == EM_OK);
    EXPECT_TRUE(flashMap->BucketCount() == UN5);

    flashMap = EmMakeRef<FlashHashmap<NaiveBucketAllocator>>();
    result = flashMap->Initialize(10000L);
    EXPECT_TRUE(result == EM_OK);
    EXPECT_TRUE(flashMap->BucketCount() == 2029L);
}

TEST_F(TestFlashHashmap, BucketInsertAndGet)
{
    EM_LOG_DEBUG("BucketInsertAndGet");

    HashBucket bucket;
    EXPECT_TRUE(bucket.key_[UN0] == kInvalidMapKey);
    EXPECT_TRUE(bucket.key_[UN1] == kInvalidMapKey);
    EXPECT_TRUE(bucket.key_[UN2] == kInvalidMapKey);
    EXPECT_TRUE(bucket.value_[UN0] == kInvalidMapValue);
    EXPECT_TRUE(bucket.value_[UN1] == kInvalidMapValue);
    EXPECT_TRUE(bucket.value_[UN2] == kInvalidMapValue);

    EXPECT_TRUE(bucket.next_ == nullptr);

    bucket.Insert(UN0, UN1);
    bucket.Insert(UN1, UN2);
    bucket.Insert(UN9, UN3);
    EXPECT_TRUE(bucket.key_[UN0] == UN0);
    EXPECT_TRUE(bucket.key_[UN1] == UN1);
    EXPECT_TRUE(bucket.key_[UN2] == UN9);

    EXPECT_TRUE(bucket.Insert(11L, UN9) == EM_HASHMAP_BUCKET_FULL);

    uint64_t value = kInvalidMapValue;
    EXPECT_TRUE(bucket.Get(UN1, value) == EM_OK && value == UN2);
    EXPECT_TRUE(bucket.Get(UN9, value) == EM_OK && value == UN3);
    EXPECT_TRUE(bucket.Get(UN0, value) == EM_OK && value == UN1);
}

TEST_F(TestFlashHashmap, BucketGetLastAndReplace)
{
    EM_LOG_DEBUG("BucketGetLastAndReplace");

    HashBucket buck1;
    uint64_t key = kInvalidMapKey;
    uint64_t value = kInvalidMapValue;

    /* no key value */
    EXPECT_TRUE(buck1.GetAndEraseLast(key, value) == EM_HASHMAP_KEY_NOT_FOUND);

    /* insert one and get one */
    buck1.Insert(UN0, UN1);
    EXPECT_TRUE(buck1.GetAndEraseLast(key, value) == EM_OK);
    EXPECT_TRUE(key == UN0 && value == UN1);
    EXPECT_TRUE(buck1.key_[UN0] == kInvalidMapKey && buck1.value_[UN0] == kInvalidMapKey);

    /* insert two and get one */
    buck1.key_[UN0] = kInvalidMapKey;
    buck1.key_[UN1] = kInvalidMapKey;
    buck1.key_[UN2] = kInvalidMapKey;
    buck1.Insert(UN0, UN1);
    buck1.Insert(UN1, UN2);
    EXPECT_TRUE(buck1.GetAndEraseLast(key, value) == EM_OK);
    EXPECT_TRUE(key == UN1 && value == UN2);
    EXPECT_TRUE(buck1.key_[UN0] == UN0 && buck1.value_[UN0] == UN1);
    EXPECT_TRUE(buck1.key_[UN1] == kInvalidMapKey && buck1.value_[UN1] == kInvalidMapKey);

    /* insert three and get one */
    buck1.key_[UN0] = kInvalidMapKey;
    buck1.key_[UN1] = kInvalidMapKey;
    buck1.key_[UN2] = kInvalidMapKey;
    buck1.Insert(UN0, UN1);
    buck1.Insert(UN1, UN2);
    buck1.Insert(UN2, UN3);
    EXPECT_TRUE(buck1.GetAndEraseLast(key, value) == EM_OK);
    EXPECT_TRUE(key == UN2 && value == UN3);
    EXPECT_TRUE(buck1.key_[UN0] == UN0 && buck1.value_[UN0] == UN1);
    EXPECT_TRUE(buck1.key_[UN1] == UN1 && buck1.value_[UN1] == UN2);
    EXPECT_TRUE(buck1.key_[UN2] == kInvalidMapKey && buck1.value_[UN2] == kInvalidMapKey);
}

TEST_F(TestFlashHashmap, BucketGetLastAndReplaceLinked)
{
    EM_LOG_DEBUG("BucketGetLastAndReplace");

    HashBucket buck1;
    HashBucket buck2;
    uint64_t key = kInvalidMapKey;
    uint64_t value = kInvalidMapValue;
    buck1.Insert(UN0, UN1);
    buck1.Insert(UN1, UN2);
    buck1.Insert(UN2, UN3);
    buck1.next_ = &buck2;

    EXPECT_TRUE(buck1.GetAndEraseLast(key, value) == EM_OK);
    EXPECT_TRUE(buck1.key_[UN2] == kInvalidMapKey && buck1.value_[UN2] == kInvalidMapValue);

    buck1.key_[UN0] = kInvalidMapKey;
    buck1.key_[UN1] = kInvalidMapKey;
    buck1.key_[UN2] = kInvalidMapKey;
    buck1.Insert(UN0, UN1);
    buck1.Insert(UN1, UN2);
    buck1.Insert(UN2, UN3);

    buck2.Insert(UN3, UN4);
    EXPECT_TRUE(buck1.GetAndEraseLast(key, value) == EM_OK);
    EXPECT_TRUE(key == UN3 && value == UN4);
    EXPECT_TRUE(buck1.key_[UN2] == UN2 && buck1.value_[UN2] == UN3);
    EXPECT_TRUE(buck2.key_[UN0] == kInvalidMapKey && buck2.value_[UN0] == kInvalidMapKey);

    EXPECT_TRUE(buck1.Replace(UN6, key, value) == EM_HASHMAP_KEY_NOT_FOUND);
    EXPECT_TRUE(buck1.Replace(UN0, key, value) == EM_OK);
    EXPECT_TRUE(buck1.key_[UN0] == UN3 && buck1.value_[UN0] == UN4);
    EXPECT_TRUE(buck1.Replace(UN1, key, value) == EM_OK);
    EXPECT_TRUE(buck1.key_[UN1] == UN3 && buck1.value_[UN1] == UN4);
    EXPECT_TRUE(buck1.Replace(UN2, key, value) == EM_OK);
    EXPECT_TRUE(buck1.key_[UN2] == UN3 && buck1.value_[UN2] == UN4);
}

class NaiveBucketAllocatorWithPrint {
public:
    void *Allocate(uint64_t size) noexcept
    {
        EM_LOG_DEBUG("Allocate " << size);
        return calloc(UN1, size);
    }

    void Free(void *p) noexcept
    {
        if (LIKELY(p != nullptr)) {
            free(p);
            p = nullptr;
        }
    }
};

TEST_F(TestFlashHashmap, HashMapFindOrInsert)
{
    auto hashmap = EmMakeRef<FlashHashmap<NaiveBucketAllocatorWithPrint>>();
    auto result = hashmap->Initialize(UN1);
    EXPECT_TRUE(result == EM_OK);

    uint64_t value = UN5;
    hashmap->FindOrInsert(UN5, value);
    hashmap->FindOrInsert(10L, value);
    hashmap->FindOrInsert(15L, value);
    hashmap->FindOrInsert(20L, value);
    hashmap->FindOrInsert(25L, value);
    hashmap->FindOrInsert(30L, value);
    value = 15L;
    hashmap->FindOrInsert(35L, value);

    EXPECT_TRUE(hashmap->Size() == UN7);

    value = kInvalidMapValue;
    EXPECT_TRUE(hashmap->Remove(35L, value) == EM_OK && value == 15L);
    EXPECT_TRUE(hashmap->Size() == UN6);

    EXPECT_TRUE(hashmap->Remove(32L, value) == EM_HASHMAP_KEY_NOT_FOUND);

    EXPECT_TRUE(hashmap->Find(UN4, value) == EM_HASHMAP_KEY_NOT_FOUND);
    EXPECT_TRUE(hashmap->Find(10L, value) == EM_OK && value == UN5);
    EXPECT_TRUE(hashmap->Find(25L, value) == EM_OK && value == UN5);
}