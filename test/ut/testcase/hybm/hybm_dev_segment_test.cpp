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
#include <mockcpp/mockcpp.hpp>

#define private public
#define protected public
#include "hybm_dev_legacy_segment.h"
#include "hybm_dev_user_legacy_segment.h"
#undef private
#undef protected

class HybmDevSegmentTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
    }
    static void TearDownTestCase()
    {
    }

    void SetUp() override
    {
        GlobalMockObject::reset();
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
    }
};

// 测试 HybmDevLegacySegment 功能
TEST_F(HybmDevSegmentTest, HybmDevLegacySegment)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    // 测试构造和参数验证
    ock::mf::HybmDevLegacySegment segment(options, 100);
    auto validateRet = segment.ValidateOptions();
    EXPECT_EQ(validateRet, ock::mf::BM_OK);
}

// 测试 HybmDevUserLegacySegment 功能
TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    // 测试构造和参数验证
    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    auto validateRet = segment.ValidateOptions();
    EXPECT_EQ(validateRet, ock::mf::BM_OK);
}

// 测试设备段功能修改拦截
TEST_F(HybmDevSegmentTest, DevSegment_FunctionModification_Intercept)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    // 测试创建一致性
    ock::mf::HybmDevLegacySegment segment1(options, 300);
    ock::mf::HybmDevLegacySegment segment2(options, 400);

    // 验证参数验证的一致性
    auto ret1 = segment1.ValidateOptions();
    auto ret2 = segment2.ValidateOptions();
    EXPECT_EQ(ret1, ret2);
}

// 测试设备段边界情况
TEST_F(HybmDevSegmentTest, DevSegment_BoundaryCases)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;

    // 测试无效大小
    options.maxSize = 0;
    ock::mf::HybmDevLegacySegment segment(options, 500);
    auto validateRet = segment.ValidateOptions();
    EXPECT_EQ(validateRet, ock::mf::BM_INVALID_PARAM);

    // 测试非大页对齐大小
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE / 2;
    ock::mf::HybmDevLegacySegment segment2(options, 600);
    validateRet = segment2.ValidateOptions();
    EXPECT_EQ(validateRet, ock::mf::BM_INVALID_PARAM);
}
