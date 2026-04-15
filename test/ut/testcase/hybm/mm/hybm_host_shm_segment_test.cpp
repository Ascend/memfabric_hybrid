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

#include <limits>
#include <vector>

#define private   public
#define protected public
#include "hybm_host_shm_segment.h"
#undef private
#undef protected

#include "hybm_ex_info_transfer.h"

using namespace ock::mf;

class HybmHostShmSegmentTest : public testing::Test {
protected:
    static MemSegmentOptions MakeOptions(uint64_t size = HYBM_LARGE_PAGE_SIZE * 2UL, uint32_t rankCnt = 3U,
                                         uint32_t rankId = 1U)
    {
        MemSegmentOptions options{};
        options.segType = HYBM_MST_DRAM;
        options.size = size;
        options.maxSize = size;
        options.rankCnt = rankCnt;
        options.rankId = rankId;
        return options;
    }

    static void SetManualMemoryWindow(HybmHostShmSegment &segment, std::vector<uint8_t> &window)
    {
        segment.globalVirtualAddress_ = window.data();
        segment.totalVirtualSize_ = window.size();
        segment.localVirtualBase_ = segment.globalVirtualAddress_ + segment.options_.size * segment.options_.rankId;
    }

    static void ResetManualMemoryWindow(HybmHostShmSegment &segment)
    {
        segment.globalVirtualAddress_ = nullptr;
        segment.totalVirtualSize_ = 0;
        segment.localVirtualBase_ = nullptr;
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

TEST_F(HybmHostShmSegmentTest, ValidateOptions_CoversSuccessAndInvalidBranches)
{
    auto options = MakeOptions();
    HybmHostShmSegment segment(options, 0);
    EXPECT_EQ(segment.ValidateOptions(), BM_OK);

    options = MakeOptions();
    options.segType = HYBM_MST_HBM;
    HybmHostShmSegment badType(options, 0);
    EXPECT_EQ(badType.ValidateOptions(), BM_INVALID_PARAM);

    options = MakeOptions();
    options.size = 0;
    HybmHostShmSegment zeroSize(options, 0);
    EXPECT_EQ(zeroSize.ValidateOptions(), BM_INVALID_PARAM);

    options = MakeOptions();
    options.size = HYBM_LARGE_PAGE_SIZE / 2UL;
    HybmHostShmSegment misalignedSize(options, 0);
    EXPECT_EQ(misalignedSize.ValidateOptions(), BM_INVALID_PARAM);

    options = MakeOptions((1ULL << 63U), 2U, 0U);
    HybmHostShmSegment overflow(options, 0);
    EXPECT_EQ(overflow.ValidateOptions(), BM_INVALID_PARAM);
}

TEST_F(HybmHostShmSegmentTest, AllocLocalMemory_CoversValidUnalignedAndOverflow)
{
    auto options = MakeOptions(HYBM_LARGE_PAGE_SIZE * 2UL, 2U, 1U);
    HybmHostShmSegment segment(options, 0);
    std::vector<uint8_t> window(options.size * options.rankCnt, 0U);
    SetManualMemoryWindow(segment, window);

    MemSlicePtr slice;
    ASSERT_EQ(segment.AllocLocalMemory(HYBM_LARGE_PAGE_SIZE, slice), BM_OK);
    ASSERT_NE(slice, nullptr);
    EXPECT_EQ(slice->index_, 0U);
    EXPECT_EQ(slice->size_, HYBM_LARGE_PAGE_SIZE);
    EXPECT_EQ(slice->vAddress_, reinterpret_cast<uint64_t>(segment.localVirtualBase_));
    EXPECT_EQ(slice->gva_, reinterpret_cast<uint64_t>(segment.globalVirtualAddress_) + options.size * options.rankId);

    MemSlicePtr unalignedSlice;
    EXPECT_EQ(segment.AllocLocalMemory(HYBM_LARGE_PAGE_SIZE / 2UL, unalignedSlice), BM_INVALID_PARAM);

    MemSlicePtr overflowSlice;
    EXPECT_EQ(segment.AllocLocalMemory(HYBM_LARGE_PAGE_SIZE * 2UL, overflowSlice), BM_INVALID_PARAM);

    ResetManualMemoryWindow(segment);
}

TEST_F(HybmHostShmSegmentTest, ExportSlice_CoversNullUnknownSerializeAndCacheReuse)
{
    auto options = MakeOptions(HYBM_LARGE_PAGE_SIZE * 2UL, 2U, 1U);
    HybmHostShmSegment segment(options, 0);
    std::vector<uint8_t> window(options.size * options.rankCnt, 0U);
    SetManualMemoryWindow(segment, window);

    std::string exInfo;
    EXPECT_EQ(segment.Export(nullptr, exInfo), BM_INVALID_PARAM);

    auto unknownSlice =
        std::make_shared<MemSlice>(9U, HYBM_MEM_TYPE_HOST, MEM_PT_TYPE_SVM, reinterpret_cast<uint64_t>(window.data()),
                                   reinterpret_cast<uint64_t>(window.data()), HYBM_LARGE_PAGE_SIZE);
    EXPECT_EQ(segment.Export(unknownSlice, exInfo), BM_INVALID_PARAM);

    MemSlicePtr localSlice;
    ASSERT_EQ(segment.AllocLocalMemory(HYBM_LARGE_PAGE_SIZE, localSlice), BM_OK);
    ASSERT_NE(localSlice, nullptr);

    ASSERT_EQ(segment.Export(localSlice, exInfo), BM_OK);
    ASSERT_EQ(segment.exportMap_.count(localSlice->index_), 1U);

    ShmExportInfo info{};
    ASSERT_EQ(LiteralExInfoTranslater<ShmExportInfo>{}.Deserialize(exInfo, info), BM_OK);
    EXPECT_EQ(info.magic, DRAM_SLICE_EXPORT_INFO_MAGIC);
    EXPECT_EQ(info.version, EXPORT_INFO_VERSION);
    EXPECT_EQ(info.mappingOffset, 0U);
    EXPECT_EQ(info.sliceIndex, localSlice->index_);
    EXPECT_EQ(info.rankId, options.rankId);
    EXPECT_EQ(info.size, localSlice->size_);
    EXPECT_EQ(info.pageTblType, MEM_PT_TYPE_SVM);
    EXPECT_EQ(info.memSegType, HYBM_MST_DRAM);
    EXPECT_EQ(info.exchangeType, HYBM_INFO_EXG_IN_NODE);
    EXPECT_FALSE(info.useHugetlbfs);

    std::string cached = "cache_miss";
    ASSERT_EQ(segment.Export(localSlice, cached), BM_OK);
    EXPECT_EQ(cached, exInfo);

    ResetManualMemoryWindow(segment);
}

TEST_F(HybmHostShmSegmentTest, Import_ReturnsInvalidParamWhenDeserializeFails)
{
    HybmHostShmSegment segment(MakeOptions(), 0);
    std::vector<std::string> allExInfo{"bad_format"};

    EXPECT_EQ(segment.Import(allExInfo, nullptr), BM_INVALID_PARAM);
}

TEST_F(HybmHostShmSegmentTest, Import_ReturnsInvalidParamWhenMagicIsInvalid)
{
    HybmHostShmSegment segment(MakeOptions(), 0);

    ShmExportInfo info{};
    info.magic = 0x1234ULL;
    info.rankId = 0U;
    std::string exInfo;
    ASSERT_EQ(LiteralExInfoTranslater<ShmExportInfo>{}.Serialize(info, exInfo), BM_OK);

    EXPECT_EQ(segment.Import({exInfo}, nullptr), BM_INVALID_PARAM);
}

TEST_F(HybmHostShmSegmentTest, Import_DeduplicatesDuplicateRanksIntoImports)
{
    HybmHostShmSegment segment(MakeOptions(), 0);

    ShmExportInfo first{};
    first.rankId = 0U;
    first.sliceIndex = 1U;
    first.mappingOffset = 128U;
    first.size = HYBM_LARGE_PAGE_SIZE;
    first.useHugetlbfs = false;

    ShmExportInfo duplicate = first;
    duplicate.sliceIndex = 2U;
    duplicate.mappingOffset = 256U;
    duplicate.useHugetlbfs = true;

    ShmExportInfo second{};
    second.rankId = 2U;
    second.sliceIndex = 3U;
    second.mappingOffset = 512U;
    second.size = HYBM_LARGE_PAGE_SIZE;
    second.useHugetlbfs = true;

    std::string ex1;
    std::string ex2;
    std::string ex3;
    ASSERT_EQ(LiteralExInfoTranslater<ShmExportInfo>{}.Serialize(first, ex1), BM_OK);
    ASSERT_EQ(LiteralExInfoTranslater<ShmExportInfo>{}.Serialize(duplicate, ex2), BM_OK);
    ASSERT_EQ(LiteralExInfoTranslater<ShmExportInfo>{}.Serialize(second, ex3), BM_OK);

    ASSERT_EQ(segment.Import({ex1, ex2, ex3}, nullptr), BM_OK);
    ASSERT_EQ(segment.imports_.size(), 2U);
    EXPECT_EQ(segment.imports_[0].rankId, 0U);
    EXPECT_EQ(segment.imports_[0].sliceIndex, 1U);
    EXPECT_EQ(segment.imports_[1].rankId, 2U);
    EXPECT_EQ(segment.imports_[1].sliceIndex, 3U);
    EXPECT_EQ(segment.importedHugetlbfsFlags_.at(0U), false);
    EXPECT_EQ(segment.importedHugetlbfsFlags_.at(2U), true);
}

TEST_F(HybmHostShmSegmentTest, ReleaseSliceMemory_CoversNullUnknownAndValid)
{
    auto options = MakeOptions(HYBM_LARGE_PAGE_SIZE * 2UL, 2U, 1U);
    HybmHostShmSegment segment(options, 0);
    std::vector<uint8_t> window(options.size * options.rankCnt, 0U);
    SetManualMemoryWindow(segment, window);

    EXPECT_EQ(segment.ReleaseSliceMemory(nullptr), BM_INVALID_PARAM);

    auto unknownSlice =
        std::make_shared<MemSlice>(100U, HYBM_MEM_TYPE_HOST, MEM_PT_TYPE_SVM, 0U, 0U, HYBM_LARGE_PAGE_SIZE);
    EXPECT_EQ(segment.ReleaseSliceMemory(unknownSlice), BM_INVALID_PARAM);

    MemSlicePtr localSlice;
    ASSERT_EQ(segment.AllocLocalMemory(HYBM_LARGE_PAGE_SIZE, localSlice), BM_OK);
    ASSERT_NE(localSlice, nullptr);
    EXPECT_EQ(segment.ReleaseSliceMemory(localSlice), BM_OK);

    ResetManualMemoryWindow(segment);
}

TEST_F(HybmHostShmSegmentTest, GetExportSliceSize_ReturnsShmExportInfoSize)
{
    HybmHostShmSegment segment(MakeOptions(), 0);
    size_t size = 0U;

    EXPECT_EQ(segment.GetExportSliceSize(size), BM_OK);
    EXPECT_EQ(size, sizeof(ShmExportInfo));
}
