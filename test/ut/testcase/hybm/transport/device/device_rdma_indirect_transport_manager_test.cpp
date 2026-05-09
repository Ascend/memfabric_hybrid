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

#include <cstdint>
#include <cstring>
#include <thread>
#include <chrono>

#include "hybm_types.h"

#define private public
#define protected public
#include "device/device_rdma_indirect_transport_manager.h"
#include "dl_acl_api.h"
#include "dl_hccp_api.h"
#undef private
#undef protected

using namespace ock::mf;
using namespace ock::mf::transport;
using namespace ock::mf::transport::device;

namespace {
constexpr uint64_t TEST_SRC_ADDR = 0x1000ULL;
constexpr uint64_t TEST_DST_ADDR = 0x2000ULL;
constexpr uint64_t TEST_FAR_ADDR = 0x5000ULL;
constexpr uint64_t TEST_ALT_DST_ADDR = 0x3000ULL;
constexpr uint64_t TEST_ALT_SRC_ADDR = 0x9000ULL;
constexpr uint64_t TEST_LARGE_SRC_BASE = 0x100000ULL;
constexpr uint64_t TEST_LARGE_DST_BASE = 0x200000ULL;
constexpr uint64_t TEST_MEM_REGION_SIZE = 0x2000ULL;
constexpr uint64_t TEST_SIZE_1 = 1024ULL;
constexpr uint64_t TEST_SIZE_2 = 2048ULL;
constexpr uint64_t TEST_SIZE_3 = 4096ULL;
constexpr uint32_t TEST_INVALID_RANK_ID_1 = 5U;
constexpr uint32_t TEST_INVALID_RANK_ID_2 = 10U;
constexpr uint32_t TEST_DST_RANK_ID = 3U;
constexpr uint16_t TEST_LOCAL_PORT = 12345U;
constexpr uint64_t TEST_ENQUEUE_TIME = 12345ULL;
constexpr uint64_t TEST_REQUEST_ID_INIT = 100ULL;
constexpr int TEST_NUM_ENTRIES = 100;
constexpr int TEST_NUM_SLICES = 5;
constexpr int TEST_SLEEP_MS_SHORT = 50;
constexpr int TEST_SLEEP_MS_MEDIUM = 100;
constexpr int TEST_TIMEOUT_SECONDS = 2;
const char* const TEST_NIC_ADDR_LOCAL = "tcp://127.0.0.1:1234";
const char* const TEST_NIC_ADDR_REMOTE = "tcp://192.168.1.10:9000";
constexpr int TEST_INITIAL_PENDING_COUNT = 3;
constexpr uint32_t TEST_RANK_COUNT = 2U;
constexpr uint32_t TEST_LOCAL_RANK_ID_ALT = 2U;
constexpr int TEST_SYNC_PENDING_COUNT = 2;
constexpr int TEST_COUNT_MINUS_TWO = -2;
}

class RdmaIndirectTransportManagerTest : public testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(RdmaIndirectTransportManagerTest, MergeBatchCopyEmptyInputReturnsError)
{
    RdmaIndirectTransportManager mgr;
    std::vector<void *> srcAddrs;
    std::vector<void *> dstAddrs;
    std::vector<uint64_t> counts;
    RdmaIndirectTransportManager::MergeResult result;

    auto ret = mgr.MergeBatchCopy(srcAddrs, dstAddrs, counts, result);
    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(result.mergedSrc.empty());
    EXPECT_TRUE(result.mergedDst.empty());
    EXPECT_TRUE(result.mergedCounts.empty());
}

TEST_F(RdmaIndirectTransportManagerTest, MergeBatchCopySizeMismatchReturnsError)
{
    RdmaIndirectTransportManager mgr;
    std::vector<void *> srcAddrs = {reinterpret_cast<void *>(TEST_SRC_ADDR)};
    std::vector<void *> dstAddrs;
    std::vector<uint64_t> counts = {TEST_SIZE_1};
    RdmaIndirectTransportManager::MergeResult result;

    auto ret = mgr.MergeBatchCopy(srcAddrs, dstAddrs, counts, result);
    EXPECT_EQ(ret, -1);
}

TEST_F(RdmaIndirectTransportManagerTest, MergeBatchCopySingleEntryNoMerge)
{
    RdmaIndirectTransportManager mgr;
    std::vector<void *> srcAddrs = {reinterpret_cast<void *>(TEST_SRC_ADDR)};
    std::vector<void *> dstAddrs = {reinterpret_cast<void *>(TEST_DST_ADDR)};
    std::vector<uint64_t> counts = {TEST_SIZE_1};
    RdmaIndirectTransportManager::MergeResult result;

    auto ret = mgr.MergeBatchCopy(srcAddrs, dstAddrs, counts, result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.mergedSrc.size(), 1U);
    EXPECT_EQ(result.mergedDst.size(), 1U);
    EXPECT_EQ(result.mergedCounts.size(), 1U);
    EXPECT_EQ(result.mergedSrc[0], reinterpret_cast<void *>(TEST_SRC_ADDR));
    EXPECT_EQ(result.mergedDst[0], reinterpret_cast<void *>(TEST_DST_ADDR));
    EXPECT_EQ(result.mergedCounts[0], TEST_SIZE_1);
}

TEST_F(RdmaIndirectTransportManagerTest, MergeBatchCopyContinuousEntriesMerged)
{
    RdmaIndirectTransportManager mgr;
    std::vector<void *> srcAddrs = {
        reinterpret_cast<void *>(TEST_SRC_ADDR),
        reinterpret_cast<void *>(TEST_SRC_ADDR + TEST_SIZE_1),
        reinterpret_cast<void *>(TEST_SRC_ADDR + TEST_SIZE_1 + TEST_SIZE_2)
    };
    std::vector<void *> dstAddrs = {
        reinterpret_cast<void *>(TEST_DST_ADDR),
        reinterpret_cast<void *>(TEST_DST_ADDR + TEST_SIZE_1),
        reinterpret_cast<void *>(TEST_DST_ADDR + TEST_SIZE_1 + TEST_SIZE_2)
    };
    std::vector<uint64_t> counts = {TEST_SIZE_1, TEST_SIZE_2, TEST_SIZE_3};
    RdmaIndirectTransportManager::MergeResult result;

    auto ret = mgr.MergeBatchCopy(srcAddrs, dstAddrs, counts, result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.mergedSrc.size(), 1U);
    EXPECT_EQ(result.mergedCounts[0], TEST_SIZE_1 + TEST_SIZE_2 + TEST_SIZE_3);
}

TEST_F(RdmaIndirectTransportManagerTest, MergeBatchCopySrcNotContinuousSplit)
{
    RdmaIndirectTransportManager mgr;
    std::vector<void *> srcAddrs = {
        reinterpret_cast<void *>(TEST_SRC_ADDR),
        reinterpret_cast<void *>(TEST_FAR_ADDR)
    };
    std::vector<void *> dstAddrs = {
        reinterpret_cast<void *>(TEST_DST_ADDR),
        reinterpret_cast<void *>(TEST_DST_ADDR + TEST_SIZE_1)
    };
    std::vector<uint64_t> counts = {TEST_SIZE_1, TEST_SIZE_2};
    RdmaIndirectTransportManager::MergeResult result;

    auto ret = mgr.MergeBatchCopy(srcAddrs, dstAddrs, counts, result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.mergedSrc.size(), 2U);
    EXPECT_EQ(result.mergedCounts[0], TEST_SIZE_1);
    EXPECT_EQ(result.mergedCounts[1], TEST_SIZE_2);
}

TEST_F(RdmaIndirectTransportManagerTest, MergeBatchCopyDstNotContinuousSplit)
{
    RdmaIndirectTransportManager mgr;
    std::vector<void *> srcAddrs = {
        reinterpret_cast<void *>(TEST_SRC_ADDR),
        reinterpret_cast<void *>(TEST_SRC_ADDR + TEST_SIZE_1)
    };
    std::vector<void *> dstAddrs = {
        reinterpret_cast<void *>(TEST_DST_ADDR),
        reinterpret_cast<void *>(TEST_FAR_ADDR)
    };
    std::vector<uint64_t> counts = {TEST_SIZE_1, TEST_SIZE_2};
    RdmaIndirectTransportManager::MergeResult result;

    auto ret = mgr.MergeBatchCopy(srcAddrs, dstAddrs, counts, result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.mergedSrc.size(), 2U);
    EXPECT_EQ(result.mergedCounts[0], TEST_SIZE_1);
    EXPECT_EQ(result.mergedCounts[1], TEST_SIZE_2);
}

TEST_F(RdmaIndirectTransportManagerTest, MergeBatchCopyPartialContinuousMerge)
{
    RdmaIndirectTransportManager mgr;
    std::vector<void *> srcAddrs = {
        reinterpret_cast<void *>(TEST_SRC_ADDR),
        reinterpret_cast<void *>(TEST_SRC_ADDR + TEST_SIZE_1),
        reinterpret_cast<void *>(TEST_ALT_SRC_ADDR),
        reinterpret_cast<void *>(TEST_ALT_SRC_ADDR + TEST_SIZE_3)
    };
    std::vector<void *> dstAddrs = {
        reinterpret_cast<void *>(TEST_DST_ADDR),
        reinterpret_cast<void *>(TEST_DST_ADDR + TEST_SIZE_1),
        reinterpret_cast<void *>(TEST_ALT_DST_ADDR),
        reinterpret_cast<void *>(TEST_ALT_DST_ADDR + TEST_SIZE_3)
    };
    std::vector<uint64_t> counts = {TEST_SIZE_1, TEST_SIZE_2, TEST_SIZE_3, TEST_SIZE_1};
    RdmaIndirectTransportManager::MergeResult result;

    auto ret = mgr.MergeBatchCopy(srcAddrs, dstAddrs, counts, result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.mergedSrc.size(), 2U);
    EXPECT_EQ(result.mergedCounts[0], TEST_SIZE_1 + TEST_SIZE_2);
    EXPECT_EQ(result.mergedCounts[1], TEST_SIZE_3 + TEST_SIZE_1);
}

TEST_F(RdmaIndirectTransportManagerTest, RegisterMemoryRegionAlwaysOk)
{
    RdmaIndirectTransportManager mgr;
    TransportMemoryRegion mr{};
    mr.addr = TEST_SRC_ADDR;
    mr.size = TEST_MEM_REGION_SIZE;
    EXPECT_EQ(mgr.RegisterMemoryRegion(mr), BM_OK);
}

TEST_F(RdmaIndirectTransportManagerTest, UnregisterMemoryRegionAlwaysOk)
{
    RdmaIndirectTransportManager mgr;
    EXPECT_EQ(mgr.UnregisterMemoryRegion(TEST_SRC_ADDR), BM_OK);
}

TEST_F(RdmaIndirectTransportManagerTest, QueryHasRegisteredAlwaysTrue)
{
    RdmaIndirectTransportManager mgr;
    EXPECT_TRUE(mgr.QueryHasRegistered(TEST_SRC_ADDR, TEST_MEM_REGION_SIZE));
    EXPECT_TRUE(mgr.QueryHasRegistered(0x0, 0x0));
}

TEST_F(RdmaIndirectTransportManagerTest, QueryMemoryKeyAlwaysOk)
{
    RdmaIndirectTransportManager mgr;
    TransportMemoryKey key{};
    EXPECT_EQ(mgr.QueryMemoryKey(TEST_SRC_ADDR, key), BM_OK);
}

TEST_F(RdmaIndirectTransportManagerTest, AsyncConnectAlwaysOk)
{
    RdmaIndirectTransportManager mgr;
    EXPECT_EQ(mgr.AsyncConnect(), BM_OK);
}

TEST_F(RdmaIndirectTransportManagerTest, SynchronizeNoPendingRequestsReturnsOk)
{
    RdmaIndirectTransportManager mgr;
    mgr.pendingRequestContext_ = nullptr;
    EXPECT_EQ(mgr.Synchronize(0), BM_OK);
}

TEST_F(RdmaIndirectTransportManagerTest, SynchronizeWithZeroCountReturnsOk)
{
    RdmaIndirectTransportManager mgr;
    mgr.pendingRequestContext_ = std::make_shared<RdmaIndirectTransportManager::PendingRequestContext>();
    mgr.pendingRequestContext_->count = 0;
    auto ctx = mgr.pendingRequestContext_;

    std::thread notifier([ctx]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_SLEEP_MS_SHORT));
        ctx->cond.notify_one();
    });

    auto ret = mgr.Synchronize(0);
    EXPECT_EQ(ret, BM_OK);
    notifier.join();
}

TEST_F(RdmaIndirectTransportManagerTest, DecrementPendingCountTriggersNotify)
{
    auto ctx = std::make_shared<RdmaIndirectTransportManager::PendingRequestContext>();
    ctx->count = 1;

    RdmaIndirectTransportManager mgr;
    bool notified = false;
    std::mutex mtx;
    std::condition_variable cv;

    std::thread waiter([&ctx, &notified, &cv]() {
        std::unique_lock<std::mutex> lock(ctx->mutex);
        ctx->cond.wait(lock, [&ctx]() { return ctx->count <= 0; });
        notified = true;
        cv.notify_one();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_SLEEP_MS_SHORT));
    mgr.DecrementPendingCount(ctx);

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(TEST_TIMEOUT_SECONDS), [&notified]() { return notified; });
    }

    EXPECT_TRUE(notified);
    EXPECT_EQ(ctx->count, 0);
    waiter.join();
}

TEST_F(RdmaIndirectTransportManagerTest, DecrementPendingCountMultipleDecrements)
{
    auto ctx = std::make_shared<RdmaIndirectTransportManager::PendingRequestContext>();
    ctx->count = TEST_INITIAL_PENDING_COUNT;

    RdmaIndirectTransportManager mgr;
    mgr.DecrementPendingCount(ctx);
    EXPECT_EQ(ctx->count, TEST_INITIAL_PENDING_COUNT - 1);

    mgr.DecrementPendingCount(ctx);
    EXPECT_EQ(ctx->count, TEST_INITIAL_PENDING_COUNT - 1 - 1);

    bool notified = false;
    std::mutex mtx;
    std::condition_variable cv;

    std::thread waiter([&ctx, &notified, &cv]() {
        std::unique_lock<std::mutex> lock(ctx->mutex);
        ctx->cond.wait(lock, [&ctx]() { return ctx->count <= 0; });
        notified = true;
        cv.notify_one();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_SLEEP_MS_SHORT));
    mgr.DecrementPendingCount(ctx);
    EXPECT_EQ(ctx->count, 0);

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(TEST_TIMEOUT_SECONDS), [&notified]() { return notified; });
    }

    EXPECT_TRUE(notified);
    waiter.join();
}

TEST_F(RdmaIndirectTransportManagerTest, ReadRemoteBatchAsyncSizeMismatchReturnsInvalidParam)
{
    RdmaIndirectTransportManager mgr;
    CopyDescriptor desc;
    desc.localAddrs = {reinterpret_cast<void *>(TEST_SRC_ADDR)};
    desc.globalAddrs = {reinterpret_cast<void *>(TEST_DST_ADDR), reinterpret_cast<void *>(TEST_ALT_DST_ADDR)};
    desc.counts = {TEST_SIZE_1};

    EXPECT_EQ(mgr.ReadRemoteBatchAsync(0, desc), BM_INVALID_PARAM);
}

TEST_F(RdmaIndirectTransportManagerTest, ReadRemoteBatchAsyncEmptyDescriptorReturnsOk)
{
    RdmaIndirectTransportManager mgr;
    CopyDescriptor desc;
    EXPECT_EQ(mgr.ReadRemoteBatchAsync(0, desc), BM_OK);
}

TEST_F(RdmaIndirectTransportManagerTest, WriteRemoteBatchAsyncSizeMismatchReturnsInvalidParam)
{
    RdmaIndirectTransportManager mgr;
    CopyDescriptor desc;
    desc.localAddrs = {reinterpret_cast<void *>(TEST_SRC_ADDR)};
    desc.globalAddrs = {reinterpret_cast<void *>(TEST_DST_ADDR)};
    desc.counts = {TEST_SIZE_1, TEST_SIZE_2};

    EXPECT_EQ(mgr.WriteRemoteBatchAsync(0, desc), BM_INVALID_PARAM);
}

TEST_F(RdmaIndirectTransportManagerTest, WriteRemoteBatchAsyncEmptyDescriptorReturnsOk)
{
    RdmaIndirectTransportManager mgr;
    CopyDescriptor desc;
    EXPECT_EQ(mgr.WriteRemoteBatchAsync(0, desc), BM_OK);
}

TEST_F(RdmaIndirectTransportManagerTest, PrepareRejectsInvalidRankId)
{
    RdmaIndirectTransportManager mgr;
    mgr.rankCount_ = TEST_RANK_COUNT;
    mgr.localRankId_ = 0;

    HybmTransPrepareOptions opts;
    TransportRankPrepareInfo info;
    info.nic = TEST_NIC_ADDR_LOCAL;
    opts.options.emplace(0U, info);
    opts.options.emplace(TEST_INVALID_RANK_ID_1, info);

    EXPECT_EQ(mgr.Prepare(opts), BM_INVALID_PARAM);
}

TEST_F(RdmaIndirectTransportManagerTest, UpdateRankOptionsRejectsInvalidRankId)
{
    RdmaIndirectTransportManager mgr;
    mgr.rankCount_ = TEST_RANK_COUNT;
    mgr.localRankId_ = 0;

    HybmTransPrepareOptions opts;
    TransportRankPrepareInfo info;
    info.nic = TEST_NIC_ADDR_LOCAL;
    opts.options.emplace(0U, info);
    opts.options.emplace(TEST_INVALID_RANK_ID_2, info);

    EXPECT_EQ(mgr.UpdateRankOptions(opts), BM_INVALID_PARAM);
}

TEST_F(RdmaIndirectTransportManagerTest, GenerateInitRequestBasicFields)
{
    RdmaIndirectTransportManager mgr;
    mgr.localRankId_ = 1;
    mgr.requestIdGen.store(0);

    RdmaIndirectTransportManager::SliceList slices;
    RdmaIndirectTransportManager::Slice slice;
    slice.lAddr = TEST_SRC_ADDR;
    slice.rAddr = TEST_DST_ADDR;
    slice.size = TEST_SIZE_1;
    slice.type = 0;
    slice.rankId = TEST_DST_RANK_ID;
    slices.slices.push_back(slice);
    slices.enqueueTime = TEST_ENQUEUE_TIME;

    auto msg = mgr.GenerateInitRequest(slices);

    EXPECT_EQ(msg.head.request, 1U);
    EXPECT_EQ(msg.head.opCode, 0);
    EXPECT_EQ(msg.head.srcRankId, 1U);
    EXPECT_EQ(msg.head.dstRankId, TEST_DST_RANK_ID);
    EXPECT_EQ(msg.head.bodySize,
        sizeof(slices.enqueueTime) + slices.slices.size() * sizeof(RdmaIndirectTransportManager::Slice));
    EXPECT_NE(msg.head.requestId, 0ULL);
    EXPECT_EQ(msg.body.size(), msg.head.bodySize);
}

TEST_F(RdmaIndirectTransportManagerTest, GenerateInitRequestIdIncrements)
{
    RdmaIndirectTransportManager mgr;
    mgr.localRankId_ = 0;
    mgr.requestIdGen.store(TEST_REQUEST_ID_INIT);

    RdmaIndirectTransportManager::SliceList slices;
    RdmaIndirectTransportManager::Slice slice;
    slice.lAddr = TEST_SRC_ADDR;
    slice.rAddr = TEST_DST_ADDR;
    slice.size = TEST_SIZE_1;
    slice.type = 0;
    slice.rankId = 1;
    slices.slices.push_back(slice);

    auto msg1 = mgr.GenerateInitRequest(slices);
    auto msg2 = mgr.GenerateInitRequest(slices);

    EXPECT_NE(msg1.head.requestId, msg2.head.requestId);
    EXPECT_EQ(msg2.head.requestId - msg1.head.requestId, 1ULL);
}

TEST_F(RdmaIndirectTransportManagerTest, GenerateInitRequestMultipleSlices)
{
    RdmaIndirectTransportManager mgr;
    mgr.localRankId_ = TEST_LOCAL_RANK_ID_ALT;
    mgr.requestIdGen.store(0);

    RdmaIndirectTransportManager::SliceList slices;
    for (int i = 0; i < TEST_NUM_SLICES; i++) {
        RdmaIndirectTransportManager::Slice slice;
        slice.lAddr = TEST_SRC_ADDR + i * TEST_SIZE_1;
        slice.rAddr = TEST_DST_ADDR + i * TEST_SIZE_1;
        slice.size = TEST_SIZE_1;
        slice.type = 1;
        slice.rankId = TEST_DST_RANK_ID;
        slices.slices.push_back(slice);
    }

    auto msg = mgr.GenerateInitRequest(slices);
    EXPECT_EQ(msg.head.bodySize,
        sizeof(slices.enqueueTime) + TEST_NUM_SLICES * sizeof(RdmaIndirectTransportManager::Slice));
    EXPECT_EQ(msg.body.size(), msg.head.bodySize);
}

TEST_F(RdmaIndirectTransportManagerTest, GetPrivateDataWithNicAndPort)
{
    RdmaIndirectTransportManager mgr;
    mgr.localNic_ = TEST_NIC_ADDR_REMOTE;
    mgr.localPort_ = TEST_LOCAL_PORT;
    mgr.swapMemKey_ = {};

    auto data = mgr.GetPrivateData();
    std::string ipStr(data.ip);
    EXPECT_FALSE(ipStr.empty());
}

TEST_F(RdmaIndirectTransportManagerTest, CloseDeviceWhenAlreadyClosedReturnsOk)
{
    RdmaIndirectTransportManager mgr;
    mgr.running_ = false;
    mgr.gServerSocket_ = -1;
    mgr.gOutBandEpollFd_ = -1;
    mgr.buffer_ = nullptr;

    auto ret = mgr.CloseDevice();
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(RdmaIndirectTransportManagerTest, ConstructorInitializesDefaults)
{
    RdmaIndirectTransportManager mgr;
    EXPECT_FALSE(mgr.running_);
    EXPECT_EQ(mgr.gServerSocket_, 0);
    EXPECT_EQ(mgr.gOutBandEpollFd_, 0);
    EXPECT_EQ(mgr.buffer_, nullptr);
    EXPECT_EQ(mgr.localPort_, 0);
    EXPECT_EQ(mgr.rankCount_, 0U);
    EXPECT_EQ(mgr.localRankId_, 0U);
}

TEST_F(RdmaIndirectTransportManagerTest, MergeBatchCopyCountsMismatchReturnsError)
{
    RdmaIndirectTransportManager mgr;
    std::vector<void *> srcAddrs = {reinterpret_cast<void *>(TEST_SRC_ADDR)};
    std::vector<void *> dstAddrs = {reinterpret_cast<void *>(TEST_DST_ADDR)};
    std::vector<uint64_t> counts = {TEST_SIZE_1, TEST_SIZE_2};
    RdmaIndirectTransportManager::MergeResult result;

    auto ret = mgr.MergeBatchCopy(srcAddrs, dstAddrs, counts, result);
    EXPECT_EQ(ret, -1);
}

TEST_F(RdmaIndirectTransportManagerTest, MergeBatchCopyLargeContinuousSegment)
{
    RdmaIndirectTransportManager mgr;
    std::vector<void *> srcAddrs;
    std::vector<void *> dstAddrs;
    std::vector<uint64_t> counts;
    uint64_t offset = 0;

    for (int i = 0; i < TEST_NUM_ENTRIES; i++) {
        srcAddrs.push_back(reinterpret_cast<void *>(TEST_LARGE_SRC_BASE + offset));
        dstAddrs.push_back(reinterpret_cast<void *>(TEST_LARGE_DST_BASE + offset));
        counts.push_back(TEST_SIZE_1);
        offset += TEST_SIZE_1;
    }

    RdmaIndirectTransportManager::MergeResult result;
    auto ret = mgr.MergeBatchCopy(srcAddrs, dstAddrs, counts, result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.mergedSrc.size(), 1U);
    EXPECT_EQ(result.mergedCounts[0], TEST_NUM_ENTRIES * TEST_SIZE_1);
}

TEST_F(RdmaIndirectTransportManagerTest, MergeBatchCopyAlternatingContinuousAndGap)
{
    RdmaIndirectTransportManager mgr;
    std::vector<void *> srcAddrs = {
        reinterpret_cast<void *>(TEST_SRC_ADDR),
        reinterpret_cast<void *>(TEST_SRC_ADDR + TEST_SIZE_1),
        reinterpret_cast<void *>(TEST_ALT_SRC_ADDR),
        reinterpret_cast<void *>(TEST_ALT_SRC_ADDR + TEST_SIZE_3)
    };
    std::vector<void *> dstAddrs = {
        reinterpret_cast<void *>(TEST_DST_ADDR),
        reinterpret_cast<void *>(TEST_DST_ADDR + TEST_SIZE_1),
        reinterpret_cast<void *>(TEST_ALT_DST_ADDR),
        reinterpret_cast<void *>(TEST_ALT_DST_ADDR + TEST_SIZE_3)
    };
    std::vector<uint64_t> counts = {TEST_SIZE_1, TEST_SIZE_2, TEST_SIZE_3, TEST_SIZE_1};
    RdmaIndirectTransportManager::MergeResult result;

    auto ret = mgr.MergeBatchCopy(srcAddrs, dstAddrs, counts, result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.mergedSrc.size(), 2U);
    EXPECT_EQ(result.mergedSrc[0], reinterpret_cast<void *>(TEST_SRC_ADDR));
    EXPECT_EQ(result.mergedSrc[1], reinterpret_cast<void *>(TEST_ALT_SRC_ADDR));
    EXPECT_EQ(result.mergedCounts[0], TEST_SIZE_1 + TEST_SIZE_2);
    EXPECT_EQ(result.mergedCounts[1], TEST_SIZE_3 + TEST_SIZE_1);
}

TEST_F(RdmaIndirectTransportManagerTest, ReadRemoteBatchAsyncLocalGlobalCountsMismatch)
{
    RdmaIndirectTransportManager mgr;
    CopyDescriptor desc;
    desc.localAddrs = {reinterpret_cast<void *>(TEST_SRC_ADDR), reinterpret_cast<void *>(TEST_DST_ADDR)};
    desc.globalAddrs = {reinterpret_cast<void *>(TEST_ALT_DST_ADDR)};
    desc.counts = {TEST_SIZE_1};

    EXPECT_EQ(mgr.ReadRemoteBatchAsync(0, desc), BM_INVALID_PARAM);
}

TEST_F(RdmaIndirectTransportManagerTest, WriteRemoteBatchAsyncLocalGlobalCountsMismatch)
{
    RdmaIndirectTransportManager mgr;
    CopyDescriptor desc;
    desc.localAddrs = {reinterpret_cast<void *>(TEST_SRC_ADDR)};
    desc.globalAddrs = {reinterpret_cast<void *>(TEST_DST_ADDR), reinterpret_cast<void *>(TEST_ALT_DST_ADDR)};
    desc.counts = {TEST_SIZE_1};

    EXPECT_EQ(mgr.WriteRemoteBatchAsync(0, desc), BM_INVALID_PARAM);
}

TEST_F(RdmaIndirectTransportManagerTest, SynchronizeWaitsForPendingCountToReachZero)
{
    RdmaIndirectTransportManager mgr;
    mgr.pendingRequestContext_ = std::make_shared<RdmaIndirectTransportManager::PendingRequestContext>();
    mgr.pendingRequestContext_->count = TEST_SYNC_PENDING_COUNT;
    auto ctx = mgr.pendingRequestContext_;

    std::thread decrementer([ctx]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_SLEEP_MS_MEDIUM));
        ctx->count--;
        ctx->cond.notify_one();
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_SLEEP_MS_MEDIUM));
        ctx->count--;
        ctx->cond.notify_one();
    });

    auto ret = mgr.Synchronize(0);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(ctx->count, 0);
    decrementer.join();
}

TEST_F(RdmaIndirectTransportManagerTest, DecrementPendingCountBelowZeroDoesNotCrash)
{
    auto ctx = std::make_shared<RdmaIndirectTransportManager::PendingRequestContext>();
    ctx->count = 0;

    RdmaIndirectTransportManager mgr;
    mgr.DecrementPendingCount(ctx);
    EXPECT_EQ(ctx->count, -1);

    mgr.DecrementPendingCount(ctx);
    EXPECT_EQ(ctx->count, TEST_COUNT_MINUS_TWO);
}
