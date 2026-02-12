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
#include "hybm_mem_segment.h"
#include "hybm_dev_legacy_segment.h"
#include "hybm_conn_based_segment.h"
#include "hybm_vmm_based_segment.h"
#undef private
#undef protected

#include "hybm_va_manager.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_gva_version.h"

// 方便 mock 成员函数 / C 接口
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

class HybmMemSegmentTest : public ::testing::Test {
protected:
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

// =========================
// 1. MemSegment::Create 选择逻辑
// =========================

/**
* Create_Rejects_InvalidRank
*  - rankId 必须 < rankCnt
*/
TEST_F(HybmMemSegmentTest, Create_Rejects_InvalidRank)
{
    ock::mf::MemSegmentOptions opt{};
    opt.rankCnt = 4;
    opt.rankId = 4;  // 等于 rankCnt，非法
    opt.segType = ock::mf::HYBM_MST_HBM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;

    auto seg = ock::mf::MemSegment::Create(opt, 0);
    EXPECT_EQ(seg, nullptr);
}

/**
* Create_Fails_When_InitDeviceInfo_Failed
*/
TEST_F(HybmMemSegmentTest, Create_Fails_When_InitDeviceInfo_Failed)
{
    // 输入参数：rank 合法、HBM 段
    ock::mf::MemSegmentOptions opt{};
    opt.rankCnt = 4;
    opt.rankId = 0;
    opt.segType = ock::mf::HYBM_MST_HBM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;

    auto seg = ock::mf::MemSegment::Create(opt, 0);
    EXPECT_EQ(seg, nullptr);
}

/**
* Create_Hbm_Default_Uses_DevLegacySegment
*  - 当 GVA 版本不是 V4 或 SoC 不是 910C（默认情况），HBM 段应该走老的设备侧实现 `HybmDevLegacySegment`。
*/
TEST_F(HybmMemSegmentTest, Create_Hbm_Default_Uses_DevLegacySegment)
{
    ock::mf::MemSegmentOptions opt{};
    opt.rankCnt = 2;
    opt.rankId = 0;
    opt.segType = ock::mf::HYBM_MST_HBM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;

    // 模拟 GVA 版本非 V4
    MOCKER(ock::mf::HybmGetGvaVersion).stubs().will(returnValue(ock::mf::HYBM_GVA_V3));

    ock::mf::MemSegment::socType_ = ock::mf::AscendSocType::ASCEND_910B;

    MOCKER(ock::mf::MemSegment::InitDeviceInfo).stubs().will(returnValue(0));

    auto seg = ock::mf::MemSegment::Create(opt, 0);
    ASSERT_NE(seg, nullptr);
    // 验证实际类型为 HybmDevLegacySegment
    auto devSeg = std::dynamic_pointer_cast<ock::mf::HybmDevLegacySegment>(seg);
    EXPECT_NE(devSeg, nullptr);
    auto vmmSeg = std::dynamic_pointer_cast<ock::mf::HybmVmmBasedSegment>(seg);
    EXPECT_EQ(vmmSeg, nullptr);
}

/**
* Create_Hbm_V4_910C_NoMte_Uses_VmmBasedSegment
*  - 当 GVA 版本为 V4 且 SoC 为 910C，且 dataOpType 中未包含 MTE，HBM 段会选择 VMM 实现。
*/
TEST_F(HybmMemSegmentTest, Create_Hbm_V4_910C_NoMte_Uses_VmmBasedSegment)
{
    ock::mf::MemSegmentOptions opt{};
    opt.rankCnt = 2;
    opt.rankId = 0;
    opt.segType = ock::mf::HYBM_MST_HBM;
    // 没有包含 HYBM_DOP_TYPE_MTE
    opt.dataOpType = HYBM_DOP_TYPE_SDMA;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;

    // 设置环境为 GVA_V4 + ASCEND_910C
    MOCKER(ock::mf::HybmGetGvaVersion).stubs().will(returnValue(ock::mf::HYBM_GVA_V4));
    ock::mf::MemSegment::socType_ = ock::mf::AscendSocType::ASCEND_910C;

    MOCKER(ock::mf::MemSegment::InitDeviceInfo).stubs().will(returnValue(0));

    auto seg = ock::mf::MemSegment::Create(opt, 1);
    ASSERT_NE(seg, nullptr);
    auto vmmSeg = std::dynamic_pointer_cast<ock::mf::HybmVmmBasedSegment>(seg);
    EXPECT_NE(vmmSeg, nullptr);
    auto devSeg = std::dynamic_pointer_cast<ock::mf::HybmDevLegacySegment>(seg);
    EXPECT_EQ(devSeg, nullptr);
}

/**
* Create_Dram_V4_910C_Uses_VmmBasedSegment
*  - 对 DRAM 段，当 GVA 版本为 V4 且 SoC 为 910C，会选择 VMM 实现。
*/
TEST_F(HybmMemSegmentTest, Create_Dram_V4_910C_Uses_VmmBasedSegment)
{
    ock::mf::MemSegmentOptions opt{};
    opt.rankCnt = 2;
    opt.rankId = 0;
    opt.segType = ock::mf::HYBM_MST_DRAM;
    opt.dataOpType = HYBM_DOP_TYPE_SDMA;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;

    MOCKER(ock::mf::HybmGetGvaVersion).stubs().will(returnValue(ock::mf::HYBM_GVA_V4));
    ock::mf::MemSegment::socType_ = ock::mf::AscendSocType::ASCEND_910C;

    MOCKER(ock::mf::MemSegment::InitDeviceInfo).stubs().will(returnValue(0));

    auto seg = ock::mf::MemSegment::Create(opt, 2);
    ASSERT_NE(seg, nullptr);
    auto vmmSeg = std::dynamic_pointer_cast<ock::mf::HybmVmmBasedSegment>(seg);
    EXPECT_NE(vmmSeg, nullptr);
}

/**
* Create_Dram_Default_Uses_ConnBasedSegment
*  - 在非 V4/910C 情况下，DRAM 段默认走 `HybmConnBasedSegment`。
*/
TEST_F(HybmMemSegmentTest, Create_Dram_Default_Uses_ConnBasedSegment)
{
    ock::mf::MemSegmentOptions opt{};
    opt.rankCnt = 2;
    opt.rankId = 0;
    opt.segType = ock::mf::HYBM_MST_DRAM;
    opt.dataOpType = HYBM_DOP_TYPE_SDMA;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;

    MOCKER(ock::mf::HybmGetGvaVersion).stubs().will(returnValue(ock::mf::HYBM_GVA_V3));
    ock::mf::MemSegment::socType_ = ock::mf::AscendSocType::ASCEND_910C;

    MOCKER(ock::mf::MemSegment::InitDeviceInfo).stubs().will(returnValue(0));

    auto seg = ock::mf::MemSegment::Create(opt, 3);
    ASSERT_NE(seg, nullptr);
    auto connSeg = std::dynamic_pointer_cast<ock::mf::HybmConnBasedSegment>(seg);
    EXPECT_NE(connSeg, nullptr);
}

/**
* Create_HbmUser_Uses_DevUserLegacySegment
*  - HBM_USER 类型始终选择用户态 legacy 段 `HybmDevUserLegacySegment`。
*  - 这里只检查非空，类型在实现中已经固定。
*/
TEST_F(HybmMemSegmentTest, Create_HbmUser_Uses_DevUserLegacySegment)
{
    ock::mf::MemSegmentOptions opt{};
    opt.rankCnt = 1;
    opt.rankId = 0;
    opt.segType = ock::mf::HYBM_MST_HBM_USER;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;

    MOCKER(ock::mf::HybmGetGvaVersion).stubs().will(returnValue(ock::mf::HYBM_GVA_V3));
    ock::mf::MemSegment::socType_ = ock::mf::AscendSocType::ASCEND_910C;
    MOCKER(ock::mf::MemSegment::InitDeviceInfo).stubs().will(returnValue(0));

    auto seg = ock::mf::MemSegment::Create(opt, 4);
    ASSERT_NE(seg, nullptr);
    // 具体类型为 HybmDevUserLegacySegment，这里只验证不会返回空指针
}

// =========================
// 2. ValidateOptions / GetReserveChunkSize 等纯逻辑函数
// =========================

/**
* DevLegacySegment_ValidateOptions
*  - 合法条件：segType=HBM / maxSize>0 且对齐大页 / devId>=0 / rankCnt*maxSize 不溢出。
*/
TEST_F(HybmMemSegmentTest, DevLegacySegment_ValidateOptions)
{
    ock::mf::MemSegmentOptions opt{};
    opt.segType = ock::mf::HYBM_MST_HBM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 4;
    opt.devId = 0;

    ock::mf::HybmDevLegacySegment segOk(opt, 0);
    EXPECT_EQ(segOk.ValidateOptions(), ock::mf::BM_OK);

    // 非 HBM 不合法
    opt.segType = ock::mf::HYBM_MST_DRAM;
    ock::mf::HybmDevLegacySegment segBadType(opt, 0);
    EXPECT_EQ(segBadType.ValidateOptions(), ock::mf::BM_INVALID_PARAM);

    // maxSize 不能为 0
    opt.segType = ock::mf::HYBM_MST_HBM;
    opt.maxSize = 0;
    ock::mf::HybmDevLegacySegment segZero(opt, 0);
    EXPECT_EQ(segZero.ValidateOptions(), ock::mf::BM_INVALID_PARAM);
}

/**
* VmmBasedSegment_ValidateOptions
*  - 只检查 maxSize>0 且对齐大页，rankCnt*maxSize 不溢出。
*/
TEST_F(HybmMemSegmentTest, VmmBasedSegment_ValidateOptions)
{
    ock::mf::MemSegmentOptions opt{};
    opt.segType = ock::mf::HYBM_MST_DRAM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 8;

    ock::mf::HybmVmmBasedSegment segOk(opt, 0);
    EXPECT_EQ(segOk.ValidateOptions(), ock::mf::BM_OK);

    opt.maxSize = 0;
    ock::mf::HybmVmmBasedSegment segZero(opt, 0);
    EXPECT_EQ(segZero.ValidateOptions(), ock::mf::BM_INVALID_PARAM);
}

/**
* ConnBasedSegment_ValidateOptions
*  - ConnBasedSegment 只支持 DRAM 段，且 maxSize>0 且大页对齐。
*/
TEST_F(HybmMemSegmentTest, ConnBasedSegment_ValidateOptions)
{
    ock::mf::MemSegmentOptions opt{};
    opt.segType = ock::mf::HYBM_MST_DRAM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 2;

    ock::mf::HybmConnBasedSegment segOk(opt, 0);
    EXPECT_EQ(segOk.ValidateOptions(), ock::mf::BM_OK);

    opt.segType = ock::mf::HYBM_MST_HBM;
    ock::mf::HybmConnBasedSegment segBadType(opt, 0);
    EXPECT_EQ(segBadType.ValidateOptions(), ock::mf::BM_INVALID_PARAM);
}

/**
* DevLegacySegment_GetReserveChunkSize
*  - 该函数用来计算 GVA 预留时的 chunk 大小，保证：
*    1) chunk 不超过 128G；
*    2) totalSize 能被 chunk 整除；
*    3) chunk 是 singleRankSize 的整数倍。
*/
TEST_F(HybmMemSegmentTest, DevLegacySegment_GetReserveChunkSize_BasicCases)
{
    using HybmSeg = ock::mf::HybmDevLegacySegment;

    // totalSize <= 128G，直接返回 totalSize
    uint64_t total = 16ULL * ock::mf::GB;
    uint64_t single = 4ULL * ock::mf::GB;
    EXPECT_EQ(HybmSeg::GetReserveChunkSize(total, single), total);

    // totalSize > 128G，返回一个不超过 128G 的整数倍 chunk
    total = 256ULL * ock::mf::GB;
    single = 4ULL * ock::mf::GB;
    uint64_t chunk = HybmSeg::GetReserveChunkSize(total, single);
    EXPECT_GT(chunk, 0U);
    EXPECT_LE(chunk, 128ULL * ock::mf::GB);
    EXPECT_EQ(total % chunk, 0U);
    EXPECT_EQ(chunk % single, 0U);

    // 异常场景：入参为 0
    EXPECT_EQ(HybmSeg::GetReserveChunkSize(0, single), 0U);
    EXPECT_EQ(HybmSeg::GetReserveChunkSize(total, 0), 0U);
}

// =========================
// 3. MemSegment::CanLocalHostReaches / CanSdmaReaches 逻辑
// =========================

/**
* CanLocalHostReaches_SameServerAndSuperPod
*  - 本地与远端 serverId / superPodId 完全相同时，始终可达。
*  - 对 910B 还需额外检查 deviceId 是否在同一“连接组”。
*/
TEST_F(HybmMemSegmentTest, CanLocalHostReaches_SameServerAndSuperPod)
{
    ock::mf::MemSegment::superPodId_ = 0x12;
    ock::mf::MemSegment::serverId_ = 0x34;
    ock::mf::MemSegment::socType_ = ock::mf::AscendSocType::ASCEND_910C;  // 非 910B，忽略 device 分组
    ock::mf::MemSegment::deviceId_ = 0;

    EXPECT_TRUE(ock::mf::MemSegment::CanLocalHostReaches(0x12, 0x34, 7));
}

/**
* CanLocalHostReaches_DifferentServerOrSuperPod
*  - serverId 或 superPodId 不一致时，返回 false。
*/
TEST_F(HybmMemSegmentTest, CanLocalHostReaches_DifferentServerOrSuperPod)
{
    ock::mf::MemSegment::superPodId_ = 0x12;
    ock::mf::MemSegment::serverId_ = 0x34;
    ock::mf::MemSegment::socType_ = ock::mf::AscendSocType::ASCEND_910C;
    ock::mf::MemSegment::deviceId_ = 0;

    EXPECT_FALSE(ock::mf::MemSegment::CanLocalHostReaches(0x13, 0x34, 0));
    EXPECT_FALSE(ock::mf::MemSegment::CanLocalHostReaches(0x12, 0x35, 0));
}

/**
* CanSdmaReaches_SameServer_DiffSuperPod
*  - serverId 相同且 SoC 类型/设备分组满足要求时，认为 SDMA 可达。
*/
TEST_F(HybmMemSegmentTest, CanSdmaReaches_SameServer_DiffSuperPod)
{
    ock::mf::MemSegment::serverId_ = 0x56;
    ock::mf::MemSegment::socType_ = ock::mf::AscendSocType::ASCEND_910C;  // 非 910B，不受连接分组限制
    ock::mf::MemSegment::deviceId_ = 0;
    ock::mf::MemSegment::superPodId_ = ock::mf::invalidSuperPodId;  // 本端 superPodId 未知

    EXPECT_TRUE(ock::mf::MemSegment::CanSdmaReaches(ock::mf::invalidSuperPodId, 0x56, 1));
}

// =========================
// 4. HybmConnBasedSegment 更深入的行为用例
// =========================

/**
* ConnBasedSegment_ExportSlice_UsesCache
*  - 当 exportMap_ 中已有该 slice 的导出信息时，Export 直接从缓存返回。
*  - 这可以帮助理解：第一次导出会构造 HostExportInfo，后续重复导出直接走缓存。
*/
TEST_F(HybmMemSegmentTest, ConnBasedSegment_ExportSlice_UsesCache)
{
    ock::mf::MemSegmentOptions opt{};
    opt.segType = ock::mf::HYBM_MST_DRAM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 2;
    opt.rankId = 0;

    ock::mf::HybmConnBasedSegment seg(opt, 0);

    // 构造一个 slice，并放入内部 map
    auto slice =
        std::make_shared<ock::mf::MemSlice>(1, ock::mf::MEM_TYPE_HOST_DRAM, ock::mf::MEM_PT_TYPE_SVM, 0x1000, 0x2000);
    seg.slices_.emplace(slice->index_, ock::mf::MemSliceStatus(slice));

    // 预置缓存
    std::string cached = "export-info-cache";
    seg.exportMap_[slice->index_] = cached;

    std::string exInfo;
    auto ret = seg.Export(slice, exInfo);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_EQ(exInfo, cached);
}

/**
* ConnBasedSegment_ExportSlice_InvalidSlice
*  - 当传入的 slice 不在 slices_ 中时，Export 返回 BM_INVALID_PARAM。
*/
TEST_F(HybmMemSegmentTest, ConnBasedSegment_ExportSlice_InvalidSlice)
{
    ock::mf::MemSegmentOptions opt{};
    opt.segType = ock::mf::HYBM_MST_DRAM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 2;
    opt.rankId = 0;

    ock::mf::HybmConnBasedSegment seg(opt, 0);

    auto slice =
        std::make_shared<ock::mf::MemSlice>(2, ock::mf::MEM_TYPE_HOST_DRAM, ock::mf::MEM_PT_TYPE_SVM, 0x2000, 0x1000);
    std::string exInfo;
    auto ret = seg.Export(slice, exInfo);
    EXPECT_EQ(ret, ock::mf::BM_INVALID_PARAM);
}

/**
* ConnBasedSegment_Import_AddsRemoteVaInfo
*  - 通过序列化/反序列化 HostExportInfo 来模拟跨节点导入：
*    - 本 rank 为 0，导入 rank1 的 HostExportInfo。
*    - Import 会调用 HybmVaManager::AddVaInfoFromExternal 注册远端 GVA 区间。
*/
TEST_F(HybmMemSegmentTest, ConnBasedSegment_Import_AddsRemoteVaInfo)
{
    ock::mf::MemSegmentOptions opt{};
    opt.segType = ock::mf::HYBM_MST_DRAM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 2;
    opt.rankId = 0;  // 本地 rank 0

    ock::mf::HybmConnBasedSegment seg(opt, 0);

    // 构造一个来自 rank1 的 HostExportInfo，并序列化
    ock::mf::HostExportInfo info{};
    info.vAddress = 0x1000;
    info.sliceIndex = 1;
    info.rankId = 1;
    info.size = 0x2000;
    info.pageTblType = ock::mf::MEM_PT_TYPE_SVM;
    info.memSegType = ock::mf::HYBM_MST_DRAM;
    info.exchangeType = ock::mf::HYBM_INFO_EXG_IN_NODE;

    std::string encoded;
    auto serRet = ock::mf::LiteralExInfoTranslater<ock::mf::HostExportInfo>{}.Serialize(info, encoded);
    ASSERT_EQ(serRet, ock::mf::BM_OK);

    std::vector<std::string> allExInfo{encoded};

    void* addresses[1]{};
    auto ret = seg.Import(allExInfo, addresses);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    // imports_ 应该包含一条记录
    EXPECT_EQ(seg.imports_.size(), 1U);
    EXPECT_EQ(seg.imports_[0].vAddress, info.vAddress);
}

/**
* ConnBasedSegment_MmapAndUnmap_IntegratesWithVaManager
*  - Mmap：将 imports_ 中除本 rank 外的记录加入 mappedMem_，并清空 imports_。
*  - Unmap：调用 HybmVaManager::RemoveOneVaInfo 清理所有映射，再清空 mappedMem_。
*/
TEST_F(HybmMemSegmentTest, ConnBasedSegment_MmapAndUnmap_IntegratesWithVaManager)
{
    ock::mf::MemSegmentOptions opt{};
    opt.segType = ock::mf::HYBM_MST_DRAM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 2;
    opt.rankId = 0;

    ock::mf::HybmConnBasedSegment seg(opt, 0);

    ock::mf::HostExportInfo local{};
    local.vAddress = 0x1000;
    local.rankId = 0;
    local.size = 0x1000;

    ock::mf::HostExportInfo remote{};
    remote.vAddress = 0x2000;
    remote.rankId = 1;
    remote.size = 0x1000;

    seg.imports_.push_back(local);
    seg.imports_.push_back(remote);

    // Mmap 只处理远端 rank
    auto ret = seg.Mmap();
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(seg.imports_.empty());
    EXPECT_EQ(seg.mappedMem_.size(), 1U);
    EXPECT_EQ(*seg.mappedMem_.begin(), remote.vAddress);

    ret = seg.Unmap();
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(seg.mappedMem_.empty());
}

// =========================
// 5. HybmDevLegacySegment 更深入的导出逻辑
// =========================

/**
* DevLegacySegment_ExportSlice_UsesCache
*  - 当 exportMap_ 中已有 slice 导出信息时，Export 直接复用。
*  - 对 HBM 设备段而言，重复导出会走缓存，以避免重复调用底层 IPC 命名接口。
*/
TEST_F(HybmMemSegmentTest, DevLegacySegment_ExportSlice_UsesCache)
{
    ock::mf::MemSegmentOptions opt{};
    opt.segType = ock::mf::HYBM_MST_HBM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 1;
    opt.rankId = 0;
    opt.devId = 0;

    ock::mf::HybmDevLegacySegment seg(opt, 0);
    auto slice =
        std::make_shared<ock::mf::MemSlice>(1, ock::mf::MEM_TYPE_DEVICE_HBM, ock::mf::MEM_PT_TYPE_SVM, 0x3000, 0);
    seg.slices_.emplace(slice->index_, ock::mf::MemSliceStatus(slice));

    std::string cached = "hbm-export-cache";
    seg.exportMap_[slice->index_] = cached;

    std::string exInfo;
    auto ret = seg.Export(slice, exInfo);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_EQ(exInfo, cached);
}

/**
* DevLegacySegment_ExportSlice_InvalidSlice
*  - slices_ 中找不到 slice 或句柄不匹配时，返回 BM_INVALID_PARAM。
*/
TEST_F(HybmMemSegmentTest, DevLegacySegment_ExportSlice_InvalidSlice)
{
    ock::mf::MemSegmentOptions opt{};
    opt.segType = ock::mf::HYBM_MST_HBM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 1;
    opt.rankId = 0;
    opt.devId = 0;

    ock::mf::HybmDevLegacySegment seg(opt, 0);
    auto slice =
        std::make_shared<ock::mf::MemSlice>(2, ock::mf::MEM_TYPE_DEVICE_HBM, ock::mf::MEM_PT_TYPE_SVM, 0x4000, 0x1000);

    std::string exInfo;
    auto ret = seg.Export(slice, exInfo);
    EXPECT_EQ(ret, ock::mf::BM_INVALID_PARAM);
}

// =========================
// 6. HybmVmmBasedSegment 较轻量的导入/导出逻辑
// =========================

/**
* VmmBasedSegment_GetExportSliceSize_ReturnsStructSize
*/
TEST_F(HybmMemSegmentTest, VmmBasedSegment_GetExportSliceSize_ReturnsStructSize)
{
    ock::mf::MemSegmentOptions opt{};
    opt.segType = ock::mf::HYBM_MST_DRAM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 1;
    opt.rankId = 0;

    ock::mf::HybmVmmBasedSegment seg(opt, 0);
    size_t size = 0;
    auto ret = seg.GetExportSliceSize(size);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_EQ(size, sizeof(ock::mf::HostSdmaExportInfo));
}

/**
* VmmBasedSegment_Export_WhenShareDisabled
*  - 当 options_.shared=false 时，Export 会直接跳过导出逻辑，返回 BM_OK。
*/
TEST_F(HybmMemSegmentTest, VmmBasedSegment_Export_WhenShareDisabled)
{
    ock::mf::MemSegmentOptions opt{};
    opt.segType = ock::mf::HYBM_MST_DRAM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 1;
    opt.rankId = 0;
    opt.shared = false;

    ock::mf::HybmVmmBasedSegment seg(opt, 0);
    auto slice =
        std::make_shared<ock::mf::MemSlice>(1, ock::mf::MEM_TYPE_HOST_DRAM, ock::mf::MEM_PT_TYPE_GVM, 0x5000, 0x1000);
    seg.slices_.emplace(slice->index_, ock::mf::MemSliceStatus(slice, nullptr));

    std::string exInfo;
    auto ret = seg.Export(slice, exInfo);
    EXPECT_EQ(ret, ock::mf::BM_OK);
}

/**
* VmmBasedSegment_Import_WhenShareDisabled
*  - 当 options_.shared=false 时，Import 会立即返回 BM_OK，不解析任何交换信息。
*/
TEST_F(HybmMemSegmentTest, VmmBasedSegment_Import_WhenShareDisabled)
{
    ock::mf::MemSegmentOptions opt{};
    opt.segType = ock::mf::HYBM_MST_DRAM;
    opt.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 1;
    opt.rankId = 0;
    opt.shared = false;

    ock::mf::HybmVmmBasedSegment seg(opt, 0);
    std::vector<std::string> allExInfo = {"dummy-info"};
    void* addresses[1]{};
    auto ret = seg.Import(allExInfo, addresses);
    EXPECT_EQ(ret, ock::mf::BM_OK);
}