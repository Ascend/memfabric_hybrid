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
const void *src = reinterpret_cast<void *>(0x000000000000ULL);
const void *dst = reinterpret_cast<void *>(0x000000010000ULL);
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

TEST_F(HybmDeviceUserMemSegTest, ReturnValueDirectly)
{
    void **addr = nullptr;
    std::vector<uint32_t> ranks{};
    std::shared_ptr<MemSlice> slice =
        std::make_shared<MemSlice>(0, MEM_TYPE_DEVICE_BUTT, MEM_PT_TYPE_BUTT, 0, 0);
    MemSegmentOptions options = g_seg_options;
    MemSegmentDeviceUseMem seg(options, 0);
    EXPECT_EQ(seg.ValidateOptions(), BM_OK);
    EXPECT_EQ(seg.ReserveMemorySpace(addr), BM_NOT_SUPPORTED);
    EXPECT_EQ(seg.UnreserveMemorySpace(), BM_OK);
    EXPECT_EQ(seg.AllocLocalMemory(0, slice), BM_NOT_SUPPORTED);
    EXPECT_EQ(seg.RemoveImported(ranks), BM_NOT_SUPPORTED);
    EXPECT_EQ(seg.Mmap(), BM_NOT_SUPPORTED);
    EXPECT_EQ(seg.Unmap(), BM_NOT_SUPPORTED);
    EXPECT_EQ(seg.GetMemoryType(), HYBM_MEM_TYPE_DEVICE);
}

TEST_F(HybmDeviceUserMemSegTest, RegisterMemoryErrorBranch)
{
    int ret = 0;
    MemSegmentOptions options = g_seg_options;
    MemSegmentDeviceUseMem seg(options, 0);
    void *addr = nullptr;
    std::shared_ptr<MemSlice> slice =
        std::make_shared<MemSlice>(0, MEM_TYPE_DEVICE_BUTT, MEM_PT_TYPE_BUTT, 0, 0);
    ret = seg.RegisterMemory(addr, 0, slice);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = seg.RegisterMemory(dst, 0, slice);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = seg.RegisterMemory(addr, 1, slice);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    ret = seg.ReleaseSliceMemory(slice);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    ret = seg.RegisterMemory(dst, 1, slice);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    MOCKER_CPP(&DlAclApi::RtSetIpcMemorySuperPodPid, int32_t(*)(const char *, uint32_t,
        int32_t, int32_t)).stubs().will(returnValue(1));
    MOCKER_CPP(&DlAclApi::AclrtDeviceEnablePeerAccess, int32_t(*)(int32_t, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlAclApi::RtIpcSetMemoryName, int32_t(*)(const void *, uint64_t, char *, uint32_t))
        .stubs().will(returnValue(0));
    HbmExportDeviceInfo dinfo{0, 1, 1, 1, 0, 1, 1};
    std::string info{};
    info.resize(sizeof(HbmExportDeviceInfo));
    std::copy(reinterpret_cast<const char*>(&dinfo),
              reinterpret_cast<const char*>(&dinfo) + sizeof(HbmExportDeviceInfo),
              info.begin());
    ret = seg.ImportDeviceInfo(info);
    EXPECT_EQ(ret, BM_OK);
    ret = seg.RegisterMemory(dst, 1, slice);
    EXPECT_EQ(ret, BM_OK);

    MOCKER_CPP(&MemSegmentDevice::CanSdmaReaches, bool(*)(uint32_t, uint32_t))
        .stubs().will(returnValue(true));
    ret = seg.RegisterMemory(dst, 1, slice);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    ret = seg.ReleaseSliceMemory(slice);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    MOCKER_CPP(&DlAclApi::RtIpcDestroyMemoryName, int32_t(*)(const char *))
        .stubs().will(returnValue(0));
    ret = seg.ReleaseSliceMemory(slice);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmDeviceUserMemSegTest, ExportErrorBranch)
{
    int ret = 0;
    MemSegmentOptions options = g_seg_options;
    MemSegmentDeviceUseMem seg(options, 0);
    void *addr = nullptr;
    std::string info{};
    ret = seg.Export(info);
    EXPECT_EQ(ret, BM_ERROR);
    MOCKER_CPP(&LiteralExInfoTranslater<HbmExportDeviceInfo>::Serialize,
        int(*)(HbmExportDeviceInfo &, std::string &)).stubs().will(returnValue(-1));
    MOCKER_CPP(&DlAclApi::AclrtGetDevice, int32_t(*)(int32_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlAclApi::RtDeviceGetBareTgid, int32_t(*)(uint32_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlAclApi::RtGetDeviceInfo, int32_t(*)(uint32_t, int32_t, int32_t, int64_t *))
        .stubs().will(returnValue(0));
    ret = seg.Export(info);
    EXPECT_EQ(ret, BM_ERROR);

    std::shared_ptr<MemSlice> slice =
        std::make_shared<MemSlice>(0, MEM_TYPE_DEVICE_BUTT, MEM_PT_TYPE_BUTT, 0, 0);
    ret = seg.Export(slice, info);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    MOCKER_CPP(&DlAclApi::RtIpcSetMemoryName, int32_t(*)(const void *, uint64_t, char *, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&LiteralExInfoTranslater<HbmExportSliceInfo>::Serialize, int(*)(HbmExportSliceInfo &, std::string &))
        .stubs().will(returnValue(-1));
    ret = seg.RegisterMemory(dst, 1, slice);
    EXPECT_EQ(ret, BM_OK);
    ret = seg.Export(slice, info);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmDeviceUserMemSegTest, ImportErrorBranch)
{
    std::vector<std::string> info{};
    void **addr = new void *[1];
    int ret = 0;
    MemSegmentOptions options = g_seg_options;
    MemSegmentDeviceUseMem seg(options, 0);
    ret = seg.Import(info, addr);
    EXPECT_EQ(ret, BM_OK);
    info.push_back("a");
    ret = seg.Import(info, addr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    info.pop_back();

    MOCKER_CPP(&MemSegmentDeviceUseMem::ImportDeviceInfo, int32_t(*)(const std::string &))
        .stubs().will(returnValue(-1));
    HbmExportDeviceInfo dinfo{0, 1, 1, 1, 0, 1, 1};
    std::string infos{};
    infos.resize(sizeof(HbmExportDeviceInfo));
    std::copy(reinterpret_cast<const char*>(&dinfo),
              reinterpret_cast<const char*>(&dinfo) + sizeof(HbmExportDeviceInfo),
              infos.begin());
    info.push_back(infos);
    ret = seg.Import(info, nullptr);
    EXPECT_EQ(ret, BM_ERROR);
    ret = seg.Import(info, addr);
    EXPECT_EQ(ret, BM_ERROR);

    delete[] addr;
}

TEST_F(HybmDeviceUserMemSegTest, GetMemSliceErrorBranch)
{
    int ret = 0;
    MemSegmentOptions options = g_options;
    options.dataOpType = HYBM_DOP_TYPE_DEVICE_RDMA;
    MemSegmentDeviceUseMem seg(options, 0);
    hybm_mem_slice_t slice{};
    EXPECT_EQ((seg.GetMemSlice(slice) == nullptr), 1);

    MOCKER_CPP(&MemSlice::GetIndexFrom, uint64_t(*)(hybm_mem_slice_t))
        .stubs().will(returnValue(1));
    MOCKER_CPP(&DlAclApi::RtIpcCloseMemory, int32_t(*)(const void *))
        .stubs().will(returnValue(0));
    std::shared_ptr<MemSlice> remoteSlice;
    HbmExportSliceInfo dinfo{1, 1, 1, 1, 0, 1, 1, "aa"};
    std::string infos{};
    infos.resize(sizeof(HbmExportSliceInfo));
    std::copy(reinterpret_cast<const char*>(&dinfo),
              reinterpret_cast<const char*>(&dinfo) + sizeof(HbmExportSliceInfo),
              infos.begin());
    ret = seg.ImportSliceInfo(infos, remoteSlice);
    EXPECT_EQ(ret, BM_OK);
    ret = seg.ImportSliceInfo(infos, remoteSlice);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ((seg.GetMemSlice(slice) == nullptr), 1);
}

TEST_F(HybmDeviceUserMemSegTest, MemoryInRangeErrorBranch)
{
    bool ret = 0;
    MemSegmentOptions options = g_options;
    MemSegmentDeviceUseMem seg(options, 0);
    ret = seg.MemoryInRange(reinterpret_cast<const void *>(src), 0);
    EXPECT_EQ(ret, false);
}