/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#define private   public
#define protected public
#include "hybm_vmm_based_segment.h"
#undef private
#undef protected

#include "hybm_va_manager.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace ock::mf;

namespace {
int HalMemImportOk(drv_mem_handle_type, MemShareHandle *, uint32_t, drv_mem_handle_t **handle)
{
    *handle = reinterpret_cast<drv_mem_handle_t *>(0x1234);
    return BM_OK;
}

int HalMemMapOk(void *, size_t, size_t, drv_mem_handle_t *, uint64_t)
{
    return BM_OK;
}

int HalMemMapFail(void *, size_t, size_t, drv_mem_handle_t *, uint64_t)
{
    return BM_ERROR;
}

int HalMemReleaseOk(drv_mem_handle_t *)
{
    return BM_OK;
}

int HalMemUnmapOk(void *)
{
    return BM_OK;
}
} // namespace

class HybmVmmBasedSegmentTest : public testing::Test {
protected:
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
* Mmap_RemoteSlice_SucceedsAndSkipsLocal
*  - 远端可达时完成映射；本 rank 记录跳过。
*/
TEST_F(HybmVmmBasedSegmentTest, Mmap_RemoteSlice_SucceedsAndSkipsLocal)
{
    MemSegmentOptions opt{};
    opt.segType = HYBM_MST_DRAM;
    opt.maxSize = HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 2UL;
    opt.rankId = 0;
    opt.shared = true;

    HybmVmmBasedSegment seg(opt, 0);
    HybmVaManager::GetInstance().AllocReserveGva(opt.rankId, opt.maxSize * opt.rankCnt, opt.maxSize * opt.rankCnt,
                                                 HYBM_MEM_TYPE_HOST);

    HostSdmaExportInfo local{};
    local.gva = 0x1000;
    local.rankId = 0;
    local.size = HYBM_LARGE_PAGE_SIZE;
    local.segmentType = SEGMENT_TYPE_VMM;

    HostSdmaExportInfo remote{};
    remote.gva = opt.maxSize;
    remote.deviceVa = opt.maxSize;
    remote.rankId = 1;
    remote.size = HYBM_LARGE_PAGE_SIZE;
    remote.segmentType = SEGMENT_TYPE_VMM;
    remote.magic = DRAM_SLICE_EXPORT_INFO_MAGIC;
    remote.superPodId = 1;
    remote.serverId = 1;

    seg.imports_ = {local, remote};

    MOCKER_CPP(&MemSegment::CanSdmaReaches, bool (*)(MemSegment *, uint32_t, uint32_t, uint32_t))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&DlHalApi::HalMemImport, int (*)(drv_mem_handle_type, MemShareHandle *, uint32_t, drv_mem_handle_t **))
        .stubs()
        .will(invoke(HalMemImportOk));
    MOCKER_CPP(&DlHalApi::HalMemMap, int (*)(void *, size_t, size_t, drv_mem_handle_t *, uint64_t))
        .stubs()
        .will(invoke(HalMemMapOk));
    MOCKER_CPP(&DlHalApi::HalMemRelease, int (*)(drv_mem_handle_t *)).stubs().will(invoke(HalMemReleaseOk));
    MOCKER_CPP(&DlHalApi::HalMemUnmap, int (*)(void *)).stubs().will(invoke(HalMemUnmapOk));

    auto ret = seg.Mmap();
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(seg.imports_.empty());
    EXPECT_EQ(seg.mappedGvaMem_.size(), 1U);
    EXPECT_TRUE(seg.mappedGvaMem_.count(remote.gva) == 1U);
    EXPECT_EQ(HybmVaManager::GetInstance().GetAllocCount(), 1U);
}

/**
* Mmap_MapFailure_RollsBackVaInfo
*  - 后续 HalMemMap 失败时，已登记的外部 VA 应回滚。
*/
TEST_F(HybmVmmBasedSegmentTest, Mmap_MapFailure_RollsBackVaInfo)
{
    MemSegmentOptions opt{};
    opt.segType = HYBM_MST_DRAM;
    opt.maxSize = HYBM_LARGE_PAGE_SIZE;
    opt.rankCnt = 2UL;
    opt.rankId = 0;
    opt.shared = true;

    HybmVmmBasedSegment seg(opt, 0);
    HybmVaManager::GetInstance().AllocReserveGva(opt.rankId, opt.maxSize * opt.rankCnt, opt.maxSize * opt.rankCnt,
                                                 HYBM_MEM_TYPE_HOST);

    HostSdmaExportInfo remote{};
    remote.gva = opt.maxSize;
    remote.deviceVa = opt.maxSize;
    remote.rankId = 1;
    remote.size = HYBM_LARGE_PAGE_SIZE;
    remote.segmentType = SEGMENT_TYPE_VMM;
    remote.magic = DRAM_SLICE_EXPORT_INFO_MAGIC;
    remote.superPodId = 1;
    remote.serverId = 1;

    seg.imports_.push_back(remote);

    MOCKER_CPP(&MemSegment::CanSdmaReaches, bool (*)(MemSegment *, uint32_t, uint32_t, uint32_t))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&DlHalApi::HalMemImport, int (*)(drv_mem_handle_type, MemShareHandle *, uint32_t, drv_mem_handle_t **))
        .stubs()
        .will(invoke(HalMemImportOk));
    MOCKER_CPP(&DlHalApi::HalMemMap, int (*)(void *, size_t, size_t, drv_mem_handle_t *, uint64_t))
        .stubs()
        .will(invoke(HalMemMapFail));
    MOCKER_CPP(&DlHalApi::HalMemRelease, int (*)(drv_mem_handle_t *)).stubs().will(invoke(HalMemReleaseOk));

    auto ret = seg.Mmap();
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_TRUE(seg.mappedGvaMem_.empty());
    EXPECT_EQ(HybmVaManager::GetInstance().GetAllocCount(), 0U);
}
