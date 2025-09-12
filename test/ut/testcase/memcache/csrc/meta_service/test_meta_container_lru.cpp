/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc_meta_container_lru.cpp"
#include "mmc_mem_obj_meta.h"
#include "mmc_meta_container.h"
#include "gtest/gtest.h"
#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>

using namespace ock::mmc;
using namespace testing;

class TestMmcMetaContainerLRU : public Test {
protected:
    using Container = MmcMetaContainerLRU<std::string, int>;
    std::unique_ptr<Container> container;
    
    void SetUp() override
    {
        container = std::make_unique<Container>();
    }
};

TEST_F(TestMmcMetaContainerLRU, InsertAndGet)
{
    EXPECT_EQ(container->Insert("key1", 100), MMC_OK);
    EXPECT_EQ(container->Insert("key2", 200), MMC_OK);
    EXPECT_EQ(container->Insert("key1", 300), MMC_DUPLICATED_OBJECT);

    int value;
    EXPECT_EQ(container->Get("key1", value), MMC_OK);
    EXPECT_EQ(value, 100);
    EXPECT_EQ(container->Get("key2", value), MMC_OK);
    EXPECT_EQ(value, 200);
    EXPECT_EQ(container->Get("key3", value), MMC_UNMATCHED_KEY);
}

TEST_F(TestMmcMetaContainerLRU, Erase)
{
    container->Insert("key1", 100);
    container->Insert("key2", 200);

    EXPECT_EQ(container->Erase("key1"), MMC_OK);

    int value;
    EXPECT_EQ(container->Get("key1", value), MMC_UNMATCHED_KEY);
    EXPECT_EQ(container->Erase("key3"), MMC_UNMATCHED_KEY);
    EXPECT_EQ(container->Get("key2", value), MMC_OK);
    EXPECT_EQ(value, 200);
}

TEST_F(TestMmcMetaContainerLRU, PromoteAndEvictOrder)
{
    container->Insert("key1", 100);
    container->Insert("key2", 200);
    container->Insert("key3", 300);
    
    EXPECT_EQ(container->Promote("key2"), MMC_OK);
   
    auto candidates = container->EvictCandidates(100, 66);
    ASSERT_EQ(candidates.size(), 1u);
    EXPECT_EQ(candidates[0], "key1");
    candidates = container->EvictCandidates(100, 33);
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0], "key1");
    EXPECT_EQ(candidates[1], "key3");
    EXPECT_EQ(container->Promote("key4"), MMC_UNMATCHED_KEY);
}
