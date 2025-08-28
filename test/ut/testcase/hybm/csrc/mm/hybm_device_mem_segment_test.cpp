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
#include "hybm_device_mem_segment.h"
#undef private

using namespace ock::mf;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
const uint64_t g_allocSize = 2 * 1024 * 1024;
MemSegmentOptions g_options = {0, HYBM_ROLE_PEER, HYBM_MST_HBM, HYBM_INFO_EXG_IN_NODE, g_allocSize, 0, 8};

HbmExportInfo g_exportInfo = {EXPORT_INFO_MAGIC, EXPORT_INFO_VERSION, 0, 0, 0, 0, 0, 0, 0, g_allocSize, 0,
                              MEM_PT_TYPE_SVM, HYBM_MST_HBM, HYBM_INFO_EXG_IN_NODE};
const MemSegmentOptions g_seg_options = {0, HYBM_ROLE_PEER, HYBM_MST_HBM, HYBM_INFO_EXG_IN_NODE, 2UL * 1024UL * 1024UL, 0, 2};
}

class HybmDevideMemSegmentTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        auto path = std::getenv("ASCEND_HOME_PATH");
        EXPECT_NE(path, nullptr);
        auto libPath = std::string(path).append("/lib64");
        EXPECT_EQ(DlApi::LoadLibrary(libPath), BM_OK);
    }
    static void TearDownTestSuite()
    {
        DlApi::CleanupLibrary();
    }
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HybmDevideMemSegmentTest, CreateMemSeg_ShouldReturnNonNullptr)
{
    EXPECT_NE(MemSegment::Create(g_seg_options, 0), nullptr);
}

TEST_F(HybmDevideMemSegmentTest, CreateMemSeg_ShouldReturnNullptr_WhenOptionsInvalid)
{
    MemSegmentOptions options = g_seg_options;
    options.rankId = options.rankCnt + 1;
    EXPECT_EQ(MemSegment::Create(options, 0), nullptr);

    options = g_seg_options;
    options.segType = HYBM_MST_DRAM;
    EXPECT_NE(MemSegment::Create(options, 0), nullptr);
}

TEST_F(HybmDevideMemSegmentTest, ValidateOptions_ShouldReturnInvalidParam)
{
    MemSegmentOptions options = g_options;
    options.segType = HYBM_MST_DRAM;
    MemSegmentDevice seg1(options, 0);
    EXPECT_EQ(seg1.ValidateOptions(), BM_INVALID_PARAM);

    options = g_options;
    options.size = 0;
    MemSegmentDevice seg2(options, 0);
    EXPECT_EQ(seg2.ValidateOptions(), BM_INVALID_PARAM);

    options = g_options;
    options.size = DEVICE_LARGE_PAGE_SIZE - 1;
    MemSegmentDevice seg3(options, 0);
    EXPECT_EQ(seg3.ValidateOptions(), BM_INVALID_PARAM);

    options = g_options;
    MemSegmentDevice seg(options, 0);
    EXPECT_EQ(seg.ValidateOptions(), BM_OK);
}

TEST_F(HybmDevideMemSegmentTest, ReserveMemorySpace_ShouldReturnMallocFailed_WhenHalGvaReserveMemoryFailed)
{
    MemSegmentOptions options = g_options;
    MemSegmentDevice seg(options, 0);
    void *addr = nullptr;
    MOCKER_CPP(&drv::HalGvaReserveMemory, int (*)(uint64_t *, size_t, int32_t, uint64_t))
        .stubs().will(returnValue(-1));
    EXPECT_EQ(seg.ReserveMemorySpace(&addr), BM_MALLOC_FAILED);
}

TEST_F(HybmDevideMemSegmentTest, ReserveMemorySpace_ShouldReturnBmError_WhenAlreadyReserved)
{
    MemSegmentOptions options = g_options;
    MemSegmentDevice seg(options, 0);
    void *addr = nullptr;
    EXPECT_EQ(seg.ReserveMemorySpace(&addr), BM_OK);
    EXPECT_EQ(seg.ReserveMemorySpace(&addr), BM_ERROR);

    EXPECT_EQ(seg.UnreserveMemorySpace(), BM_OK);
}

TEST_F(HybmDevideMemSegmentTest, AllocLocalMemory_ShouldReturnError_WhenAllocError)
{
    MemSegmentOptions options = g_options;
    MemSegmentDevice seg(options, 0);
    void *addr = nullptr;
    EXPECT_EQ(seg.ReserveMemorySpace(&addr), BM_OK);

    std::shared_ptr<MemSlice> slice;
    EXPECT_EQ(seg.AllocLocalMemory(DEVICE_LARGE_PAGE_SIZE - 1, slice), BM_INVALID_PARAM);

    EXPECT_EQ(seg.AllocLocalMemory(DEVICE_LARGE_PAGE_SIZE + DEVICE_LARGE_PAGE_SIZE, slice), BM_INVALID_PARAM);

    MOCKER_CPP(&drv::HalGvaAlloc, int (*)(uint64_t, size_t, uint64_t)).stubs().will(returnValue(-1));
    EXPECT_EQ(seg.AllocLocalMemory(DEVICE_LARGE_PAGE_SIZE, slice), BM_DL_FUNCTION_FAILED);

    EXPECT_EQ(seg.UnreserveMemorySpace(), BM_OK);
}

TEST_F(HybmDevideMemSegmentTest, ReleaseSliceMemory_ShouldReturnError_WhenReleaseSliceMemoryError)
{
    MemSegmentOptions options = g_options;
    MemSegmentDevice seg(options, 0);

    EXPECT_EQ(seg.ReleaseSliceMemory(nullptr), BM_INVALID_PARAM);

    auto slice1 = std::make_shared<MemSlice>(-1, MEM_TYPE_DEVICE_HBM, MEM_PT_TYPE_SVM, 0, g_allocSize);
    EXPECT_EQ(seg.ReleaseSliceMemory(slice1), BM_INVALID_PARAM);

    void *addr = nullptr;
    EXPECT_EQ(seg.ReserveMemorySpace(&addr), BM_OK);
    std::shared_ptr<MemSlice> slice2;
    EXPECT_EQ(seg.AllocLocalMemory(DEVICE_LARGE_PAGE_SIZE, slice2), BM_OK);

    auto slice3 = std::make_shared<MemSlice>(slice2->index_, MEM_TYPE_DEVICE_HBM, MEM_PT_TYPE_SVM, 0, g_allocSize);
    EXPECT_EQ(seg.ReleaseSliceMemory(slice1), BM_INVALID_PARAM);

    std::string exInfo;
    EXPECT_NE(BM_OK, seg.Export(exInfo));
}

TEST_F(HybmDevideMemSegmentTest, Export_ShouldReturnError_WhenExportError)
{
    MemSegmentOptions options = g_options;
    MemSegmentDevice seg(options, 0);
    void *addr = nullptr;
    EXPECT_EQ(seg.ReserveMemorySpace(&addr), BM_OK);
    std::shared_ptr<MemSlice> slice;
    EXPECT_EQ(seg.AllocLocalMemory(DEVICE_LARGE_PAGE_SIZE, slice), BM_OK);

    std::string exInfo;
    EXPECT_NE(BM_OK, seg.Export(exInfo));

    EXPECT_EQ(seg.Export(nullptr, exInfo), BM_INVALID_PARAM);

    EXPECT_EQ(seg.Export(slice, exInfo), BM_OK);
    EXPECT_EQ(seg.Export(slice, exInfo), BM_OK);
}

TEST_F(HybmDevideMemSegmentTest, Export_ShouldReturnError_WhenGetDeviceIdFail)
{
    MemSegmentOptions options = g_options;
    MemSegmentDevice seg(options, 0);
    void *addr = nullptr;
    EXPECT_EQ(seg.ReserveMemorySpace(&addr), BM_OK);
    std::shared_ptr<MemSlice> slice;
    EXPECT_EQ(seg.AllocLocalMemory(DEVICE_LARGE_PAGE_SIZE, slice), BM_OK);

    std::string exInfo;
    MOCKER_CPP(&DlAclApi::RtGetDeviceInfo, int(*)(uint32_t, int32_t, int32_t, int64_t *)).stubs().will(returnValue(-1));
    EXPECT_EQ(seg.Export(slice, exInfo), BM_OK);

    MOCKER_CPP(&DlAclApi::RtDeviceGetBareTgid, int (*)(uint32_t *)).stubs().will(returnValue(-1));
    EXPECT_EQ(seg.Export(slice, exInfo), BM_OK);
}

TEST_F(HybmDevideMemSegmentTest, Import_ShouldReturnError_WhenImportError)
{
    MemSegmentOptions options = g_options;
    MemSegmentDevice seg(options, 0);
    void* addresses[1] = { nullptr };
    EXPECT_EQ(seg.Import({""}, addresses), BM_INVALID_PARAM);

    std::string exInfo;
    HbmExportInfo info = g_exportInfo;
    info.magic = 0;
    LiteralExInfoTranslater<HbmExportInfo>{}.Serialize(info, exInfo);
    EXPECT_EQ(seg.Import({exInfo}, addresses), BM_INVALID_PARAM);

    std::string exInfo1;
    info = g_exportInfo;
    LiteralExInfoTranslater<HbmExportInfo>{}.Serialize(info, exInfo1);
    std::string exInfo2;
    info = g_exportInfo;
    info.rankId = 1;
    info.deviceId = 1;
    LiteralExInfoTranslater<HbmExportInfo>{}.Serialize(info, exInfo2);

    MOCKER_CPP(&DlAclApi::RtSetIpcMemorySuperPodPid, int (*)(char *, uint32_t, int32_t *, int32_t))
        .stubs().will(returnValue(-1));
    EXPECT_EQ(seg.Import({exInfo1, exInfo2}, addresses), BM_DL_FUNCTION_FAILED);

    MOCKER_CPP(&DlAclApi::AclrtDeviceEnablePeerAccess, int (*)(int32_t, uint32_t)).stubs().will(returnValue(-1));
    EXPECT_EQ(seg.Import({exInfo1, exInfo2}, addresses), BM_DL_FUNCTION_FAILED);
}

TEST_F(HybmDevideMemSegmentTest, Mmap_ShouldReturnError_WhenMmapError)
{
    MemSegmentOptions options = g_options;
    MemSegmentDevice seg(options, 0);

    EXPECT_EQ(seg.Mmap(), BM_OK);

    HbmExportInfo info = g_exportInfo;
    std::string exInfo1;
    LiteralExInfoTranslater<HbmExportInfo>{}.Serialize(info, exInfo1);
    std::string exInfo2;
    info = g_exportInfo;
    info.rankId = 1;
    info.deviceId = 1;
    LiteralExInfoTranslater<HbmExportInfo>{}.Serialize(info, exInfo2);
    MOCKER_CPP(&drv::HalGvaOpen, int (*)(uint64_t, char *, size_t, uint64_t)).stubs().will(returnValue(-1));
    void* addresses[1] = { nullptr };
    EXPECT_EQ(seg.Import({exInfo1, exInfo2, exInfo2}, addresses), BM_OK);
    EXPECT_EQ(seg.Mmap(), BM_DL_FUNCTION_FAILED);

    GlobalMockObject::verify();
    EXPECT_EQ(seg.Mmap(), BM_OK);

    MOCKER_CPP(&drv::HalGvaClose, int (*)(uint64_t, uint64_t)).stubs().will(returnValue(-1));
    seg.Unmap();
}