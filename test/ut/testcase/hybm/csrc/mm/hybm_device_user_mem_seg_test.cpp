/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "hybm.h"
#include "hybm_types.h"
#include "dl_acl_api.h"
#include "dl_api.h"
#include "dl_hal_api.h"
#include "devmm_svm_gva.h"
#include "hybm_ex_info_transfer.h"

#define private public
#include "hybm_device_user_mem_seg.h"
#undef private

using namespace ock::mf;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
const uint64_t g_allocSize = 2 * 1024 * 1024;
MemSegmentOptions g_options = {0, HYBM_ROLE_PEER, HYBM_DOP_TYPE_SDMA,
                               HYBM_MST_HBM, HYBM_INFO_EXG_IN_NODE, g_allocSize, 0, 8};

HbmExportInfo g_exportInfo = {EXPORT_INFO_MAGIC, EXPORT_INFO_VERSION, 0, 0, 0, 0, 0, 0, 0, g_allocSize, 0,
                              MEM_PT_TYPE_SVM, HYBM_MST_HBM, HYBM_INFO_EXG_IN_NODE};
const MemSegmentOptions g_seg_options = {0, HYBM_ROLE_PEER, HYBM_DOP_TYPE_SDMA, HYBM_MST_HBM,
                                         HYBM_INFO_EXG_IN_NODE, 2UL * 1024UL * 1024UL, 0, 2};
}

class HybmDeviceUserMemSegTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HybmDeviceUserMemSegTest, ValidateOptionsTest)
{
    void **addr = nullptr;
    std::vector<uint32_t> ranks{};
    std::shared_ptr<MemSlice> slice = std::make_shared<MemSlice>(0, MEM_TYPE_DEVICE_BUTT, MEM_PT_TYPE_BUTT, 0, 0);
    MemSegmentOptions options = g_seg_options;
    MemSegmentDeviceUseMem seg1(options, 0);
    EXPECT_EQ(seg1.ValidateOptions(), BM_OK);
    EXPECT_EQ(seg1.ReserveMemorySpace(addr), BM_NOT_SUPPORTED);
    EXPECT_EQ(seg1.UnreserveMemorySpace(), BM_OK);
    EXPECT_EQ(seg1.AllocLocalMemory(0, slice), BM_NOT_SUPPORTED);
    EXPECT_EQ(seg1.RemoveImported(ranks), BM_NOT_SUPPORTED);
    EXPECT_EQ(seg1.Mmap(), BM_NOT_SUPPORTED);
    EXPECT_EQ(seg1.Unmap(), BM_NOT_SUPPORTED);
}