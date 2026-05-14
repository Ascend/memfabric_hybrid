/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include <thread>
#include <vector>
#include <algorithm>
#include <unordered_map>

#include "zbal_sma_mm_heap.h"

using namespace zbal;
using namespace zbal::sma::heap;
using namespace zbal::sma;

/* ================================================================
 * Test fixture: provides a 1MB heap buffer for most tests
 * ================================================================ */
class TestSplitMemoryHeap : public testing::Test {
public:
    static constexpr size_t kHeapSize = 1024 * 1024; // 1 MB
    static constexpr size_t kDefaultThreshold = kHeapSize; // all use low-to-high by default

    void SetUp() override
    {
        buffer_ = static_cast<uint8_t *>(aligned_alloc(32, kHeapSize));
        ASSERT_NE(buffer_, nullptr) << "aligned_alloc failed";
        heap_ = new SplitMemoryHeap(buffer_, kHeapSize, kDefaultThreshold);
        ASSERT_TRUE(heap_->isInitialized());
    }

    void TearDown() override
    {
        delete heap_;
        free(buffer_);
    }

    uint8_t *buffer_;
    SplitMemoryHeap *heap_;
};

/* ================================================================
 * Constructor
 * ================================================================ */

TEST_F(TestSplitMemoryHeap, ConstructValid)
{
    EXPECT_TRUE(heap_->isInitialized());
    EXPECT_EQ(heap_->getTotalSize(), kHeapSize);
    EXPECT_EQ(heap_->getInUsedSize(), 0u);
}

TEST(TestSplitMemoryHeapCtor, NullBaseNotInitialized)
{
    SplitMemoryHeap heap(nullptr, 1024);
    EXPECT_FALSE(heap.isInitialized());
    EXPECT_EQ(heap.alignedAllocate(32, 128), nullptr);
}

TEST(TestSplitMemoryHeapCtor, ZeroSizeNotInitialized)
{
    uint8_t buf[256];
    SplitMemoryHeap heap(buf, 0);
    EXPECT_FALSE(heap.isInitialized());
    EXPECT_EQ(heap.alignedAllocate(32, 128), nullptr);
}

/* ================================================================
 * Basic allocate / free / allocatedSize
 * ================================================================ */

TEST_F(TestSplitMemoryHeap, AllocFreeRoundTrip)
{
    void *p = heap_->alignedAllocate(32, 1024);
    ASSERT_NE(p, nullptr);
    EXPECT_GE(heap_->getInUsedSize(), 1024u);

    uint64_t sz = 0;
    EXPECT_TRUE(heap_->allocatedSize(p, sz));
    EXPECT_EQ(sz, 1024u);

    EXPECT_EQ(heap_->release(p), 0);
    EXPECT_EQ(heap_->getInUsedSize(), 0u);
    EXPECT_FALSE(heap_->allocatedSize(p, sz));
}

TEST_F(TestSplitMemoryHeap, AllocZeroSizeReturnsNull)
{
    EXPECT_EQ(heap_->alignedAllocate(32, 0), nullptr);
}

TEST_F(TestSplitMemoryHeap, AllocAlignmentOne)
{
    // alignment=0 is treated as alignment=1
    void *p = heap_->alignedAllocate(0, 64);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(heap_->release(p), 0);
}

/* ================================================================
 * Multiple allocations — exhaust and re-use
 * ================================================================ */

TEST_F(TestSplitMemoryHeap, MultipleAllocsUntilOOM)
{
    std::vector<void *> ptrs;
    size_t blockSize = kHeapSize / 16;
    for (int i = 0; i < 16; i++) {
        void *p = heap_->alignedAllocate(32, blockSize);
        ASSERT_NE(p, nullptr) << "failed at alloc " << i;
        ptrs.push_back(p);
    }
    // next alloc should fail
    EXPECT_EQ(heap_->alignedAllocate(32, 1), nullptr);

    // free half and re-allocate
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(heap_->release(ptrs[i]), 0);
    }
    for (int i = 0; i < 8; i++) {
        void *p = heap_->alignedAllocate(32, blockSize);
        ASSERT_NE(p, nullptr) << "failed at re-alloc " << i;
    }
    // free remaining
    for (int i = 8; i < 16; i++) {
        EXPECT_EQ(heap_->release(ptrs[i]), 0);
    }
}

/* ================================================================
 * Alignment verification: returned pointers must be aligned
 * ================================================================ */

TEST_F(TestSplitMemoryHeap, Alignment32)
{
    for (int i = 0; i < 50; i++) {
        void *p = heap_->alignedAllocate(32, 7 + (i * 13) % 200);
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 32, 0u);
        heap_->release(p);
    }
}

TEST_F(TestSplitMemoryHeap, Alignment128)
{
    uint8_t bigBuf[1024 * 1024];
    SplitMemoryHeap heap(bigBuf, sizeof(bigBuf));
    for (int i = 0; i < 20; i++) {
        void *p = heap.alignedAllocate(128, 63 + (i * 31) % 500);
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 128, 0u);
        heap.release(p);
    }
}

/* ================================================================
 * High-to-low allocation strategy (size >= threshold)
 * ================================================================ */

TEST(TestSplitMemoryHeapHiLo, AllocatesFromHighAddress)
{
    constexpr size_t kSize = 64 * 1024;
    uint8_t buf[kSize];
    // threshold = 0 means all allocs use high-to-low
    SplitMemoryHeap heap(buf, kSize, 0);

    void *p1 = heap.alignedAllocate(1, 1024);
    void *p2 = heap.alignedAllocate(1, 2048);

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);

    // p1 sized 1024 is allocated from the high end first,
    // p2 sized 2048 is allocated from the remaining high end (lower addr)
    EXPECT_LT(reinterpret_cast<uintptr_t>(p2), reinterpret_cast<uintptr_t>(p1));

    heap.release(p1);
    heap.release(p2);
}

TEST(TestSplitMemoryHeapHiLo, ExhaustThenOOM)
{
    constexpr size_t kSize = 64 * 1024;
    uint8_t buf[kSize];
    SplitMemoryHeap heap(buf, kSize, 0);

    std::vector<void *> ptrs;
    for (int i = 0; i < 32; i++) {
        void *p = heap.alignedAllocate(1, 2048);
        if (p == nullptr) break;
        ptrs.push_back(p);
    }
    EXPECT_GT(ptrs.size(), 0u);

    // verify all addresses are within buffer
    uintptr_t base = reinterpret_cast<uintptr_t>(buf);
    for (auto *p : ptrs) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        EXPECT_GE(addr, base);
        EXPECT_LT(addr, base + kSize);
        heap.release(p);
    }
}

/* ================================================================
 * Coalescing: adjacent free blocks merge
 * ================================================================ */

TEST_F(TestSplitMemoryHeap, CoalesceRightNeighbor)
{
    void *a = heap_->alignedAllocate(32, 1024);
    void *b = heap_->alignedAllocate(32, 1024);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // free a then b: they should coalesce (b is right neighbor of a)
    heap_->release(a);
    heap_->release(b);

    // after coalescing, we should be able to allocate 2048 in one block
    void *c = heap_->alignedAllocate(32, 2000);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(heap_->release(c), 0);
}

TEST_F(TestSplitMemoryHeap, CoalesceLeftNeighbor)
{
    void *a = heap_->alignedAllocate(32, 1024);
    void *b = heap_->alignedAllocate(32, 1024);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // free b then a: they should coalesce (b is right of a, so a merges with b)
    heap_->release(b);
    heap_->release(a);

    void *c = heap_->alignedAllocate(32, 2000);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(heap_->release(c), 0);
}

TEST_F(TestSplitMemoryHeap, CoalesceBothNeighbors)
{
    void *a = heap_->alignedAllocate(32, 512);
    void *b = heap_->alignedAllocate(32, 1024);
    void *c = heap_->alignedAllocate(32, 512);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    // free a, c, then b: b should coalesce with both
    heap_->release(a);
    heap_->release(c);
    heap_->release(b);

    void *d = heap_->alignedAllocate(32, 2000);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(heap_->release(d), 0);
}

/* ================================================================
 * Fragment management: padding + remaining fragments
 * ================================================================ */

TEST_F(TestSplitMemoryHeap, AlignmentCreatesLowerFragment)
{
    // Allocate with specific alignment to force padding fragment
    void *p = heap_->alignedAllocate(256, 1024);
    ASSERT_NE(p, nullptr);

    // If the original address wasn't 256-aligned, there should be a
    // lower fragment. Verify by allocating a tiny block from it.
    void *tiny = heap_->alignedAllocate(1, 8);
    // tiny may or may not come from the fragment depending on alignment
    // Just verify it doesn't crash and is valid
    if (tiny != nullptr) {
        heap_->release(tiny);
    }
    EXPECT_EQ(heap_->release(p), 0);
}

/* ================================================================
 * Double-free detection
 * ================================================================ */

TEST_F(TestSplitMemoryHeap, DoubleFreeReturnsError)
{
    void *p = heap_->alignedAllocate(32, 256);
    ASSERT_NE(p, nullptr);

    EXPECT_EQ(heap_->release(p), 0);
    EXPECT_EQ(heap_->release(p), -1); // double free
}

TEST_F(TestSplitMemoryHeap, ReleaseNullReturnsError)
{
    EXPECT_EQ(heap_->release(nullptr), -1);
}

TEST_F(TestSplitMemoryHeap, ReleaseInvalidPointerReturnsError)
{
    int dummy = 0;
    EXPECT_EQ(heap_->release(&dummy), -1);
}

/* ================================================================
 * Size tracking
 * ================================================================ */

TEST_F(TestSplitMemoryHeap, InUsedSizeTracksAllocations)
{
    EXPECT_EQ(heap_->getInUsedSize(), 0u);

    void *p1 = heap_->alignedAllocate(32, 100);
    size_t after1 = heap_->getInUsedSize();
    EXPECT_GE(after1, 100u);

    void *p2 = heap_->alignedAllocate(32, 250);
    size_t after2 = heap_->getInUsedSize();
    EXPECT_GE(after2, after1 + 250u);

    heap_->release(p1);
    heap_->release(p2);
    EXPECT_EQ(heap_->getInUsedSize(), 0u);
}

TEST_F(TestSplitMemoryHeap, TotalSizeConstant)
{
    EXPECT_EQ(heap_->getTotalSize(), kHeapSize);
    void *p = heap_->alignedAllocate(32, 512);
    EXPECT_EQ(heap_->getTotalSize(), kHeapSize);
    heap_->release(p);
    EXPECT_EQ(heap_->getTotalSize(), kHeapSize);
}

/* ================================================================
 * Bucket index calculation (free function in zbal::sma)
 * ================================================================ */

TEST_F(TestSplitMemoryHeap, BucketIndexZero)
{
    EXPECT_EQ(get_bucket_index(0), 0);
}

TEST_F(TestSplitMemoryHeap, BucketIndexPowersOfTwo)
{
    EXPECT_EQ(get_bucket_index(1), 0);       // 2^0
    EXPECT_EQ(get_bucket_index(2), 1);        // 2^1
    EXPECT_EQ(get_bucket_index(4), 2);        // 2^2
    EXPECT_EQ(get_bucket_index(8), 3);        // 2^3
    EXPECT_EQ(get_bucket_index(16), 4);       // 2^4
    EXPECT_EQ(get_bucket_index(32), 5);
    EXPECT_EQ(get_bucket_index(64), 6);
    EXPECT_EQ(get_bucket_index(128), 7);
    EXPECT_EQ(get_bucket_index(256), 8);
    EXPECT_EQ(get_bucket_index(512), 9);
    EXPECT_EQ(get_bucket_index(1024), 10);
}

TEST_F(TestSplitMemoryHeap, BucketIndexNonPowerOfTwo)
{
    EXPECT_EQ(get_bucket_index(3), 1);        // floor(log2(3)) = 1
    EXPECT_EQ(get_bucket_index(5), 2);
    EXPECT_EQ(get_bucket_index(7), 2);
    EXPECT_EQ(get_bucket_index(100), 6);       // floor(log2(100)) = 6
    EXPECT_EQ(get_bucket_index(1000), 9);      // floor(log2(1000)) = 9
}

/* ================================================================
 * Concurrent access — spinlock stress
 * ================================================================ */

TEST_F(TestSplitMemoryHeap, ConcurrentAllocFree)
{
    constexpr int kThreads = 4;
    constexpr int kOpsPerThread = 250;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; t++) {
        threads.emplace_back([this]() {
            for (int i = 0; i < kOpsPerThread; i++) {
                void *p = heap_->alignedAllocate(32, 64 + (i % 10) * 13);
                if (p != nullptr) {
                    // tiny delay to increase contention
                    for (volatile int d = 0; d < 10; d++) {}
                    EXPECT_EQ(heap_->release(p), 0);
                }
            }
        });
    }
    for (auto &t : threads) {
        t.join();
    }
    // after all threads finish, heap should be back to initial state
    EXPECT_EQ(heap_->getInUsedSize(), 0u);
}

/* ================================================================
 * Threshold strategy boundary
 * ================================================================ */

TEST(TestSplitMemoryHeapStrategy, BelowThresholdUsesLowToHigh)
{
    constexpr size_t kSize = 128 * 1024;
    uint8_t buf[kSize];
    SplitMemoryHeap heap(buf, kSize, 4096); // threshold = 4096

    // size < 4096: low-to-high
    void *p = heap.alignedAllocate(1, 1024);
    ASSERT_NE(p, nullptr);
    // Should be near the start of the buffer
    uintptr_t base = reinterpret_cast<uintptr_t>(buf);
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    EXPECT_LT(addr - base, kSize / 4u);
    heap.release(p);
}

TEST(TestSplitMemoryHeapStrategy, AtThresholdUsesHighToLow)
{
    constexpr size_t kSize = 128 * 1024;
    uint8_t buf[kSize];
    SplitMemoryHeap heap(buf, kSize, 4096); // threshold = 4096

    // size >= 4096: high-to-low
    void *p = heap.alignedAllocate(1, 4096);
    ASSERT_NE(p, nullptr);
    uintptr_t base = reinterpret_cast<uintptr_t>(buf);
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    // Should be near the end of the buffer
    EXPECT_GT(addr - base, kSize / 2u);
    heap.release(p);
}

/* ================================================================
 * C API wrappers
 * ================================================================ */

using namespace zbal::sma;

class TestSplitMemoryHeapCApi : public testing::Test {
public:
    static constexpr size_t kHeapSize = 1024 * 1024;

    void SetUp() override
    {
        buffer_ = new uint8_t[kHeapSize];
        auto *rawHeap = new SplitMemoryHeap(buffer_, kHeapSize);
        heap_ = std::shared_ptr<CustomMemoryHeap>(rawHeap);
    }

    void TearDown() override
    {
        heap_.reset();
        delete[] buffer_;
    }

    uint8_t *buffer_;
    std::shared_ptr<CustomMemoryHeap> heap_;
};

TEST_F(TestSplitMemoryHeapCApi, CustomHeapAlignedAllocateSuccess)
{
    void *devPtr = nullptr;
    EXPECT_EQ(CustomHeapAlignedAllocate(&devPtr, 256, heap_), Z_OK);
    ASSERT_NE(devPtr, nullptr);
    EXPECT_EQ(CustomHeapRelease(devPtr, heap_), 0);
}

TEST_F(TestSplitMemoryHeapCApi, CustomHeapAlignedAllocateNullPool)
{
    void *devPtr = nullptr;
    EXPECT_EQ(CustomHeapAlignedAllocate(&devPtr, 256, nullptr), Z_ERROR);
}

TEST_F(TestSplitMemoryHeapCApi, CustomHeapAlignedAllocateOOM)
{
    void *devPtr = nullptr;
    // request more than available
    EXPECT_EQ(CustomHeapAlignedAllocate(&devPtr, kHeapSize * 2, heap_), Z_ERROR_ALLOC);
    EXPECT_EQ(devPtr, nullptr);
}

TEST_F(TestSplitMemoryHeapCApi, CustomHeapReleaseNullPool)
{
    int dummy = 0;
    EXPECT_EQ(CustomHeapRelease(&dummy, nullptr), Z_ERROR);
}

TEST_F(TestSplitMemoryHeapCApi, CustomGetTotalSize)
{
    size_t size = 0;
    EXPECT_EQ(CustomGetTotalSize(size, heap_), Z_OK);
    EXPECT_EQ(size, kHeapSize);
}

TEST_F(TestSplitMemoryHeapCApi, CustomGetTotalSizeNullPool)
{
    size_t size = 0;
    EXPECT_EQ(CustomGetTotalSize(size, nullptr), Z_ERROR);
}

TEST_F(TestSplitMemoryHeapCApi, CustomGetInUsedSize)
{
    size_t size = 0;
    void *p = nullptr;
    CustomHeapAlignedAllocate(&p, 512, heap_);
    EXPECT_EQ(CustomInUsedSize(size, heap_), Z_OK);
    EXPECT_GE(size, 512u);
    CustomHeapRelease(p, heap_);
}

TEST_F(TestSplitMemoryHeapCApi, CustomGetInUsedSizeNullPool)
{
    size_t size = 0;
    EXPECT_EQ(CustomInUsedSize(size, nullptr), Z_ERROR);
}

/* ================================================================
 * Edge case: single allocation spanning entire heap
 * ================================================================ */

TEST_F(TestSplitMemoryHeap, AllocEntireHeap)
{
    // allocate almost entire heap (alignment=1 to minimize overhead)
    void *p = heap_->alignedAllocate(1, kHeapSize - 128);
    ASSERT_NE(p, nullptr);
    // attempt to allocate remaining space — may or may not succeed depending on fragments
    // the key verification: we consumed most of the heap
    void *q2 = heap_->alignedAllocate(1, kHeapSize / 2);
    EXPECT_EQ(q2, nullptr); // should be OOM for this large alloc
    EXPECT_EQ(heap_->release(p), 0);
    // after free, should be able to allocate again
    void *q = heap_->alignedAllocate(32, 1024);
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(heap_->release(q), 0);
}

/* ================================================================
 * Edge case: many tiny allocations
 * ================================================================ */

TEST_F(TestSplitMemoryHeap, ManyTinyAllocs)
{
    std::vector<void *> ptrs;
    for (int i = 0; i < 200; i++) {
        void *p = heap_->alignedAllocate(8, 32);
        if (p == nullptr) break;
        ptrs.push_back(p);
    }
    EXPECT_GT(ptrs.size(), 50u);
    for (auto *p : ptrs) {
        EXPECT_EQ(heap_->release(p), 0);
    }
    EXPECT_EQ(heap_->getInUsedSize(), 0u);
}
