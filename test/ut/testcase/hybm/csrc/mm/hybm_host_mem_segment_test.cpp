/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <sys/mman.h>
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "hybm_ex_info_transfer.h"
#define private public
#include "hybm_host_mem_segment.h"
#undef private

using namespace ock::mf;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
const uint64_t g_allocSize = 2 * 1024 * 1024;
MemSegmentOptions g_options = {0, HYBM_ROLE_PEER, HYBM_DOP_TYPE_SDMA,
                               HYBM_MST_HBM, HYBM_INFO_EXG_IN_NODE, g_allocSize, 0, 8};
const MemSegmentOptions g_seg_options = {0, HYBM_ROLE_PEER, HYBM_DOP_TYPE_SDMA, HYBM_MST_HBM,
                                         HYBM_INFO_EXG_IN_NODE, 2UL * 1024UL * 1024UL, 0, 2};
const void *src = reinterpret_cast<void *>(0x000000000000ULL);
const void *dst = reinterpret_cast<void *>(0x000000010000ULL);
}

class HybmHostMemSegmentTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HybmHostMemSegmentTest, ValidateOptionsErrorBranch)
{
    int ret = 0;
    MemSegmentOptions options = {0, HYBM_ROLE_PEER, HYBM_DOP_TYPE_SDMA, HYBM_MST_HBM,
                                 HYBM_INFO_EXG_IN_NODE, 1, 0, 2};
    MemSegmentHost seg1(options, 0);
    ret = seg1.ValidateOptions();
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    options.size = 0;
    MemSegmentHost seg2(options, 0);
    ret = seg2.ValidateOptions();
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    options.size = 2UL * 1024UL * 1024UL;
    MemSegmentHost seg3(options, 0);
    ret = seg3.ValidateOptions();
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    options.segType = HYBM_MST_DRAM;
    options.size = 1;
    MemSegmentHost seg4(options, 0);
    ret = seg4.ValidateOptions();
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    options.size = 0;
    MemSegmentHost seg5(options, 0);
    ret = seg5.ValidateOptions();
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmHostMemSegmentTest, AllocLocalMemoryErrorBranch)
{
    int ret = 0;
    MemSegmentOptions options = {0, HYBM_ROLE_PEER, HYBM_DOP_TYPE_SDMA, HYBM_MST_HBM,
                                 HYBM_INFO_EXG_IN_NODE, 2, 0, 2};
    MemSegmentHost seg(options, 0);
    std::shared_ptr<MemSlice> slice = std::make_shared<MemSlice>(0, MEM_TYPE_DEVICE_BUTT, MEM_PT_TYPE_BUTT, 0, 0);
    ret = seg.AllocLocalMemory(1, slice);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = seg.AllocLocalMemory(10, slice);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = seg.AllocLocalMemory(g_allocSize, slice);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    size_t size = 1;
    ret = seg.RegisterMemory(dst, 1, slice);
    EXPECT_EQ(ret, BM_OK);
    ret = seg.GetExportSliceSize(size);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmHostMemSegmentTest, ExportErrorBranch)
{
    int ret = 0;
    MemSegmentOptions options = {0, HYBM_ROLE_PEER, HYBM_DOP_TYPE_SDMA, HYBM_MST_HBM,
                                 HYBM_INFO_EXG_IN_NODE, g_allocSize, 0, 2};
    MemSegmentHost seg(options, 0);
    std::shared_ptr<MemSlice> slice = std::make_shared<MemSlice>(0, MEM_TYPE_DEVICE_BUTT, MEM_PT_TYPE_BUTT, 0, 0);
    std::string str{};
    ret = seg.Export(nullptr, str);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = seg.Export(slice, str);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    std::shared_ptr<MemSlice> slice2 = std::make_shared<MemSlice>(0, MEM_TYPE_DEVICE_HBM, MEM_PT_TYPE_BUTT, 1, 1);
    ret = seg.AllocLocalMemory(g_allocSize, slice);
    EXPECT_EQ(ret, BM_OK);
    ret = seg.Export(slice2, str);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    MOCKER_CPP(&LiteralExInfoTranslater<HostExportInfo>::Serialize, int(*)(HostExportInfo &, std::string &))
        .stubs().will(returnValue(-1));
    ret = seg.Export(slice, str);
    EXPECT_EQ(ret, BM_ERROR);

    GlobalMockObject::verify();
    MOCKER_CPP(&LiteralExInfoTranslater<HostExportInfo>::Serialize, int(*)(HostExportInfo &, std::string &))
        .stubs().will(returnValue(0));
    ret = seg.Export(slice, str);
    EXPECT_EQ(ret, BM_OK);
    ret = seg.Export(slice, str);
    EXPECT_EQ(ret, BM_OK);

    void *mapped = mmap(src, g_allocSize, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mapped != MAP_FAILED) {
        MemSegmentHost::LvaShmReservePhysicalMemory(mapped, g_allocSize);
    }
}

TEST_F(HybmHostMemSegmentTest, ImportErrorBranch)
{
    int ret = 0;
    MemSegmentOptions options = {0, HYBM_ROLE_PEER, HYBM_DOP_TYPE_SDMA, HYBM_MST_HBM,
                                 HYBM_INFO_EXG_IN_NODE, g_allocSize, 0, 2};
    MemSegmentHost seg(options, 0);
    void **addr = new void *[1];
    std::vector<std::string> info{};

    HostExportInfo dinfo{0, 1, 1, 1, 0, 1, 1};
    std::string infos{};
    infos.resize(sizeof(HostExportInfo));
    std::copy(reinterpret_cast<const char*>(&dinfo),
              reinterpret_cast<const char*>(&dinfo) + sizeof(HostExportInfo),
              infos.begin());
    info.push_back(infos);
    ret = seg.Import(info, addr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    MOCKER_CPP(&LiteralExInfoTranslater<HostExportInfo>::Deserialize, int(*)(const std::string &, HostExportInfo &))
        .stubs().will(returnValue(-1));
    ret = seg.Import(info, addr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    delete[] addr;
}

TEST_F(HybmHostMemSegmentTest, GetMemSliceErrorBranch)
{
    hybm_mem_slice_t mem_slice{};
    MemSegmentOptions options = {0, HYBM_ROLE_PEER, HYBM_DOP_TYPE_SDMA, HYBM_MST_HBM,
                                 HYBM_INFO_EXG_IN_NODE, g_allocSize, 0, 2};
    MemSegmentHost seg(options, 0);
    std::shared_ptr<MemSlice> slice = std::make_shared<MemSlice>(0, MEM_TYPE_DEVICE_HBM, MEM_PT_TYPE_BUTT, 1, 1);
    int result = seg.AllocLocalMemory(g_allocSize, slice);
    EXPECT_EQ(result, BM_OK);
    MOCKER_CPP(&MemSlice::GetIndexFrom, uint64_t(*)(hybm_mem_slice_t))
        .stubs().will(returnValue(1));
    auto ret = seg.GetMemSlice(mem_slice);
    EXPECT_EQ((ret == nullptr), 1);

    std::shared_ptr<MemSlice> slice2 = std::make_shared<MemSlice>(1, MEM_TYPE_DEVICE_HBM, MEM_PT_TYPE_BUTT, 1, 1);
    MemSegmentHost seg2(options, 0);
    result = seg2.AllocLocalMemory(g_allocSize, slice2);
    EXPECT_EQ(result, BM_OK);
    MOCKER_CPP(&MemSlice::ValidateId, bool(*)(hybm_mem_slice_t))
        .stubs().will(returnValue(false));
    ret = seg2.GetMemSlice(mem_slice);
    EXPECT_EQ((ret == nullptr), 1);

    MOCKER_CPP(&MemSegmentHost::MemoryInRange, bool(*)(const void *, uint64_t))
        .stubs().will(returnValue(false));
    uint32_t rankId = 0;
    seg.GetRankIdByAddr(nullptr, 0, rankId);
}