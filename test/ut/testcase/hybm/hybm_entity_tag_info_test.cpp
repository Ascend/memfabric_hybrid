/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#include <gtest/gtest.h>

#define private public

#include "hybm_entity_tag_info.h"

#undef private

namespace ock {
namespace mf {
class HybmEntityTagInfoTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        tagInfo_ = std::make_unique<HybmEntityTagInfo>();
        hybm_options opt{};
        opt.rankCount = 8;
        EXPECT_EQ(tagInfo_->TagInfoInit(opt), BM_OK);
    }

    void TearDown() override
    {
        tagInfo_.reset();
    }

    std::unique_ptr<HybmEntityTagInfo> tagInfo_;
};

// ========================
// Test Case 1: TagInfoInit
// ========================
TEST_F(HybmEntityTagInfoTest, TagInfoInit)
{
    hybm_options opt;
    opt.rankCount = 4;
    EXPECT_EQ(tagInfo_->TagInfoInit(opt), BM_OK);
    EXPECT_EQ(tagInfo_->options_.rankCount, 4);
}

// ========================
// Test Case 2: AddRankTag - Valid
// ========================
TEST_F(HybmEntityTagInfoTest, AddRankTag_Valid)
{
    EXPECT_EQ(tagInfo_->AddRankTag(0, "rank0"), BM_OK);
    EXPECT_EQ(tagInfo_->GetTagByRank(0), "rank0");
}

// ========================
// Test Case 3: AddRankTag - Invalid Tag (too long)
// ========================
TEST_F(HybmEntityTagInfoTest, AddRankTag_Invalid_TooLong)
{
    EXPECT_EQ(tagInfo_->AddRankTag(0, "a123456789012345678901234561234"), BM_INVALID_PARAM);
}

// ========================
// Test Case 4: AddRankTag - Invalid Tag (contains invalid chars)
// ========================
TEST_F(HybmEntityTagInfoTest, AddRankTag_Invalid_InvalidChars)
{
    EXPECT_EQ(tagInfo_->AddRankTag(0, "rank@0"), BM_INVALID_PARAM);
    EXPECT_EQ(tagInfo_->AddRankTag(0, "rank 0"), BM_INVALID_PARAM);
}

// ========================
// Test Case 5: RemoveRankTag
// ========================
TEST_F(HybmEntityTagInfoTest, RemoveRankTag)
{
    tagInfo_->AddRankTag(0, "rank0");
    EXPECT_EQ(tagInfo_->GetTagByRank(0), "rank0");

    EXPECT_EQ(tagInfo_->RemoveRankTag(0, "rank0"), BM_OK);
    EXPECT_EQ(tagInfo_->GetTagByRank(0), "");
}

// ========================
// Test Case 6: GetTagByRank - Not Found
// ========================
TEST_F(HybmEntityTagInfoTest, GetTagByRank_NotFound)
{
    EXPECT_EQ(tagInfo_->GetTagByRank(999), "");
}

// ========================
// Test Case 7: AddTagOpInfo - Valid
// ========================
TEST_F(HybmEntityTagInfoTest, AddTagOpInfo_Valid)
{
    std::string tagOpInfo = "tag1:DEVICE_SDMA:tag2";
    EXPECT_EQ(tagInfo_->AddTagOpInfo(tagOpInfo), BM_OK);

    std::string tagOpInfo2 = "tag1:DEVICE_RDMA:tag2";
    EXPECT_EQ(tagInfo_->AddTagOpInfo(tagOpInfo2), BM_OK);

    // 验证 key 存在，且映射正确
    auto opType = tagInfo_->GetTag2TagOpType("tag1", "tag2");
    EXPECT_EQ(opType, HYBM_DOP_TYPE_SDMA | HYBM_DOP_TYPE_DEVICE_RDMA);
}

// ========================
// Test Case 8: AddTagOpInfo - Invalid Format
// ========================
TEST_F(HybmEntityTagInfoTest, AddTagOpInfo_Invalid_Format)
{
    std::string invalid = "tag1:INVALID_OP:tag2";
    EXPECT_EQ(tagInfo_->AddTagOpInfo(invalid), BM_INVALID_PARAM);
}

// ========================
// Test Case 9: AddTagOpInfo - Invalid opType
// ========================
TEST_F(HybmEntityTagInfoTest, AddTagOpInfo_Invalid_OpType)
{
    std::string invalid = "tag1:UNKNOWN_OP:tag2";
    EXPECT_EQ(tagInfo_->AddTagOpInfo(invalid), BM_INVALID_PARAM);
}

// ========================
// Test Case 10: GetTag2TagOpType - Valid
// ========================
TEST_F(HybmEntityTagInfoTest, GetTag2TagOpType_Valid)
{
    tagInfo_->AddTagOpInfo("tagA:HOST_RDMA:tagB");

    auto opType = tagInfo_->GetTag2TagOpType("tagA", "tagB");
    EXPECT_EQ(opType, HYBM_DOP_TYPE_HOST_RDMA);

    opType = tagInfo_->GetTag2TagOpType("tagB", "tagA");
    EXPECT_EQ(opType, HYBM_DOP_TYPE_HOST_RDMA);
}

// ========================
// Test Case 11: GetTag2TagOpType - Not Found
// ========================
TEST_F(HybmEntityTagInfoTest, GetTag2TagOpType_NotFound)
{
    auto opType = tagInfo_->GetTag2TagOpType("tag1", "tag2");
    EXPECT_EQ(opType, HYBM_DOP_TYPE_DEFAULT);
}

// ========================
// Test Case 12: GetRank2RankOpType
// ========================
TEST_F(HybmEntityTagInfoTest, GetRank2RankOpType)
{
    auto opType = tagInfo_->GetRank2RankOpType(0, 1);
    EXPECT_EQ(opType, HYBM_DOP_TYPE_DEFAULT);

    tagInfo_->AddRankTag(0, "tag0");
    opType = tagInfo_->GetRank2RankOpType(0, 1);
    EXPECT_EQ(opType, HYBM_DOP_TYPE_DEFAULT);

    tagInfo_->AddRankTag(1, "tag1");
    opType = tagInfo_->GetRank2RankOpType(0, 1);
    EXPECT_EQ(opType, HYBM_DOP_TYPE_DEFAULT);

    tagInfo_->AddTagOpInfo("tag0:HOST_RDMA:tag1");
    opType = tagInfo_->GetRank2RankOpType(0, 1);
    EXPECT_EQ(opType, HYBM_DOP_TYPE_HOST_RDMA);

    tagInfo_->AddTagOpInfo("tag0:DEVICE_SDMA:tag1");
    opType = tagInfo_->GetRank2RankOpType(0, 1);
    EXPECT_EQ(opType, HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_SDMA);
}

} // namespace mf
} // namespace ock