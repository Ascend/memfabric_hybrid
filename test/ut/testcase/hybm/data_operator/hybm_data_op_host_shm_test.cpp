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

#include <vector>

#include "hybm_data_op_host_shm.h"
#include "dl_hybrid_api.h"

using namespace ock::mf;

namespace {
constexpr size_t H2H_COPY_COUNT = 3U;
constexpr size_t H2H_BATCH_COPY_COUNT = 3U;

struct MemcpyStubState {
    uint32_t callCount = 0U;
    std::vector<uint32_t> kinds;
    std::vector<void *> dsts;
    std::vector<const void *> srcs;
    std::vector<size_t> destMaxes;
    std::vector<size_t> counts;
    uint32_t failOnCall = 0U;
    ock::mf::Result failResult = BM_ERROR;
};

MemcpyStubState g_memCpyState;

ock::mf::Result MemcpyInvokeStub(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind)
{
    ++g_memCpyState.callCount;
    g_memCpyState.kinds.push_back(kind);
    g_memCpyState.dsts.push_back(dst);
    g_memCpyState.srcs.push_back(src);
    g_memCpyState.destMaxes.push_back(destMax);
    g_memCpyState.counts.push_back(count);
    if (g_memCpyState.failOnCall > 0U && g_memCpyState.callCount == g_memCpyState.failOnCall) {
        return g_memCpyState.failResult;
    }
    return BM_OK;
}

void ResetMemcpyState()
{
    g_memCpyState.callCount = 0U;
    g_memCpyState.kinds.clear();
    g_memCpyState.dsts.clear();
    g_memCpyState.srcs.clear();
    g_memCpyState.destMaxes.clear();
    g_memCpyState.counts.clear();
    g_memCpyState.failOnCall = 0U;
    g_memCpyState.failResult = BM_ERROR;
}
} // namespace

class HybmDataOpHostShmTest : public testing::Test {
protected:
    void SetUp() override
    {
        GlobalMockObject::reset();
        ResetMemcpyState();
        MOCKER(&ock::mf::DlHybridApi::Memcpy).stubs().will(invoke(MemcpyInvokeStub));
        dataOp_ = std::make_shared<HostDataOpHostShm>(0U);
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
        ResetMemcpyState();
    }

    std::shared_ptr<HostDataOpHostShm> dataOp_;
};

TEST_F(HybmDataOpHostShmTest, InitializeUnInitializeAndWait_BasicBehavior)
{
    EXPECT_EQ(dataOp_->Wait(0), BM_NOT_INITIALIZED);
    EXPECT_EQ(dataOp_->Initialize(), BM_OK);
    EXPECT_EQ(dataOp_->Wait(0), BM_OK);
    dataOp_->UnInitialize();
    EXPECT_EQ(dataOp_->Wait(0), BM_NOT_INITIALIZED);
}

TEST_F(HybmDataOpHostShmTest, DataCopy_NotInitializedAndInvalidDirection)
{
    uint8_t src[8] = {0};
    uint8_t dst[8] = {0};
    hybm_copy_params params{};
    params.src = src;
    params.dest = dst;
    params.dataSize = sizeof(src);
    ExtOptions options{};

    EXPECT_EQ(dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options), BM_NOT_INITIALIZED);

    ASSERT_EQ(dataOp_->Initialize(), BM_OK);
    auto invalid = static_cast<hybm_data_copy_direction>(HYBM_DATA_COPY_DIRECTION_BUTT);
    EXPECT_EQ(dataOp_->DataCopy(params, invalid, options), BM_INVALID_PARAM);
    EXPECT_EQ(g_memCpyState.callCount, 0U);
}

TEST_F(HybmDataOpHostShmTest, DataCopy_SupportedDirectionsDispatchMemcpyKinds)
{
    ASSERT_EQ(dataOp_->Initialize(), BM_OK);

    uint8_t src[16] = {0};
    uint8_t dst[16] = {0};
    hybm_copy_params params{};
    params.src = src;
    params.dest = dst;
    params.dataSize = sizeof(src);
    ExtOptions options{};

    ASSERT_EQ(dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options), BM_OK);
    ASSERT_EQ(dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options), BM_OK);
    ASSERT_EQ(dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, options), BM_OK);
    ASSERT_EQ(dataOp_->DataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, options), BM_OK);
    ASSERT_EQ(dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options), BM_OK);

    ASSERT_EQ(g_memCpyState.callCount, 5U);
    ASSERT_EQ(g_memCpyState.dsts.size(), 5U);
    ASSERT_EQ(g_memCpyState.srcs.size(), 5U);
    ASSERT_EQ(g_memCpyState.destMaxes.size(), 5U);
    ASSERT_EQ(g_memCpyState.counts.size(), 5U);
    constexpr size_t firstH2hIndex = 0U;
    constexpr size_t secondH2hIndex = 1U;
    constexpr size_t thirdH2hIndex = 2U;
    EXPECT_EQ(g_memCpyState.kinds[firstH2hIndex], ACL_MEMCPY_HOST_TO_HOST);
    EXPECT_EQ(g_memCpyState.kinds[secondH2hIndex], ACL_MEMCPY_HOST_TO_HOST);
    EXPECT_EQ(g_memCpyState.kinds[thirdH2hIndex], ACL_MEMCPY_HOST_TO_HOST);
    constexpr size_t deviceToHostIndex = H2H_COPY_COUNT;
    constexpr size_t hostToDeviceIndex = H2H_COPY_COUNT + 1U;
    EXPECT_EQ(g_memCpyState.kinds[deviceToHostIndex], ACL_MEMCPY_DEVICE_TO_HOST);
    EXPECT_EQ(g_memCpyState.kinds[hostToDeviceIndex], ACL_MEMCPY_HOST_TO_DEVICE);
    for (size_t i = 0; i < g_memCpyState.callCount; ++i) {
        EXPECT_EQ(g_memCpyState.dsts[i], static_cast<void *>(dst));
        EXPECT_EQ(g_memCpyState.srcs[i], static_cast<const void *>(src));
        EXPECT_EQ(g_memCpyState.destMaxes[i], sizeof(dst));
        EXPECT_EQ(g_memCpyState.counts[i], sizeof(src));
    }
}

TEST_F(HybmDataOpHostShmTest, DataCopyAsync_ReturnCodesByInitializationState)
{
    uint8_t src[8] = {0};
    uint8_t dst[8] = {0};
    hybm_copy_params params{};
    params.src = src;
    params.dest = dst;
    params.dataSize = sizeof(src);
    ExtOptions options{};

    EXPECT_EQ(dataOp_->DataCopyAsync(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options), BM_NOT_INITIALIZED);

    ASSERT_EQ(dataOp_->Initialize(), BM_OK);
    EXPECT_EQ(dataOp_->DataCopyAsync(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options), BM_ERROR);
}

TEST_F(HybmDataOpHostShmTest, BatchDataCopy_NotInitializedAndInvalidDirection)
{
    uint8_t src0[4] = {0};
    uint8_t dst0[4] = {0};
    void *sources[1] = {src0};
    void *destinations[1] = {dst0};
    uint64_t dataSizes[1] = {sizeof(src0)};

    hybm_batch_copy_params params{};
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;
    params.batchSize = 1U;
    ExtOptions options{};

    EXPECT_EQ(dataOp_->BatchDataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options), BM_NOT_INITIALIZED);

    ASSERT_EQ(dataOp_->Initialize(), BM_OK);
    auto invalid = static_cast<hybm_data_copy_direction>(HYBM_DATA_COPY_DIRECTION_BUTT);
    EXPECT_EQ(dataOp_->BatchDataCopy(params, invalid, options), BM_INVALID_PARAM);
    EXPECT_EQ(g_memCpyState.callCount, 0U);
}

TEST_F(HybmDataOpHostShmTest, BatchDataCopy_SuccessIteratesAllEntries)
{
    ASSERT_EQ(dataOp_->Initialize(), BM_OK);

    uint8_t src0[4] = {0};
    uint8_t src1[4] = {1};
    uint8_t src2[4] = {2};
    uint8_t dst0[4] = {0};
    uint8_t dst1[4] = {0};
    uint8_t dst2[4] = {0};
    void *sources[3] = {src0, src1, src2};
    void *destinations[3] = {dst0, dst1, dst2};
    uint64_t dataSizes[3] = {sizeof(src0), sizeof(src1), sizeof(src2)};

    hybm_batch_copy_params params{};
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;
    params.batchSize = 3U;
    ExtOptions options{};

    EXPECT_EQ(dataOp_->BatchDataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options), BM_OK);
    EXPECT_EQ(g_memCpyState.callCount, 3U);
    EXPECT_EQ(g_memCpyState.kinds.size(), 3U);
    ASSERT_EQ(g_memCpyState.dsts.size(), 3U);
    ASSERT_EQ(g_memCpyState.srcs.size(), 3U);
    ASSERT_EQ(g_memCpyState.destMaxes.size(), 3U);
    ASSERT_EQ(g_memCpyState.counts.size(), 3U);
    EXPECT_EQ(g_memCpyState.kinds[0], ACL_MEMCPY_HOST_TO_HOST);
    EXPECT_EQ(g_memCpyState.kinds[1], ACL_MEMCPY_HOST_TO_HOST);
    constexpr size_t lastBatchIndex = H2H_BATCH_COPY_COUNT - 1U;
    EXPECT_EQ(g_memCpyState.kinds[lastBatchIndex], ACL_MEMCPY_HOST_TO_HOST);
    for (size_t i = 0; i < g_memCpyState.callCount; ++i) {
        EXPECT_EQ(g_memCpyState.dsts[i], destinations[i]);
        EXPECT_EQ(g_memCpyState.srcs[i], static_cast<const void *>(sources[i]));
        EXPECT_EQ(g_memCpyState.destMaxes[i], dataSizes[i]);
        EXPECT_EQ(g_memCpyState.counts[i], dataSizes[i]);
    }
}

TEST_F(HybmDataOpHostShmTest, BatchDataCopy_StopsEarlyWhenCopyFails)
{
    ASSERT_EQ(dataOp_->Initialize(), BM_OK);

    uint8_t src0[4] = {0};
    uint8_t src1[4] = {1};
    uint8_t src2[4] = {2};
    uint8_t dst0[4] = {0};
    uint8_t dst1[4] = {0};
    uint8_t dst2[4] = {0};
    void *sources[3] = {src0, src1, src2};
    void *destinations[3] = {dst0, dst1, dst2};
    uint64_t dataSizes[3] = {sizeof(src0), sizeof(src1), sizeof(src2)};

    hybm_batch_copy_params params{};
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;
    params.batchSize = 3U;
    ExtOptions options{};

    g_memCpyState.failOnCall = 2U;
    g_memCpyState.failResult = BM_ERROR;

    EXPECT_EQ(dataOp_->BatchDataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options), BM_ERROR);
    EXPECT_EQ(g_memCpyState.callCount, 2U);
    EXPECT_EQ(g_memCpyState.kinds.size(), 2U);
    ASSERT_EQ(g_memCpyState.dsts.size(), 2U);
    ASSERT_EQ(g_memCpyState.srcs.size(), 2U);
    ASSERT_EQ(g_memCpyState.destMaxes.size(), 2U);
    ASSERT_EQ(g_memCpyState.counts.size(), 2U);
    EXPECT_EQ(g_memCpyState.kinds[0], ACL_MEMCPY_HOST_TO_HOST);
    EXPECT_EQ(g_memCpyState.kinds[1], ACL_MEMCPY_HOST_TO_HOST);
    EXPECT_EQ(g_memCpyState.dsts[0], destinations[0]);
    EXPECT_EQ(g_memCpyState.srcs[0], static_cast<const void *>(sources[0]));
    EXPECT_EQ(g_memCpyState.destMaxes[0], dataSizes[0]);
    EXPECT_EQ(g_memCpyState.counts[0], dataSizes[0]);
    EXPECT_EQ(g_memCpyState.dsts[1], destinations[1]);
    EXPECT_EQ(g_memCpyState.srcs[1], static_cast<const void *>(sources[1]));
    EXPECT_EQ(g_memCpyState.destMaxes[1], dataSizes[1]);
    EXPECT_EQ(g_memCpyState.counts[1], dataSizes[1]);
}
