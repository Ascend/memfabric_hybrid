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
#include "hybm_def.h"
#include "hybm_define.h"
#undef private
#undef protected

using namespace ock::mf;

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
    EXPECT_EQ(validateRet, BM_OK);
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
    EXPECT_EQ(validateRet, BM_OK);
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
    EXPECT_EQ(validateRet, BM_INVALID_PARAM);

    // 测试非大页对齐大小
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE / 2;
    ock::mf::HybmDevLegacySegment segment2(options, 600);
    validateRet = segment2.ValidateOptions();
    EXPECT_EQ(validateRet, BM_INVALID_PARAM);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_ReleaseSliceMemory_NotExist1)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    auto validateRet = segment.ValidateOptions();
    EXPECT_EQ(validateRet, BM_OK);

    auto unregisteredSlice = std::make_shared<ock::mf::MemSlice>(
        99999,
        HYBM_MEM_TYPE_DEVICE,
        ock::mf::MEM_PT_TYPE_SVM,
        0x10000000ULL,
        0x20000000ULL,
        4096ULL
    );

    auto ret = segment.ReleaseSliceMemory(unregisteredSlice);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_ReleaseSliceMemory_NotExist2)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    auto slice = std::make_shared<ock::mf::MemSlice>(
        0xFFFF,
        HYBM_MEM_TYPE_DEVICE,
        ock::mf::MEM_PT_TYPE_SVM,
        0x10000000ULL,
        0x20000000ULL,
        4096ULL
    );

    auto ret = segment.ReleaseSliceMemory(slice);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_RollbackIpcMemory)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    auto validateRet = segment.ValidateOptions();
    EXPECT_EQ(validateRet, BM_OK);

    void* addrs1[3] = {nullptr, nullptr, nullptr};
    segment.RollbackIpcMemory(addrs1, 3);

    void* dummy1 = reinterpret_cast<void*>(0x1000);
    void* dummy2 = reinterpret_cast<void*>(0x2000);
    void* addrs2[4] = {dummy1, nullptr, dummy2, nullptr};
    segment.RollbackIpcMemory(addrs2, 4);

    segment.RollbackIpcMemory(nullptr, 0);

    SUCCEED();
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_Import_Empty)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    std::vector<std::string> emptyInfos;
    void* addrs[1] = {nullptr};

    auto ret = segment.Import(emptyInfos, addrs);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_Import_ValidSliceMagic)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    ock::mf::UserHbmExportSliceInfo exportInfo{};
    exportInfo.magic = ock::mf::HBM_SLICE_EXPORT_INFO_MAGIC;
    exportInfo.segmentType = ock::mf::SEGMENT_TYPE_USER_DEV;
    exportInfo.gvaOffset = 0x10000000ULL;
    exportInfo.address = 0x20000000ULL;
    exportInfo.size = 4096;
    strncpy(exportInfo.name, "test", sizeof(exportInfo.name) - 1);
    exportInfo.name[sizeof(exportInfo.name) - 1] = '\0';

    std::string infoStr(reinterpret_cast<const char*>(&exportInfo), sizeof(exportInfo));
    std::vector<std::string> allExInfo = {infoStr};
    void* addresses[1] = {nullptr};

    auto ret = segment.Import(allExInfo, addresses);

    EXPECT_NE(ret, BM_INVALID_PARAM);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_Import_InvalidMagic)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    std::string badInfo(16, 'X');
    *reinterpret_cast<uint64_t*>(badInfo.data()) = 0xBADBADBADBADULL;

    std::vector<std::string> infos = {badInfo};
    void* addrs[1] = {nullptr};

    auto ret = segment.Import(infos, addrs);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_Import_AddressesNull)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    std::string badInfo(16, 'X');
    *reinterpret_cast<uint64_t*>(badInfo.data()) = 0xBADBADBADBADULL;

    std::vector<std::string> infos = {badInfo};

    auto ret = segment.Import(infos, nullptr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_RemoveImported_NoCrash)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    EXPECT_EQ(segment.RemoveImported({}), BM_OK);

    EXPECT_EQ(segment.RemoveImported({999}), BM_OK);

    EXPECT_EQ(segment.RemoveImported({1, 2, 3}), BM_OK);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_RemoveSliceInfo_RankNotExist)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    segment.RemoveSliceInfo(999);
    SUCCEED();
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_RemoveSliceInfo_SingleSliceNoSdma)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;
    options.dataOpType = HYBM_DOP_TYPE_DEFAULT;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    const uint32_t rankId = 5;
    const uint32_t sliceIndex = 100;
    const uint64_t gva = 0x10000000ULL;
    const uint64_t vAddress = 0x20000000ULL;
    const std::string sliceName = "slice_5_100";

    auto remoteSlice = std::make_shared<ock::mf::MemSlice>(
        sliceIndex,
        HYBM_MEM_TYPE_DEVICE,
        ock::mf::MEM_PT_TYPE_SVM,
        gva,
        vAddress,
        4096ULL
    );

    segment.rankToRemoteSlices_[rankId].assign({remoteSlice});

    void* addrKey = reinterpret_cast<void*>(static_cast<uintptr_t>(vAddress));
    segment.registerAddrs_.insert(addrKey);

    segment.remoteSlices_[static_cast<uint16_t>(sliceIndex)] =
        ock::mf::RegisterSlice(remoteSlice, sliceName);

    ock::mf::UserHbmExportSliceInfo exportInfo{};
    exportInfo.magic = ock::mf::HBM_SLICE_EXPORT_INFO_MAGIC;
    exportInfo.segmentType = ock::mf::SEGMENT_TYPE_USER_DEV;
    exportInfo.gvaOffset = gva;
    exportInfo.address = vAddress;
    exportInfo.size = 4096;
    exportInfo.rankId = rankId;
    exportInfo.logicDeviceId = 0;
    exportInfo.superPodId = 0;
    exportInfo.serverId = 0;
    strncpy(exportInfo.name, sliceName.c_str(), sizeof(exportInfo.name) - 1);
    exportInfo.name[sizeof(exportInfo.name) - 1] = '\0';

    segment.importedSliceInfo_[sliceName] = exportInfo;

    EXPECT_EQ(segment.rankToRemoteSlices_.count(rankId), 1U);
    EXPECT_EQ(segment.registerAddrs_.count(addrKey), 1U);
    EXPECT_EQ(segment.remoteSlices_.count(static_cast<uint16_t>(sliceIndex)), 1U);
    EXPECT_EQ(segment.importedSliceInfo_.count(sliceName), 1U);

    segment.RemoveSliceInfo(rankId);

    EXPECT_EQ(segment.rankToRemoteSlices_.count(rankId), 0U);
    EXPECT_EQ(segment.registerAddrs_.count(addrKey), 0U);
    EXPECT_EQ(segment.remoteSlices_.count(static_cast<uint16_t>(sliceIndex)), 0U);
    EXPECT_EQ(segment.importedSliceInfo_.count(sliceName), 0U);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_Mmap_NotSupported)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    auto ret = segment.Mmap();

    EXPECT_EQ(ret, BM_NOT_SUPPORTED);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_Unmap_NotSupported)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    auto ret = segment.Unmap();

    EXPECT_EQ(ret, BM_NOT_SUPPORTED);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_ImportDeviceInfo_DeserializeFailed)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    std::string badInfo = "invalid_serialized_data";
    auto ret = segment.ImportDeviceInfo(badInfo);
    EXPECT_NE(ret, BM_OK);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_ImportDeviceInfo_InvalidLogicDeviceId)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    ock::mf::HbmExportDeviceInfo deviceInfo{};
    deviceInfo.magic = ock::mf::ENTITY_EXPORT_INFO_MAGIC;
    deviceInfo.logicDeviceId = 16;
    deviceInfo.rankId = 0;
    deviceInfo.sdid = 123;
    deviceInfo.pid = 456;

    std::string info(reinterpret_cast<const char*>(&deviceInfo), sizeof(deviceInfo));
    auto ret = segment.ImportDeviceInfo(info);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_ImportDeviceInfo_SuccessNoP2PNoSlices)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    const uint32_t localDeviceId = 5;
    segment.logicDeviceId_ = localDeviceId;
    segment.deviceId_ = 0;

    ASSERT_TRUE(segment.registerSlices_.empty());

    ock::mf::HbmExportDeviceInfo deviceInfo{};
    deviceInfo.magic = ock::mf::ENTITY_EXPORT_INFO_MAGIC;
    deviceInfo.logicDeviceId = localDeviceId;
    deviceInfo.rankId = 10;
    deviceInfo.sdid = 123;
    deviceInfo.pid = 456;

    std::string info(reinterpret_cast<const char*>(&deviceInfo), sizeof(deviceInfo));
    auto ret = segment.ImportDeviceInfo(info);
    EXPECT_EQ(ret, BM_OK);

    EXPECT_TRUE(segment.importedDeviceInfo_.count(10) > 0);
    const auto& stored = segment.importedDeviceInfo_.at(10);
    EXPECT_EQ(stored.logicDeviceId, localDeviceId);
    EXPECT_EQ(stored.rankId, 10U);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_ImportSliceInfo_DeserializeFailed)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;
    options.dataOpType = 0; // avoid hardware paths

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    std::string badInfo = "invalid_data";
    ock::mf::MemSlicePtr remoteSlice;
    auto ret = segment.ImportSliceInfo(badInfo, remoteSlice);
    EXPECT_NE(ret, BM_OK);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_ImportSliceInfo_InvalidLogicDeviceId)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;
    options.dataOpType = 0;

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    ock::mf::UserHbmExportSliceInfo sliceInfo{};
    sliceInfo.logicDeviceId = 16; // >= MAX_DEVICE_COUNT (16) → invalid
    sliceInfo.rankId = 5;
    sliceInfo.gvaOffset = 0x10000000ULL;
    sliceInfo.size = 4096;
    strncpy(sliceInfo.name, "test_slice", sizeof(sliceInfo.name) - 1);

    std::string info(reinterpret_cast<const char*>(&sliceInfo), sizeof(sliceInfo));
    ock::mf::MemSlicePtr remoteSlice;
    auto ret = segment.ImportSliceInfo(info, remoteSlice);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmDevSegmentTest, HybmDevUserLegacySegment_ImportSliceInfo_SuccessNoHardware)
{
    ock::mf::MemSegmentOptions options{};
    options.segType = ock::mf::HYBM_MST_HBM;
    options.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    options.rankCnt = 1;
    options.dataOpType = 0; // ← 关键：禁用 SDMA/RDMA

    ock::mf::HybmDevUserLegacySegment segment(options, 200);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    // Prepare valid slice info
    ock::mf::UserHbmExportSliceInfo sliceInfo{};
    sliceInfo.logicDeviceId = 5; // < 16, valid
    sliceInfo.rankId = 10;
    sliceInfo.gvaOffset = 0x10000000ULL;
    sliceInfo.size = 4096;
    strncpy(sliceInfo.name, "slice_10_5", sizeof(sliceInfo.name) - 1);

    std::string info(reinterpret_cast<const char*>(&sliceInfo), sizeof(sliceInfo));
    ock::mf::MemSlicePtr remoteSlice;
    auto ret = segment.ImportSliceInfo(info, remoteSlice);
    EXPECT_EQ(ret, BM_OK);
    ASSERT_NE(remoteSlice, nullptr);

    // Verify outputs
    EXPECT_EQ(remoteSlice->gva_, sliceInfo.gvaOffset);
    EXPECT_EQ(remoteSlice->size_, sliceInfo.size);
    EXPECT_EQ(remoteSlice->memType_, HYBM_MEM_TYPE_DEVICE);
    EXPECT_EQ(remoteSlice->index_, 0U); // first slice, sliceCount_=0 → index=0

    // Verify containers (via friend)
    EXPECT_EQ(segment.rankToRemoteSlices_.count(10), 1U);
    EXPECT_EQ(segment.rankToRemoteSlices_.at(10).size(), 1U);
    EXPECT_EQ(segment.rankToRemoteSlices_.at(10)[0], remoteSlice);

    EXPECT_EQ(segment.remoteSlices_.count(0), 1U);
    EXPECT_EQ(segment.remoteSlices_.at(0).name, "slice_10_5");
    EXPECT_EQ(segment.remoteSlices_.at(0).slice, remoteSlice);

    EXPECT_EQ(segment.importedSliceInfo_.count("slice_10_5"), 1U);
    const auto& stored = segment.importedSliceInfo_.at("slice_10_5");
    EXPECT_EQ(stored.gvaOffset, sliceInfo.gvaOffset);
    EXPECT_EQ(stored.rankId, 10U);
}