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

// ========================
// Test Case 13: AddRankTag - Multiple ranks
// ========================
TEST_F(HybmEntityTagInfoTest, AddRankTag_MultipleRanks)
{
    EXPECT_EQ(tagInfo_->AddRankTag(0, "rank0"), BM_OK);
    EXPECT_EQ(tagInfo_->AddRankTag(1, "rank1"), BM_OK);
    EXPECT_EQ(tagInfo_->AddRankTag(2, "rank2"), BM_OK);
    EXPECT_EQ(tagInfo_->GetTagByRank(0), "rank0");
    EXPECT_EQ(tagInfo_->GetTagByRank(1), "rank1");
    EXPECT_EQ(tagInfo_->GetTagByRank(2), "rank2");
}

// ========================
// Test Case 14: AddRankTag - Overwrite existing tag
// ========================
TEST_F(HybmEntityTagInfoTest, AddRankTag_OverwriteExisting)
{
    EXPECT_EQ(tagInfo_->AddRankTag(0, "rank0"), BM_OK);
    EXPECT_EQ(tagInfo_->GetTagByRank(0), "rank0");

    EXPECT_EQ(tagInfo_->AddRankTag(0, "rank0_new"), BM_OK);
    EXPECT_EQ(tagInfo_->GetTagByRank(0), "rank0_new");
}

// ========================
// Test Case 15: RemoveRankTag - Non-existent tag
// ========================
TEST_F(HybmEntityTagInfoTest, RemoveRankTag_NonExistent)
{
    EXPECT_EQ(tagInfo_->RemoveRankTag(999, "non_exist"), BM_OK);
    EXPECT_EQ(tagInfo_->GetTagByRank(999), "");
}

// ========================
// Test Case 16: RemoveRankTag - Wrong tag name
// ========================
TEST_F(HybmEntityTagInfoTest, RemoveRankTag_WrongTagName)
{
    tagInfo_->AddRankTag(0, "rank0");
    EXPECT_EQ(tagInfo_->GetTagByRank(0), "rank0");
    EXPECT_EQ(tagInfo_->RemoveRankTag(0, "no_use_input"), BM_OK);
    EXPECT_EQ(tagInfo_->GetTagByRank(0), "");
}

// ========================
// Test Case 17: AddTagOpInfo - Multiple tag pairs
// ========================
TEST_F(HybmEntityTagInfoTest, AddTagOpInfo_MultiplePairs)
{
    std::string tagOpInfo = "tag1:HOST_RDMA:tag2,tag2:DEVICE_SDMA:tag3,tag3:HOST_TCP:tag1";
    EXPECT_EQ(tagInfo_->AddTagOpInfo(tagOpInfo), BM_OK);

    EXPECT_EQ(tagInfo_->GetTag2TagOpType("tag1", "tag2"), HYBM_DOP_TYPE_HOST_RDMA);
    EXPECT_EQ(tagInfo_->GetTag2TagOpType("tag2", "tag3"), HYBM_DOP_TYPE_SDMA);
    EXPECT_EQ(tagInfo_->GetTag2TagOpType("tag3", "tag1"), HYBM_DOP_TYPE_HOST_TCP);
}

// ========================
// Test Case 18: AddTagOpInfo - Empty string
// ========================
TEST_F(HybmEntityTagInfoTest, AddTagOpInfo_EmptyString)
{
    EXPECT_EQ(tagInfo_->AddTagOpInfo(""), BM_OK);
}

// ========================
// Test Case 19: AddTagOpInfo - Invalid format (missing colon)
// ========================
TEST_F(HybmEntityTagInfoTest, AddTagOpInfo_InvalidFormat_MissingColon)
{
    std::string invalid = "tag1HOST_RDMAtag2";
    EXPECT_EQ(tagInfo_->AddTagOpInfo(invalid), BM_INVALID_PARAM);
}

// ========================
// Test Case 20: AddTagOpInfo - Invalid format (too many colons)
// ========================
TEST_F(HybmEntityTagInfoTest, AddTagOpInfo_InvalidFormat_TooManyColons)
{
    std::string invalid = "tag1:HOST_RDMA:tag2:extra";
    EXPECT_EQ(tagInfo_->AddTagOpInfo(invalid), BM_INVALID_PARAM);
}

// ========================
// Test Case 21: AddTagOpInfo - All op types
// ========================
TEST_F(HybmEntityTagInfoTest, AddTagOpInfo_AllOpTypes)
{
    EXPECT_EQ(tagInfo_->AddTagOpInfo("tag1:DEVICE_SDMA:tag2"), BM_OK);
    EXPECT_EQ(tagInfo_->AddTagOpInfo("tag2:DEVICE_RDMA:tag3"), BM_OK);
    EXPECT_EQ(tagInfo_->AddTagOpInfo("tag3:HOST_RDMA:tag4"), BM_OK);
    EXPECT_EQ(tagInfo_->AddTagOpInfo("tag4:HOST_TCP:tag5"), BM_OK);
    EXPECT_EQ(tagInfo_->AddTagOpInfo("tag5:HOST_URMA:tag6"), BM_OK);

    EXPECT_EQ(tagInfo_->GetTag2TagOpType("tag1", "tag2"), HYBM_DOP_TYPE_SDMA);
    EXPECT_EQ(tagInfo_->GetTag2TagOpType("tag2", "tag3"), HYBM_DOP_TYPE_DEVICE_RDMA);
    EXPECT_EQ(tagInfo_->GetTag2TagOpType("tag3", "tag4"), HYBM_DOP_TYPE_HOST_RDMA);
    EXPECT_EQ(tagInfo_->GetTag2TagOpType("tag4", "tag5"), HYBM_DOP_TYPE_HOST_TCP);
    EXPECT_EQ(tagInfo_->GetTag2TagOpType("tag5", "tag6"), HYBM_DOP_TYPE_HOST_URMA);
}

// ========================
// Test Case 22: GetTag2TagOpType - Same tag
// ========================
TEST_F(HybmEntityTagInfoTest, GetTag2TagOpType_SameTag)
{
    tagInfo_->AddTagOpInfo("tag1:HOST_RDMA:tag2");
    auto opType = tagInfo_->GetTag2TagOpType("tag1", "tag1");
    EXPECT_EQ(opType, HYBM_DOP_TYPE_DEFAULT);
}

// ========================
// Test Case 23: GetRank2RankOpType - Same rank
// ========================
TEST_F(HybmEntityTagInfoTest, GetRank2RankOpType_SameRank)
{
    tagInfo_->AddRankTag(0, "tag0");
    tagInfo_->AddTagOpInfo("tag0:HOST_RDMA:tag0");
    auto opType = tagInfo_->GetRank2RankOpType(0, 0);
    EXPECT_EQ(opType, HYBM_DOP_TYPE_HOST_RDMA);
}

// ========================
// Test Case 24: GetRank2RankOpType - Invalid rank
// ========================
TEST_F(HybmEntityTagInfoTest, GetRank2RankOpType_InvalidRank)
{
    auto opType = tagInfo_->GetRank2RankOpType(999, 1000);
    EXPECT_EQ(opType, HYBM_DOP_TYPE_DEFAULT);
}

// ========================
// Test Case 25: GetAllOpType
// ========================
TEST_F(HybmEntityTagInfoTest, GetAllOpType)
{
    hybm_options opt{};
    std::string tagOpInfo = "tag1:DEVICE_SDMA:tag2,tag2:DEVICE_RDMA:tag3,tag3:HOST_RDMA:tag1";
    memcpy(opt.tagOpInfo, tagOpInfo.c_str(), tagOpInfo.size()+1);
    opt.bmDataOpType = static_cast<hybm_data_op_type>(
        HYBM_DOP_TYPE_SDMA | HYBM_DOP_TYPE_DEVICE_RDMA | HYBM_DOP_TYPE_HOST_RDMA);
    auto ret = tagInfo_->TagInfoInit(opt);
    EXPECT_EQ(ret, BM_OK);
    uint32_t allOpType = tagInfo_->GetAllOpType();
    EXPECT_TRUE(allOpType & HYBM_DOP_TYPE_SDMA);
    EXPECT_TRUE(allOpType & HYBM_DOP_TYPE_DEVICE_RDMA);
    EXPECT_TRUE(allOpType & HYBM_DOP_TYPE_HOST_RDMA);
}

// ========================
// Test Case 26: GetOpTypeStr - Static method
// ========================
TEST_F(HybmEntityTagInfoTest, GetOpTypeStr_StaticMethod)
{
    std::string str1 = HybmEntityTagInfo::GetOpTypeStr(HYBM_DOP_TYPE_SDMA);
    EXPECT_FALSE(str1.empty());

    std::string str2 = HybmEntityTagInfo::GetOpTypeStr(HYBM_DOP_TYPE_HOST_RDMA);
    EXPECT_FALSE(str2.empty());

    std::string str3 = HybmEntityTagInfo::GetOpTypeStr(HYBM_DOP_TYPE_DEFAULT);
    EXPECT_FALSE(str3.empty());
}

// ========================
// Test Case 27: AddRankTag - Valid characters (underscore, numbers)
// ========================
TEST_F(HybmEntityTagInfoTest, AddRankTag_ValidChars_UnderscoreAndNumbers)
{
    EXPECT_EQ(tagInfo_->AddRankTag(0, "rank_0"), BM_OK);
    EXPECT_EQ(tagInfo_->AddRankTag(1, "rank1_2"), BM_OK);
    EXPECT_EQ(tagInfo_->AddRankTag(2, "_rank2"), BM_OK);
    EXPECT_EQ(tagInfo_->GetTagByRank(0), "rank_0");
    EXPECT_EQ(tagInfo_->GetTagByRank(1), "rank1_2");
    EXPECT_EQ(tagInfo_->GetTagByRank(2), "_rank2");
}

// ========================
// Test Case 28: AddTagOpInfo - Case sensitivity
// ========================
TEST_F(HybmEntityTagInfoTest, AddTagOpInfo_CaseSensitivity)
{
    EXPECT_EQ(tagInfo_->AddTagOpInfo("Tag1:HOST_RDMA:Tag2"), BM_OK);
    EXPECT_EQ(tagInfo_->GetTag2TagOpType("Tag1", "Tag2"), HYBM_DOP_TYPE_HOST_RDMA);
    EXPECT_EQ(tagInfo_->GetTag2TagOpType("tag1", "tag2"), HYBM_DOP_TYPE_DEFAULT);
}

// ========================
// Test Case 29: Complex scenario - Multiple ranks with multiple tag ops
// ========================
TEST_F(HybmEntityTagInfoTest, ComplexScenario_MultipleRanksAndTagOps)
{
    EXPECT_EQ(tagInfo_->AddRankTag(0, "rank0"), BM_OK);
    EXPECT_EQ(tagInfo_->AddRankTag(1, "rank1"), BM_OK);
    EXPECT_EQ(tagInfo_->AddRankTag(2, "rank2"), BM_OK);

    EXPECT_EQ(tagInfo_->AddTagOpInfo("rank0:HOST_RDMA:rank1,rank1:DEVICE_SDMA:rank2,rank2:HOST_TCP:rank0"), BM_OK);

    EXPECT_EQ(tagInfo_->GetRank2RankOpType(0, 1), HYBM_DOP_TYPE_HOST_RDMA);
    EXPECT_EQ(tagInfo_->GetRank2RankOpType(1, 2), HYBM_DOP_TYPE_SDMA);
    EXPECT_EQ(tagInfo_->GetRank2RankOpType(2, 0), HYBM_DOP_TYPE_HOST_TCP);
}

} // namespace mf
} // namespace ock