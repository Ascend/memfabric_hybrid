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

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vector>

#define private   public
#define protected public
#include "hybm_conn_based_segment.h"
#undef private
#undef protected

#include "hybm_ex_info_transfer.h"
#include "hybm_va_manager.h"

using namespace ock::mf;

class HybmConnBasedSegmentTest : public testing::Test {
protected:
    static MemSegmentOptions MakeOptions(uint32_t rankCnt = 2U, uint32_t rankId = 1U)
    {
        MemSegmentOptions options{};
        options.segType = HYBM_MST_DRAM;
        options.maxSize = HYBM_LARGE_PAGE_SIZE;
        options.size = HYBM_LARGE_PAGE_SIZE;
        options.rankCnt = rankCnt;
        options.rankId = rankId;
        options.dataOpType = HYBM_DOP_TYPE_SDMA;
        return options;
    }

    static MemSlicePtr MakeHostSlice(uint16_t index, uint64_t gva, uint64_t hva, uint64_t size = HYBM_LARGE_PAGE_SIZE)
    {
        return std::make_shared<MemSlice>(index, HYBM_MEM_TYPE_HOST, MEM_PT_TYPE_SVM, gva, hva, size);
    }

    void AddHostVaInfo(uint64_t gva, uint64_t hva, uint64_t size, uint32_t rankId)
    {
        BaseAllocatedGvaInfo baseInfo{};
        baseInfo.va[HVM_GVA] = gva;
        baseInfo.va[HVM_HVA] = hva;
        baseInfo.size = size;
        baseInfo.memType = HYBM_MEM_TYPE_HOST;
        ASSERT_EQ(HybmVaManager::GetInstance().AddVaInfo(baseInfo, rankId), BM_OK);
    }

    void SetUp() override
    {
        GlobalMockObject::reset();
        HybmVaManager::GetInstance().ClearAll();
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
        HybmVaManager::GetInstance().ClearAll();
    }
};

/**
 * ValidateOptions_ChecksTypeAlignAndOverflow
 *  - 验证 DRAM 段的类型、页对齐和总大小溢出约束。
 */
TEST_F(HybmConnBasedSegmentTest, ValidateOptions_ChecksTypeAlignAndOverflow)
{
    auto options = MakeOptions();

    HybmConnBasedSegment segment(options, 0);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    options.segType = HYBM_MST_HBM;
    HybmConnBasedSegment badType(options, 0);
    EXPECT_EQ(badType.ValidateOptions(), BM_INVALID_PARAM);

    options = MakeOptions();
    options.maxSize = HYBM_LARGE_PAGE_SIZE / 2UL;
    HybmConnBasedSegment badAlign(options, 0);
    EXPECT_EQ(badAlign.ValidateOptions(), BM_INVALID_PARAM);

    options = MakeOptions();
    options.maxSize = (1ULL << 63U);
    options.rankCnt = 2U;
    HybmConnBasedSegment overflow(options, 0);
    EXPECT_EQ(overflow.ValidateOptions(), BM_INVALID_PARAM);
}

/**
 * ReserveAndUnreserveMemorySpace_ManageAddressWindow
 *  - 验证预留地址窗口成功，重复预留被拦截，并且可通过 UnReserve 回收。
 */
TEST_F(HybmConnBasedSegmentTest, ReserveAndUnreserveMemorySpace_ManageAddressWindow)
{
    auto options = MakeOptions(2U, 1U);
    HybmConnBasedSegment segment(options, 0);

    void *address = nullptr;
    ASSERT_EQ(segment.ReserveMemorySpace(&address), BM_OK);
    EXPECT_EQ(address, segment.globalVirtualAddress_);
    EXPECT_EQ(segment.totalVirtualSize_, options.rankCnt * options.maxSize);
    EXPECT_EQ(segment.localVirtualBase_, segment.globalVirtualAddress_ + options.maxSize * options.rankId);
    EXPECT_EQ(HybmVaManager::GetInstance().GetReservedCount(), 1U);

    EXPECT_EQ(segment.ReserveMemorySpace(&address), BM_NOT_INITIALIZED);

    EXPECT_EQ(segment.UnReserveMemorySpace(), BM_OK);
    EXPECT_EQ(segment.globalVirtualAddress_, nullptr);
    EXPECT_EQ(HybmVaManager::GetInstance().GetReservedCount(), 0U);
}

/**
 * LvaShmReservePhysicalMemory_TouchesEachLargePage
 *  - 验证按大页步长触碰物理页，并补写最后一个字节。
 */
TEST_F(HybmConnBasedSegmentTest, LvaShmReservePhysicalMemory_TouchesEachLargePage)
{
    std::vector<uint8_t> buffer(HYBM_LARGE_PAGE_SIZE * 2UL, 0x5AU);

    HybmConnBasedSegment::LvaShmReservePhysicalMemory(buffer.data(), buffer.size());

    EXPECT_EQ(buffer.front(), 0U);
    EXPECT_EQ(buffer[HYBM_LARGE_PAGE_SIZE], 0U);
    EXPECT_EQ(buffer.back(), 0U);
    EXPECT_EQ(buffer[1UL], 0x5AU);
}

/**
 * AllocLocalMemory_MapsOneSliceAndTracksVa
 *  - 验证本地分配会映射 slice、登记 VA，并更新内部计数。
 */
TEST_F(HybmConnBasedSegmentTest, AllocLocalMemory_MapsOneSliceAndTracksVa)
{
    auto options = MakeOptions(2U, 1U);
    HybmConnBasedSegment segment(options, 0);

    void *address = nullptr;
    ASSERT_EQ(segment.ReserveMemorySpace(&address), BM_OK);

    MemSlicePtr slice;
    ASSERT_EQ(segment.AllocLocalMemory(HYBM_LARGE_PAGE_SIZE, slice), BM_OK);
    ASSERT_NE(slice, nullptr);
    EXPECT_EQ(slice->index_, 0U);
    EXPECT_EQ(slice->gva_,
              reinterpret_cast<uint64_t>(segment.globalVirtualAddress_) + options.maxSize * options.rankId);
    EXPECT_EQ(slice->vAddress_, reinterpret_cast<uint64_t>(segment.localVirtualBase_));
    EXPECT_EQ(segment.allocatedSize_, HYBM_LARGE_PAGE_SIZE);
    EXPECT_EQ(segment.slices_.count(slice->index_), 1U);
    EXPECT_EQ(HybmVaManager::GetInstance().GetAllocCount(), 1U);

    MemSlicePtr invalidSlice;
    EXPECT_EQ(segment.AllocLocalMemory(HYBM_LARGE_PAGE_SIZE / 2UL, invalidSlice), BM_INVALID_PARAM);

    segment.FreeMemory();
}

/**
 * Export
 *  - 验证段级 Export、slice Export、序列化字段和 exportMap_ 缓存命中。
 */
TEST_F(HybmConnBasedSegmentTest, Export)
{
    auto options = MakeOptions(2U, 1U);

    HybmConnBasedSegment segment(options, 0);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    std::string segmentExInfo;
    EXPECT_EQ(segment.Export(segmentExInfo), BM_OK);

    constexpr uint16_t sliceIndex = 3U;
    constexpr uint64_t gva = 0x100000000ULL;
    constexpr uint64_t hva = 0x200000000ULL;
    auto slice = MakeHostSlice(sliceIndex, gva, hva);
    segment.slices_.emplace(slice->index_, MemSliceStatus(slice));
    AddHostVaInfo(gva, hva, slice->size_, options.rankId);

    std::string exInfo;
    ASSERT_EQ(segment.Export(slice, exInfo), BM_OK);
    ASSERT_EQ(segment.exportMap_.count(sliceIndex), 1U);
    EXPECT_EQ(segment.exportMap_.at(sliceIndex), exInfo);

    HostExportInfo exportedInfo{};
    ASSERT_EQ(LiteralExInfoTranslater<HostExportInfo>{}.Deserialize(exInfo, exportedInfo), 0);
    EXPECT_EQ(exportedInfo.magic, DRAM_SLICE_EXPORT_INFO_MAGIC);
    EXPECT_EQ(exportedInfo.version, EXPORT_INFO_VERSION);
    EXPECT_EQ(exportedInfo.gva, gva);
    EXPECT_EQ(exportedInfo.sliceIndex, sliceIndex);
    EXPECT_EQ(exportedInfo.rankId, options.rankId);
    EXPECT_EQ(exportedInfo.size, HYBM_LARGE_PAGE_SIZE);
    EXPECT_EQ(exportedInfo.pageTblType, MEM_PT_TYPE_SVM);
    EXPECT_EQ(exportedInfo.memSegType, HYBM_MST_DRAM);
    EXPECT_EQ(exportedInfo.exchangeType, HYBM_INFO_EXG_IN_NODE);

    std::string cachedExInfo = "cache_miss";
    EXPECT_EQ(segment.Export(slice, cachedExInfo), BM_OK);
    EXPECT_EQ(cachedExInfo, exInfo);
}

/**
 * Import_StoresDeserializedHostExportInfo
 *  - 验证导入信息会被反序列化后落入 imports_。
 */
TEST_F(HybmConnBasedSegmentTest, Import_StoresDeserializedHostExportInfo)
{
    HybmConnBasedSegment segment(MakeOptions(), 0);

    HostExportInfo info{};
    info.gva = 0x300000000ULL;
    info.sliceIndex = 2U;
    info.rankId = 0;
    info.size = HYBM_LARGE_PAGE_SIZE;

    std::string exInfo;
    ASSERT_EQ(LiteralExInfoTranslater<HostExportInfo>{}.Serialize(info, exInfo), BM_OK);

    void *addresses[1U] = {nullptr};
    ASSERT_EQ(segment.Import({exInfo}, addresses), BM_OK);
    ASSERT_EQ(segment.imports_.size(), 1U);
    EXPECT_EQ(segment.imports_[0].gva, info.gva);
    EXPECT_EQ(segment.imports_[0].sliceIndex, info.sliceIndex);
    EXPECT_EQ(segment.imports_[0].rankId, info.rankId);
    EXPECT_EQ(segment.imports_[0].size, info.size);
}

/**
 * MmapAndUnmap_RegisterAndClearImportedGva
 *  - 验证只为远端 rank 建立 VA 映射，并在 Unmap 时清空登记。
 */
TEST_F(HybmConnBasedSegmentTest, MmapAndUnmap_RegisterAndClearImportedGva)
{
    auto options = MakeOptions(3U, 1U);
    HybmConnBasedSegment segment(options, 0);

    HostExportInfo local{};
    local.gva = 0x400000000ULL;
    local.rankId = options.rankId;
    local.size = HYBM_LARGE_PAGE_SIZE;

    HostExportInfo remote{};
    remote.gva = 0x500000000ULL;
    remote.rankId = 2U;
    remote.size = HYBM_LARGE_PAGE_SIZE;

    segment.imports_.push_back(local);
    segment.imports_.push_back(remote);

    ASSERT_EQ(segment.Mmap(), BM_OK);
    EXPECT_TRUE(segment.imports_.empty());
    EXPECT_EQ(segment.mappedGvaMem_.size(), 1U);
    EXPECT_EQ(*segment.mappedGvaMem_.begin(), remote.gva);

    auto found = HybmVaManager::GetInstance().FindAllocByVa(remote.gva, HVM_GVA);
    ASSERT_TRUE(found.second);
    EXPECT_EQ(found.first.RankId(), remote.rankId);

    ASSERT_EQ(segment.Unmap(), BM_OK);
    EXPECT_TRUE(segment.mappedGvaMem_.empty());
    EXPECT_FALSE(HybmVaManager::GetInstance().FindAllocByVa(remote.gva, HVM_GVA).second);
}

/**
 * GetMemSlice_ValidatesIdBeforeReturningSlice
 *  - 验证索引命中后还会校验 slice id，避免错误对象冒用。
 */
TEST_F(HybmConnBasedSegmentTest, GetMemSlice_ValidatesIdBeforeReturningSlice)
{
    HybmConnBasedSegment segment(MakeOptions(), 0);

    auto slice = MakeHostSlice(7U, 0x1000ULL, 0x2000ULL);
    segment.slices_.emplace(slice->index_, MemSliceStatus(slice));

    EXPECT_EQ(segment.GetMemSlice(slice->ConvertToId(), false), slice);

    auto fakeSameIndex = MakeHostSlice(slice->index_, 0x3000ULL, 0x4000ULL);
    EXPECT_EQ(segment.GetMemSlice(fakeSameIndex->ConvertToId(), true), nullptr);
    EXPECT_EQ(segment.GetMemSlice(nullptr, true), nullptr);
}

/**
 * MemoryInRange_ChecksBeginAndEndBounds
 *  - 验证范围判断既检查起点，也检查尾部是否越界。
 */
TEST_F(HybmConnBasedSegmentTest, MemoryInRange_ChecksBeginAndEndBounds)
{
    HybmConnBasedSegment segment(MakeOptions(), 0);
    std::vector<uint8_t> buffer(HYBM_LARGE_PAGE_SIZE * 2UL);

    segment.globalVirtualAddress_ = buffer.data();
    segment.totalVirtualSize_ = buffer.size();

    EXPECT_TRUE(segment.MemoryInRange(buffer.data(), buffer.size()));
    EXPECT_TRUE(segment.MemoryInRange(buffer.data() + 128UL, HYBM_LARGE_PAGE_SIZE));
    EXPECT_FALSE(segment.MemoryInRange(buffer.data() + HYBM_LARGE_PAGE_SIZE, HYBM_LARGE_PAGE_SIZE + 1UL));

    segment.globalVirtualAddress_ = nullptr;
    segment.totalVirtualSize_ = 0;
}

/**
 * FreeMemory_ReleasesSlicesAndReservedWindow
 *  - 验证释放时会清空 slices_、VA 登记和预留地址窗口。
 */
TEST_F(HybmConnBasedSegmentTest, FreeMemory_ReleasesSlicesAndReservedWindow)
{
    auto options = MakeOptions(2U, 0);
    HybmConnBasedSegment segment(options, 0);

    void *address = nullptr;
    ASSERT_EQ(segment.ReserveMemorySpace(&address), BM_OK);

    MemSlicePtr slice;
    ASSERT_EQ(segment.AllocLocalMemory(HYBM_LARGE_PAGE_SIZE, slice), BM_OK);
    ASSERT_EQ(HybmVaManager::GetInstance().GetAllocCount(), 1U);
    ASSERT_EQ(HybmVaManager::GetInstance().GetReservedCount(), 1U);

    segment.FreeMemory();

    EXPECT_TRUE(segment.slices_.empty());
    EXPECT_EQ(segment.globalVirtualAddress_, nullptr);
    EXPECT_EQ(segment.localVirtualBase_, nullptr);
    EXPECT_EQ(HybmVaManager::GetInstance().GetAllocCount(), 0U);
    EXPECT_EQ(HybmVaManager::GetInstance().GetReservedCount(), 0U);
}

/**
 * PrepareShareMemoryFd_ExpandsBackingFileToRequestedSize
 *  - 验证共享内存 fd 会被扩容到 options.size。
 */
TEST_F(HybmConnBasedSegmentTest, PrepareShareMemoryFd_ExpandsBackingFileToRequestedSize)
{
    char path[] = "/tmp/hybm_conn_based_segment_ut_XXXXXX";
    int fd = mkstemp(path);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(unlink(path), 0);

    auto options = MakeOptions();
    options.shmFd = fd;
    options.size = HYBM_LARGE_PAGE_SIZE;
    HybmConnBasedSegment segment(options, 0);

    struct stat fileStat{};
    ASSERT_EQ(fstat(fd, &fileStat), 0);
    EXPECT_EQ(fileStat.st_size, 0);

    EXPECT_EQ(segment.PrepareShareMemoryFd(), BM_OK);

    ASSERT_EQ(fstat(fd, &fileStat), 0);
    EXPECT_EQ(fileStat.st_size, static_cast<off_t>(options.size));

    close(fd);
}

/**
 * MapSlice_SizeZeroReturnsOk
 *  - 验证零长度 slice 直接返回成功，不触发实际映射。
 */
TEST_F(HybmConnBasedSegmentTest, MapSlice_SizeZeroReturnsOk)
{
    HybmConnBasedSegment segment(MakeOptions(), 0);
    void *mapped = reinterpret_cast<void *>(0x1234ULL);

    EXPECT_EQ(segment.MapSlice(mapped, nullptr, 0, 0, 0x1000ULL), BM_OK);
    EXPECT_EQ(mapped, reinterpret_cast<void *>(0x1234ULL));
}

/**
 * RemoveImported_RemovesSpecifiedRanksFromMapAndImports
 *  - 验证只清理指定 rank 的 imports_ 和 mappedGvaMem_。
 */
TEST_F(HybmConnBasedSegmentTest, RemoveImported_RemovesSpecifiedRanksFromMapAndImports)
{
    auto options = MakeOptions(4U, 0);
    HybmConnBasedSegment segment(options, 0);
    std::vector<uint8_t> buffer(options.rankCnt * options.maxSize);

    segment.globalVirtualAddress_ = buffer.data();

    auto rank1Gva = reinterpret_cast<uint64_t>(buffer.data() + options.maxSize);
    auto rank2Gva = reinterpret_cast<uint64_t>(buffer.data() + options.maxSize * 2UL);

    BaseAllocatedGvaInfo rank1Info{};
    rank1Info.va[HVM_GVA] = rank1Gva;
    rank1Info.size = HYBM_LARGE_PAGE_SIZE;
    rank1Info.memType = HYBM_MEM_TYPE_HOST;
    ASSERT_EQ(HybmVaManager::GetInstance().AddVaInfoFromExternal(rank1Info, options.rankId, 1U), BM_OK);

    BaseAllocatedGvaInfo rank2Info{};
    rank2Info.va[HVM_GVA] = rank2Gva;
    rank2Info.size = HYBM_LARGE_PAGE_SIZE;
    rank2Info.memType = HYBM_MEM_TYPE_HOST;
    ASSERT_EQ(HybmVaManager::GetInstance().AddVaInfoFromExternal(rank2Info, options.rankId, 2U), BM_OK);

    HostExportInfo import1{};
    import1.gva = rank1Gva;
    import1.rankId = 1U;
    import1.size = HYBM_LARGE_PAGE_SIZE;

    HostExportInfo import2{};
    import2.gva = rank2Gva;
    import2.rankId = 2U;
    import2.size = HYBM_LARGE_PAGE_SIZE;

    segment.imports_.push_back(import1);
    segment.imports_.push_back(import2);
    segment.mappedGvaMem_.insert(rank1Gva);
    segment.mappedGvaMem_.insert(rank2Gva);

    ASSERT_EQ(segment.RemoveImported({1U}), BM_OK);
    EXPECT_EQ(segment.imports_.size(), 1U);
    EXPECT_EQ(segment.imports_[0].rankId, 2U);
    EXPECT_EQ(segment.mappedGvaMem_.count(rank1Gva), 0U);
    EXPECT_EQ(segment.mappedGvaMem_.count(rank2Gva), 1U);
    EXPECT_FALSE(HybmVaManager::GetInstance().FindAllocByVa(rank1Gva, HVM_GVA).second);
    EXPECT_TRUE(HybmVaManager::GetInstance().FindAllocByVa(rank2Gva, HVM_GVA).second);

    EXPECT_EQ(segment.RemoveImported({options.rankCnt}), BM_INVALID_PARAM);

    segment.globalVirtualAddress_ = nullptr;
}

/**
 * RegisterMemory_AddsDeviceSliceIntoSegment
 *  - 验证 RegisterMemory 会登记一块现成地址并写入 slices_。
 */
TEST_F(HybmConnBasedSegmentTest, RegisterMemory_AddsDeviceSliceIntoSegment)
{
    HybmConnBasedSegment segment(MakeOptions(), 0);
    auto *deviceAddr = reinterpret_cast<void *>(HYBM_HBM_START_ADDR + HYBM_LARGE_PAGE_SIZE);

    MemSlicePtr slice;
    ASSERT_EQ(segment.RegisterMemory(deviceAddr, 4096ULL, slice), BM_OK);
    ASSERT_NE(slice, nullptr);
    EXPECT_EQ(slice->memType_, HYBM_MEM_TYPE_DEVICE);
    EXPECT_EQ(segment.slices_.count(slice->index_), 1U);

    auto found = HybmVaManager::GetInstance().FindAllocByVa(reinterpret_cast<uint64_t>(deviceAddr), HVM_HVA);
    ASSERT_TRUE(found.second);
    EXPECT_EQ(found.first.base.va[HVM_HVA], reinterpret_cast<uint64_t>(deviceAddr));
}

/**
 * ReleaseSliceMemory_RemovesRegisteredSliceAndVaInfo
 *  - 验证释放已登记 slice 时会同步清理 slices_ 和 VA 管理器。
 */
TEST_F(HybmConnBasedSegmentTest, ReleaseSliceMemory_RemovesRegisteredSliceAndVaInfo)
{
    HybmConnBasedSegment segment(MakeOptions(), 0);
    auto *deviceAddr = reinterpret_cast<void *>(HYBM_HBM_START_ADDR + HYBM_LARGE_PAGE_SIZE * 2UL);

    MemSlicePtr slice;
    ASSERT_EQ(segment.RegisterMemory(deviceAddr, 4096ULL, slice), BM_OK);
    ASSERT_TRUE(HybmVaManager::GetInstance().FindAllocByVa(reinterpret_cast<uint64_t>(deviceAddr), HVM_HVA).second);

    EXPECT_EQ(segment.ReleaseSliceMemory(slice), BM_OK);
    EXPECT_TRUE(segment.slices_.empty());
    EXPECT_FALSE(HybmVaManager::GetInstance().FindAllocByVa(reinterpret_cast<uint64_t>(deviceAddr), HVM_HVA).second);
    EXPECT_EQ(segment.ReleaseSliceMemory(slice), BM_INVALID_PARAM);
}

/**
 * GetExportSliceSize_ReturnsHostExportInfoSize
 *  - 验证导出描述大小与 HostExportInfo 结构体保持一致。
 */
TEST_F(HybmConnBasedSegmentTest, GetExportSliceSize_ReturnsHostExportInfoSize)
{
    HybmConnBasedSegment segment(MakeOptions(), 0);
    size_t size = 0;

    EXPECT_EQ(segment.GetExportSliceSize(size), BM_OK);
    EXPECT_EQ(size, sizeof(HostExportInfo));
}
