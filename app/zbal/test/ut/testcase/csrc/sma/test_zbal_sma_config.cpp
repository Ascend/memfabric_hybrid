/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <limits>

#define private   public
#define protected public
#include "zbal_sma_config.h"
#undef private
#undef protected

using namespace zbal::sma;

/* ================================================================
 * lexArgs — tokenization
 * ================================================================ */

class TestSMAConfigLexer : public testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}

    SMAConfig config_;
};

TEST_F(TestSMAConfigLexer, EmptyStringEmpty)
{
    std::vector<std::string> config;
    config_.lexArgs("", config);
    EXPECT_TRUE(config.empty());
}

TEST_F(TestSMAConfigLexer, SimpleKeyValue)
{
    std::vector<std::string> config;
    config_.lexArgs("max_split_size_mb:100", config);
    ASSERT_EQ(config.size(), 3u);
    EXPECT_EQ(config[0], "max_split_size_mb");
    EXPECT_EQ(config[1], ":");
    EXPECT_EQ(config[2], "100");
}

TEST_F(TestSMAConfigLexer, MultipleOptions)
{
    std::vector<std::string> config;
    config_.lexArgs("max_split_size_mb:100,garbage_collection_threshold:0.5", config);
    ASSERT_EQ(config.size(), 7u);
    EXPECT_EQ(config[0], "max_split_size_mb");
    EXPECT_EQ(config[1], ":");
    EXPECT_EQ(config[2], "100");
    EXPECT_EQ(config[3], ",");
    EXPECT_EQ(config[4], "garbage_collection_threshold");
    EXPECT_EQ(config[5], ":");
    EXPECT_EQ(config[6], "0.5");
}

TEST_F(TestSMAConfigLexer, WhitespaceSkipped)
{
    std::vector<std::string> config;
    config_.lexArgs("  max_split_size_mb : 100  ,  use_sma_allocator : True  ", config);
    ASSERT_EQ(config.size(), 7u);
    EXPECT_EQ(config[0], "max_split_size_mb");
    EXPECT_EQ(config[1], ":");
    EXPECT_EQ(config[2], "100");
    EXPECT_EQ(config[3], ",");
    EXPECT_EQ(config[4], "use_sma_allocator");
    EXPECT_EQ(config[5], ":");
    EXPECT_EQ(config[6], "True");
}

TEST_F(TestSMAConfigLexer, BracketedValues)
{
    std::vector<std::string> config;
    config_.lexArgs("small_heap_size:[1024]", config);
    ASSERT_EQ(config.size(), 5u);
    EXPECT_EQ(config[2], "[");
    EXPECT_EQ(config[3], "1024");
    EXPECT_EQ(config[4], "]");
}

/* ================================================================
 * consumeToken
 * ================================================================ */

TEST_F(TestSMAConfigLexer, ConsumeTokenValid)
{
    // consumeToken doesn't throw on valid tokens
    std::vector<std::string> config = {"key", ":", "val", ",", "next"};
    EXPECT_NO_THROW(config_.consumeToken(config, 1, ':'));
    EXPECT_NO_THROW(config_.consumeToken(config, 3, ','));
}

/* ================================================================
 * Default values (no env set, no env var)
 * ================================================================ */

class TestSMAConfigDefaults : public testing::Test {
public:
    static void SetUpTestCase()
    {
        ::unsetenv("ZBAL_NPU_ALLOC_CONF");
    }

    void SetUp() override
    {
        // Create a fresh config and parse empty env
        config_ = new SMAConfig();
        config_->parseEnv(nullptr);
    }

    void TearDown() override
    {
        delete config_;
        config_ = nullptr;
    }

    SMAConfig *config_;
};

TEST_F(TestSMAConfigDefaults, MaxSplitSizeIsMax)
{
    EXPECT_EQ(config_->max_split_size_, std::numeric_limits<size_t>::max());
}

TEST_F(TestSMAConfigDefaults, GCThresholdIsZero)
{
    EXPECT_EQ(config_->garbage_collection_threshold_, 0.0);
}

TEST_F(TestSMAConfigDefaults, UseSMAAllocatorTrue)
{
    EXPECT_TRUE(config_->use_sma_allocator_);
}

TEST_F(TestSMAConfigDefaults, UseVMMForStaticMemoryFalse)
{
    EXPECT_FALSE(config_->use_vmm_for_static_memory_);
}

/* ================================================================
 * Parsing a valid configuration string
 * ================================================================ */

class TestSMAConfigParse : public testing::Test {
public:
    void SetUp() override
    {
        config_ = new SMAConfig();
    }

    void TearDown() override
    {
        delete config_;
        config_ = nullptr;
    }

    SMAConfig *config_;
};

TEST_F(TestSMAConfigParse, MaxSplitSizeValid)
{
    config_->parseEnv("max_split_size_mb:100");
    EXPECT_EQ(config_->max_split_size_, 100u * zbal::sma::kMB);
}

TEST_F(TestSMAConfigParse, MaxSplitSizeClamped)
{
    // values below kLargeBuffer/kMB are clamped up
    config_->parseEnv("max_split_size_mb:25");
    EXPECT_GE(config_->max_split_size_, zbal::sma::kLargeBuffer);
}

TEST_F(TestSMAConfigParse, GarbageCollectionThreshold)
{
    config_->parseEnv("garbage_collection_threshold:0.75");
    EXPECT_DOUBLE_EQ(config_->garbage_collection_threshold_, 0.75);
}

TEST_F(TestSMAConfigParse, UseSMAAllocatorFalse)
{
    config_->parseEnv("use_sma_allocator:False");
    EXPECT_FALSE(config_->use_sma_allocator_);
}

TEST_F(TestSMAConfigParse, UseSMAAllocatorTrue)
{
    config_->parseEnv("use_sma_allocator:True");
    EXPECT_TRUE(config_->use_sma_allocator_);
}

TEST_F(TestSMAConfigParse, UseVMMForStaticMemoryTrue)
{
    config_->parseEnv("use_vmm_for_static_memory:True");
    EXPECT_TRUE(config_->use_vmm_for_static_memory_);
}

TEST_F(TestSMAConfigParse, SmallHeapSize)
{
    config_->parseEnv("small_heap_size:2048");
    EXPECT_EQ(config_->small_heap_size_, 2048u);
}

TEST_F(TestSMAConfigParse, SmallHeapThreshold)
{
    config_->parseEnv("small_heap_threshold:512");
    EXPECT_EQ(config_->small_heap_threshold_, 512u);
}

TEST_F(TestSMAConfigParse, SegmentSizeMb)
{
    config_->parseEnv("segment_size_mb:64");
    EXPECT_EQ(config_->segment_size_mb_, 64u * zbal::sma::kMB);
}

TEST_F(TestSMAConfigParse, MultipleOptionsCommaSeparated)
{
    config_->parseEnv("max_split_size_mb:200,use_sma_allocator:False,"
                      "garbage_collection_threshold:0.25,small_heap_size:1024,"
                      "use_vmm_for_static_memory:True,segment_size_mb:32,"
                      "small_heap_threshold:256");

    EXPECT_EQ(config_->max_split_size_, 200u * zbal::sma::kMB);
    EXPECT_FALSE(config_->use_sma_allocator_);
    EXPECT_DOUBLE_EQ(config_->garbage_collection_threshold_, 0.25);
    EXPECT_EQ(config_->small_heap_size_, 1024u);
    EXPECT_TRUE(config_->use_vmm_for_static_memory_);
    EXPECT_EQ(config_->segment_size_mb_, 32u * zbal::sma::kMB);
    EXPECT_EQ(config_->small_heap_threshold_, 256u);
}

/* ================================================================
 * Boundary value tests
 * ================================================================ */

TEST_F(TestSMAConfigParse, MaxSplitSizeAtMax)
{
    config_->parseEnv("max_split_size_mb:100000000");
    EXPECT_GT(config_->max_split_size_, 0u);
}

TEST_F(TestSMAConfigParse, GCThresholdAtBoundary)
{
    config_->parseEnv("garbage_collection_threshold:0.01");
    EXPECT_GT(config_->garbage_collection_threshold_, 0.0);
}

TEST_F(TestSMAConfigParse, GCThresholdNearOne)
{
    config_->parseEnv("garbage_collection_threshold:0.99");
    EXPECT_LT(config_->garbage_collection_threshold_, 1.0);
}

/* ================================================================
 * SMAConfig static accessors (on default singleton)
 * ================================================================ */

class TestSMAConfigStaticAccessors : public testing::Test {
public:
    static void SetUpTestCase()
    {
        // Set a known env before the singleton is first accessed
        ::setenv("ZBAL_NPU_ALLOC_CONF",
                 "max_split_size_mb:500,use_sma_allocator:True,"
                 "garbage_collection_threshold:0.5,small_heap_size:256,"
                 "segment_size_mb:64,small_heap_threshold:128,"
                 "use_vmm_for_static_memory:False",
                 1);
    }

    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestSMAConfigStaticAccessors, MaxSplitSize)
{
    EXPECT_EQ(SMAConfig::max_split_size(), 500u * kMB);
}

TEST_F(TestSMAConfigStaticAccessors, GCThreshold)
{
    EXPECT_DOUBLE_EQ(SMAConfig::garbage_collection_threshold(), 0.5);
}

TEST_F(TestSMAConfigStaticAccessors, UseSMAAllocator)
{
    EXPECT_TRUE(SMAConfig::use_sma_allocator());
}

TEST_F(TestSMAConfigStaticAccessors, UseVMMForStaticMemory)
{
    EXPECT_FALSE(SMAConfig::use_vmm_for_static_memory());
}

TEST_F(TestSMAConfigStaticAccessors, SmallHeapSize)
{
    EXPECT_EQ(SMAConfig::small_heap_size(), 256u);
}

TEST_F(TestSMAConfigStaticAccessors, SmallHeapThreshold)
{
    EXPECT_EQ(SMAConfig::small_heap_threshold(), 128u);
}

TEST_F(TestSMAConfigStaticAccessors, SegmentSizeMb)
{
    EXPECT_EQ(SMAConfig::segment_size_mb(), 64u * kMB);
}
