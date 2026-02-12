#include <gtest/gtest.h>
#include <cstdint>
#include "mock_env.h"

#define down_read(x) ((void)0)
#define up_read(x) ((void)0)
#define NDR_PEER_MEM_ERR(fmt, ...) ((void)0)
#define NPU_PAGE_SIZE 4096
#define NPU_PAGE_SIZE_ONE_HUNDRED (4096 * 100)
#define NPU_PAGE_SIZE_FIVE (4096 * 5)
#define SZ_1M (1024 * 1024)
#define SZ_2M (2 * SZ_1M)
#define NPU_PAGE_MASK (~(NPU_PAGE_SIZE - 1UL))
#define EXPECTED_DMA_MAP_COUNT_THREE (3)
#define EXPECTED_DMA_MAP_COUNT_TWO (2)
#define TEST_SMALL_PAGE_COUNT (100) /* 40KB buffer for basic functionality test */
#define TEST_HOST_PID_ONE_TWO_THREE_FOUR (1234)
#define TEST_PROCESS_ID_VID (2)
#define TEST_DMA_MAP_FAIL_INDEX (2)

extern "C" {
struct svm_agent_context;
typedef void (*free_callback_t)(void *);
unsigned long ndr_mem_get_page_size(void *client_context);
int ndr_mem_acquire(unsigned long va, size_t size, void *peer_mem_data, char *peer_mem_name, void **client_context);

extern int (*hal_get_pages_func)(u64, u64, free_callback_t, void *, p2p_page_table **);
extern int (*hal_put_pages_func)(p2p_page_table *);

int mock_hal_get_pages(unsigned long, unsigned long, void (*)(void *), void *, struct p2p_page_table **);
int mock_hal_put_pages(struct p2p_page_table *);

#define DMA_BIDIRECTIONAL 0
#define ENOMEM 12
#define EINVAL 22
#define EBUSY 16
#define EFAULT 14
#define ENOENT 2
}

#define UNIT_TESTING 1
#define DECLARE_RWSEM(name) struct rw_semaphore name = {0, 0}

#include "../../src/npu_direct_rdma.c"

class NDRGetPageSizeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        hal_get_pages_func = mock_hal_get_pages;
        hal_put_pages_func = mock_hal_put_pages;
    }
    void TearDown() override
    {
        hal_get_pages_func = nullptr;
        hal_put_pages_func = nullptr;
    }
};
TEST_F(NDRGetPageSizeTest, ReturnsZeroWhenContextIsNull)
{
    EXPECT_EQ(ndr_mem_get_page_size(nullptr), 0UL);
}

TEST_F(NDRGetPageSizeTest, ReturnsZeroWhenContextNotInitialized)
{
    struct svm_agent_context ctx {};
    ctx.inited_flag = 0;
    ctx.page_size   = NPU_PAGE_SIZE;
    EXPECT_EQ(ndr_mem_get_page_size(&ctx), 0UL);
}

TEST_F(NDRGetPageSizeTest, ReturnsPageSizeWhenSuccess)
{
    struct svm_agent_context ctx {};
    ctx.inited_flag = 1;
    ctx.page_size   = SZ_2M;
    EXPECT_EQ(ndr_mem_get_page_size(&ctx), SZ_2M);
}

// ============================================================
//  Acquire Fixture + Tests (Coverage successful, kzalloc failed, hal failed)
// ============================================================
class NDRAcquireTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        hal_get_pages_func              = mock_hal_get_pages;
        hal_put_pages_func              = mock_hal_put_pages;
        g_fail_kzalloc                  = false;
        g_hal_get_pages_should_fail     = false;
        g_hal_get_pages_ret_val         = 0;
        g_kfree_call_count              = 0;
        g_mock_hal_get_pages_call_count = 0;
    }
    void TearDown() override
    {
        g_kfree_call_count              = 0;
        g_mock_hal_get_pages_call_count = 0;
    }
};

TEST_F(NDRAcquireTest, AcquireSuccess_NormalVa)
{
    void *ctx        = nullptr;
    unsigned long va = 0x12345123UL;
    size_t size      = 80000;

    bool ret = ndr_mem_acquire(va, size, nullptr, nullptr, &ctx);

    EXPECT_TRUE(ret);
    EXPECT_NE(ctx, nullptr);
    EXPECT_EQ(g_kfree_call_count, 0);
    EXPECT_GT(g_mock_hal_get_pages_call_count, 0);

    struct svm_agent_context *sctx = (struct svm_agent_context *)ctx;
    EXPECT_EQ(sctx->va, va);
    EXPECT_EQ(sctx->len, size);
    EXPECT_EQ(sctx->inited_flag, 1);
    EXPECT_NE(sctx->page_table, nullptr);
    EXPECT_GT(sctx->aligned_size, size);
    EXPECT_EQ(sctx->va_aligned_start, va & NPU_PAGE_MASK);
    unsigned long expected_end = (va + size + NPU_PAGE_SIZE - 1) & NPU_PAGE_MASK;
    EXPECT_EQ(sctx->va_aligned_end, expected_end);

    EXPECT_EQ(sctx->pa_num, sctx->page_table->page_num);
    EXPECT_EQ(sctx->page_size, 4096UL);

    ndr_mem_release(ctx);
}

TEST_F(NDRAcquireTest, AcquireSuccess_AlignedVa)
{
    void *ctx        = nullptr;
    unsigned long va = 0x10000000UL;
    size_t size      = 4096 * 10;

    bool ret = ndr_mem_acquire(va, size, nullptr, nullptr, &ctx);

    EXPECT_TRUE(ret);
    EXPECT_NE(ctx, nullptr);
    struct svm_agent_context *sctx = (struct svm_agent_context *)ctx;
    EXPECT_EQ(sctx->aligned_size, size);

    ndr_mem_release(ctx);
}

TEST_F(NDRAcquireTest, AcquireFail_Kzalloc)
{
    g_fail_kzalloc = true;

    void *ctx = nullptr;
    bool ret  = ndr_mem_acquire(0x1000, 4096, nullptr, nullptr, &ctx);

    EXPECT_FALSE(ret);
    EXPECT_EQ(ctx, nullptr);
    EXPECT_EQ(g_kfree_call_count, 0);
}

TEST_F(NDRAcquireTest, AcquireFail_HalGetPages)
{
    g_hal_get_pages_should_fail = true;
    g_hal_get_pages_ret_val     = -ENOMEM;

    void *ctx = nullptr;
    bool ret  = ndr_mem_acquire(0x1000, 4096 * 5, nullptr, nullptr, &ctx);

    EXPECT_FALSE(ret);
    EXPECT_EQ(ctx, nullptr);
    EXPECT_EQ(g_kfree_call_count, 1);
}

// ============================================================
// 8. GetPages Fixture + Tests
// ============================================================
class NDRGetPagesTest : public ::testing::Test {
protected:
    void *valid_ctx = nullptr;

    void SetUp() override
    {
        hal_get_pages_func              = mock_hal_get_pages;
        hal_put_pages_func              = mock_hal_put_pages;
        g_hal_get_pages_should_fail     = false;
        g_hal_get_pages_ret_val         = 0;
        g_mock_hal_get_pages_call_count = 0;

        unsigned long va = 0x10000000UL;
        size_t size      = 4096 * 5;
        bool ret         = ndr_mem_acquire(va, size, nullptr, nullptr, &valid_ctx);
        ASSERT_TRUE(ret);

        struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;
        sctx->get_flag                 = 0;
        sctx->process_id.hostpid       = TEST_HOST_PID_ONE_TWO_THREE_FOUR;
        sctx->process_id.devid         = 1;
        sctx->process_id.vfid          = TEST_PROCESS_ID_VID;
        sctx->pa_list                  = nullptr;
    }

    void TearDown() override
    {
        if (valid_ctx) {
            ndr_mem_release(valid_ctx);
            valid_ctx = nullptr;
        }
        g_mock_hal_get_pages_call_count = 0;
    }
};

TEST_F(NDRGetPagesTest, GetPagesFail_ContextNull)
{
    struct sg_table dummy_sg {};
    int ret = ndr_mem_get_pages(0x1000, 4096, 0, 0, &dummy_sg, nullptr, 0);

    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(NDRGetPagesTest, GetPagesFail_NotInited)
{
    struct svm_agent_context bad_ctx {};
    bad_ctx.inited_flag = 0;

    struct sg_table dummy_sg {};
    int ret = ndr_mem_get_pages(0x1000, 4096, 0, 0, &dummy_sg, &bad_ctx, 0);

    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(NDRGetPagesTest, GetPagesFail_VaSizeMismatch)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;
    struct sg_table dummy_sg {};
    int ret = ndr_mem_get_pages(0x20000000UL, 4096 * 10, 0, 0, &dummy_sg, valid_ctx, 0);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(NDRGetPagesTest, GetPagesFail_AlreadyGet)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;

    struct sg_table dummy_sg {};
    int ret1 = ndr_mem_get_pages(0x10000000UL, 4096 * 5, 0, 0, &dummy_sg, valid_ctx, 0);
    EXPECT_EQ(ret1, 0);

    int ret2 = ndr_mem_get_pages(0x10000000UL, 4096 * 5, 0, 0, &dummy_sg, valid_ctx, 0);
    EXPECT_EQ(ret2, 0);
}

TEST_F(NDRGetPagesTest, GetPagesSuccess_Normal)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;

    if (sctx->page_table) {
        mock_hal_put_pages(sctx->page_table);
        sctx->page_table = nullptr;
    }

    struct sg_table dummy_sg {};
    int ret = ndr_mem_get_pages(0x10000000UL, 4096 * 5, 1, 1, &dummy_sg, valid_ctx, 123ULL);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(sctx->page_table, nullptr);
}

TEST_F(NDRGetPagesTest, GetPagesFail_HalError)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;

    if (sctx->page_table) {
        mock_hal_put_pages(sctx->page_table);
        sctx->page_table = nullptr;
    }

    g_hal_get_pages_should_fail     = true;
    g_hal_get_pages_ret_val         = -ENOMEM;
    g_mock_hal_get_pages_call_count = 0;

    struct sg_table dummy_sg {};
    int ret = ndr_mem_get_pages(0x10000000UL, 4096 * 5, 0, 0, &dummy_sg, valid_ctx, 0);

    EXPECT_EQ(ret, -ENOMEM);
    EXPECT_EQ(g_mock_hal_get_pages_call_count, 1);
    EXPECT_EQ(sctx->page_table, nullptr);
}

TEST_F(NDRGetPagesTest, GetPagesSuccess_AlreadyHavePageTable)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;
    struct sg_table dummy_sg1 {};
    int ret1 = ndr_mem_get_pages(0x10000000UL, 4096 * 5, 0, 0, &dummy_sg1, valid_ctx, 0);
    EXPECT_EQ(ret1, 0);
    EXPECT_NE(sctx->page_table, nullptr);

    struct sg_table dummy_sg2 {};
    int ret2 = ndr_mem_get_pages(0x10000000UL, 4096 * 5, 0, 0, &dummy_sg2, valid_ctx, 0);
    EXPECT_EQ(ret2, 0);
    EXPECT_NE(sctx->page_table, nullptr);
}

TEST_F(NDRGetPagesTest, GetPagesSuccess_VaNotAlignedWarning)
{
    void *ctx        = nullptr;
    unsigned long va = 0x10000003UL;
    size_t size      = 4096 * 2;

    bool acquire_ret = ndr_mem_acquire(va, size, nullptr, nullptr, &ctx);
    ASSERT_TRUE(acquire_ret);

    struct sg_table dummy_sg {};
    int ret = ndr_mem_get_pages(va, size, 0, 0, &dummy_sg, ctx, 0);

    EXPECT_EQ(ret, 0);

    ndr_mem_release(ctx);
}

// ============================================================
// DmaMap Fixture + Tests
// ============================================================
// Test constants
static constexpr uint64_t TEST_VA_BASE = 0x10000000UL;
static constexpr size_t NUM_PAGES      = 5;
static constexpr size_t TEST_PAGE_SIZE = NPU_PAGE_SIZE;  // 复用已有宏
static constexpr uint64_t TEST_LEN     = TEST_PAGE_SIZE * NUM_PAGES;
static constexpr uint64_t TEST_VA_END  = TEST_VA_BASE + TEST_LEN;

// Process ID mock values
static constexpr int TEST_HOST_PID = 1234;
static constexpr int TEST_DEV_ID   = 1;
static constexpr int TEST_VF_ID    = 2;

class NDRDmaMapTest : public ::testing::Test {
protected:
    void *valid_ctx = nullptr;
    struct sg_table sg_head {};
    struct device dma_dev {};
    int nmap = 0;

    void SetUp() override
    {
        hal_get_pages_func = mock_hal_get_pages;
        hal_put_pages_func = mock_hal_put_pages;

        g_fail_kzalloc                  = false;
        g_hal_get_pages_should_fail     = false;
        g_hal_get_pages_ret_val         = 0;
        g_kfree_call_count              = 0;
        g_mock_hal_get_pages_call_count = 0;
        g_fail_dma_map                  = false;
        g_dma_map_fail_index            = -1;
        g_dma_map_call_count            = 0;
        g_dma_unmap_call_count          = 0;
        svm_context_sem.read_locked     = 0;
        svm_context_sem.write_locked    = 0;

        init_rwsem(&svm_context_sem);
        valid_ctx = kzalloc(sizeof(struct svm_agent_context), GFP_KERNEL);
        ASSERT_NE(valid_ctx, nullptr);

        struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;

        mutex_init(&sctx->context_mutex);
        sctx->va                 = TEST_VA_BASE;
        sctx->len                = TEST_LEN;
        sctx->aligned_size       = TEST_LEN;
        sctx->va_aligned_start   = TEST_VA_BASE;
        sctx->va_aligned_end     = TEST_VA_END;
        sctx->page_size          = TEST_PAGE_SIZE;
        sctx->inited_flag        = 1;
        sctx->get_flag           = 0;
        sctx->page_table         = nullptr;
        sctx->sg_head            = nullptr;
        sctx->dma_addr_list      = nullptr;
        sctx->dma_mapped_count   = 0;
        sctx->process_id.hostpid = TEST_HOST_PID_ONE_TWO_THREE_FOUR;
        sctx->process_id.devid   = TEST_DEV_ID;
        sctx->process_id.vfid    = TEST_VF_ID;
        sctx->pa_list            = nullptr;

        struct sg_table dummy_sg {};
        int ret = ndr_mem_get_pages(0x10000000UL, 4096 * 5, 0, 0, &dummy_sg, valid_ctx, 0);
        ASSERT_EQ(ret, 0);
        ASSERT_NE(sctx->page_table, nullptr);
    }

    void TearDown() override
    {
        svm_context_sem.read_locked  = 0;
        svm_context_sem.write_locked = 0;
        if (valid_ctx) {
            struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;

            if (sctx->sg_head) {
                sg_free_table(sctx->sg_head);
                sctx->sg_head = nullptr;
            }

            if (sctx->dma_addr_list) {
                kfree(sctx->dma_addr_list);
                sctx->dma_addr_list = nullptr;
            }

            if (sctx->page_table) {
                mock_hal_put_pages(sctx->page_table);
                sctx->page_table = nullptr;
            }

            kfree(valid_ctx);
            valid_ctx = nullptr;
        }

        if (sg_head.sgl) {
            sg_free_table(&sg_head);
        }
    }
};

TEST_F(NDRDmaMapTest, DmaMapFail_NullParams)
{
    int ret;

    ret = ndr_mem_dma_map(nullptr, valid_ctx, &dma_dev, 0, &nmap);
    EXPECT_EQ(ret, -EINVAL);

    ret = ndr_mem_dma_map(&sg_head, nullptr, &dma_dev, 0, &nmap);
    EXPECT_EQ(ret, -EINVAL);

    ret = ndr_mem_dma_map(&sg_head, valid_ctx, &dma_dev, 0, nullptr);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(NDRDmaMapTest, DmaMapFail_ContextNotInited)
{
    struct svm_agent_context bad_ctx {};
    bad_ctx.inited_flag = 0;

    int ret = ndr_mem_dma_map(&sg_head, &bad_ctx, &dma_dev, 0, &nmap);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(NDRDmaMapTest, DmaMapFail_PageTableNotReady)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;

    if (sctx->page_table) {
        mock_hal_put_pages(sctx->page_table);
        sctx->page_table = nullptr;
    }

    int ret = ndr_mem_dma_map(&sg_head, valid_ctx, &dma_dev, 0, &nmap);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(NDRDmaMapTest, DmaMapFail_DoubleMapping)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;

    int ret1 = ndr_mem_dma_map(&sg_head, valid_ctx, &dma_dev, 0, &nmap);
    EXPECT_EQ(ret1, 0);
    EXPECT_NE(sctx->sg_head, nullptr);

    struct sg_table sg_head2 {};

    int ret2 = ndr_mem_dma_map(&sg_head2, valid_ctx, &dma_dev, 0, &nmap);
    EXPECT_EQ(ret2, -EBUSY);

    if (sg_head2.sgl) {
        sg_free_table(&sg_head2);
    }
}

TEST_F(NDRDmaMapTest, DmaMapFail_KzallocDmaAddrList)
{
    g_fail_kcalloc = true;

    int ret = ndr_mem_dma_map(&sg_head, valid_ctx, &dma_dev, 0, &nmap);

    EXPECT_EQ(ret, -ENOMEM);

    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;
    EXPECT_EQ(sctx->dma_addr_list, nullptr);
    EXPECT_EQ(sctx->dma_mapped_count, 0);
}

TEST_F(NDRDmaMapTest, DmaMapFail_SgAllocTable)
{
    g_fail_sg_alloc_table = true;

    int ret = ndr_mem_dma_map(&sg_head, valid_ctx, &dma_dev, 0, &nmap);
    EXPECT_EQ(ret, -ENOMEM);

    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;
    EXPECT_EQ(sctx->sg_head, nullptr);
    EXPECT_EQ(sctx->dma_mapped_count, 0);
}

TEST_F(NDRDmaMapTest, DmaMapSuccess_Normal)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;

    g_dma_map_call_count   = 0;
    g_dma_unmap_call_count = 0;
    g_fail_kcalloc         = false;
    g_fail_sg_alloc_table  = false;

    int ret = ndr_mem_dma_map(&sg_head, valid_ctx, &dma_dev, 0, &nmap);
    EXPECT_EQ(ret, 0);
}

TEST_F(NDRDmaMapTest, DmaMapFail_PartialMappingRollback)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;

    g_fail_kcalloc         = false;
    g_fail_dma_map         = true;
    g_dma_map_fail_index   = TEST_DMA_MAP_FAIL_INDEX;
    g_dma_map_call_count   = 0;
    g_dma_unmap_call_count = 0;

    int ret = ndr_mem_dma_map(&sg_head, valid_ctx, &dma_dev, 0, &nmap);

    EXPECT_EQ(ret, -EFAULT);
    EXPECT_EQ(g_dma_map_call_count, EXPECTED_DMA_MAP_COUNT_THREE);
    EXPECT_EQ(g_dma_unmap_call_count, EXPECTED_DMA_MAP_COUNT_TWO);
    EXPECT_EQ(sctx->dma_mapped_count, 0);
    EXPECT_EQ(sctx->dma_addr_list, nullptr);
    EXPECT_EQ(sctx->sg_head, nullptr);
    EXPECT_EQ(sg_head.sgl, nullptr);
}

TEST_F(NDRDmaMapTest, DmaMapFail_FirstMapping)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;

    g_fail_dma_map         = true;
    g_dma_map_fail_index   = 0;
    g_dma_map_call_count   = 0;
    g_dma_unmap_call_count = 0;

    int ret = ndr_mem_dma_map(&sg_head, valid_ctx, &dma_dev, 0, &nmap);

    EXPECT_EQ(ret, -EFAULT);
    EXPECT_EQ(g_dma_map_call_count, 1);
    EXPECT_EQ(g_dma_unmap_call_count, 0);
    EXPECT_EQ(sctx->dma_mapped_count, 0);
    EXPECT_EQ(sctx->dma_addr_list, nullptr);
    EXPECT_EQ(sctx->sg_head, nullptr);
}

TEST_F(NDRDmaMapTest, DmaMapSuccess_ManyPages)
{
    void *large_ctx = kzalloc(sizeof(struct svm_agent_context), GFP_KERNEL);
    ASSERT_NE(large_ctx, nullptr);

    struct svm_agent_context *sctx = (struct svm_agent_context *)large_ctx;
    mutex_init(&sctx->context_mutex);
    sctx->va               = 0x20000000UL;
    sctx->len              = NPU_PAGE_SIZE_ONE_HUNDRED;
    sctx->aligned_size     = NPU_PAGE_SIZE_ONE_HUNDRED;
    sctx->va_aligned_start = 0x20000000UL;
    sctx->va_aligned_end   = 0x20019000UL;
    sctx->page_size        = NPU_PAGE_SIZE;
    sctx->inited_flag      = 1;
    sctx->get_flag         = 0;
    sctx->page_table       = nullptr;
    sctx->sg_head          = nullptr;
    sctx->dma_addr_list    = nullptr;
    sctx->dma_mapped_count = 0;

    struct sg_table dummy_sg {};
    int ret = ndr_mem_get_pages(0x20000000UL, 4096 * 100, 0, 0, &dummy_sg, large_ctx, 0);
    ASSERT_EQ(ret, 0);

    struct sg_table large_sg {};
    int large_nmap = 0;

    ret = ndr_mem_dma_map(&large_sg, large_ctx, &dma_dev, 0, &large_nmap);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(large_nmap, TEST_SMALL_PAGE_COUNT);
    EXPECT_EQ(g_dma_map_call_count, TEST_SMALL_PAGE_COUNT);

    if (large_sg.sgl) {
        sg_free_table(&large_sg);
    }
    if (sctx->page_table) {
        mock_hal_put_pages(sctx->page_table);
    }
    if (sctx->dma_addr_list) {
        kfree(sctx->dma_addr_list);
    }
    kfree(large_ctx);
}

// ============================================================
// PutPages Fixture + Tests
// ============================================================
class NDRPutPagesTest : public ::testing::Test {
protected:
    void *valid_ctx = nullptr;
    struct sg_table sg_head {};

    void SetUp() override
    {
        hal_get_pages_func = mock_hal_get_pages;
        hal_put_pages_func = mock_hal_put_pages;

        svm_context_sem.read_locked  = 0;
        svm_context_sem.write_locked = 0;

        valid_ctx = kzalloc(sizeof(struct svm_agent_context), GFP_KERNEL);
        ASSERT_NE(valid_ctx, nullptr);

        struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;

        mutex_init(&sctx->context_mutex);
        sctx->va                 = 0x10000000UL;
        sctx->len                = NPU_PAGE_SIZE_FIVE;
        sctx->aligned_size       = NPU_PAGE_SIZE_FIVE;
        sctx->va_aligned_start   = 0x10000000UL;
        sctx->va_aligned_end     = 0x10005000UL;
        sctx->page_size          = NPU_PAGE_SIZE;
        sctx->inited_flag        = 1;
        sctx->get_flag           = 0;
        sctx->process_id.hostpid = TEST_HOST_PID_ONE_TWO_THREE_FOUR;
        sctx->process_id.devid   = 1;
        sctx->process_id.vfid    = TEST_PROCESS_ID_VID;

        struct sg_table dummy_sg {};
        int ret = ndr_mem_get_pages(0x10000000UL, 4096 * 5, 0, 0, &dummy_sg, valid_ctx, 0);
        ASSERT_EQ(ret, 0);
        ASSERT_NE(sctx->page_table, nullptr);
        ASSERT_NE(sctx->page_table, nullptr);
    }

    void TearDown() override
    {
        if (valid_ctx) {
            struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;
            sctx->context_mutex.locked     = 0;

            if (sctx->page_table) {
                mock_hal_put_pages(sctx->page_table);
                sctx->page_table = nullptr;
            }

            kfree(valid_ctx);
            valid_ctx = nullptr;
        }

        svm_context_sem.read_locked  = 0;
        svm_context_sem.write_locked = 0;
    }
};

TEST_F(NDRPutPagesTest, PutPages_NullContext)
{
    int original_mock_hal_call_count = g_mock_hal_get_pages_call_count;

    ndr_mem_put_pages(&sg_head, nullptr);

    EXPECT_EQ(g_mock_hal_get_pages_call_count, original_mock_hal_call_count);
    EXPECT_EQ(svm_context_sem.read_locked, 0);
}

TEST_F(NDRPutPagesTest, PutPages_HalFailure)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)valid_ctx;

    g_hal_put_pages_should_fail     = true;
    g_hal_put_pages_ret_val         = -EINVAL;
    g_mock_hal_put_pages_call_count = 0;

    struct p2p_page_table *original_page_table = sctx->page_table;
    EXPECT_NE(original_page_table, nullptr);

    ndr_mem_put_pages(&sg_head, valid_ctx);

    EXPECT_EQ(g_mock_hal_put_pages_call_count, 1);
    EXPECT_EQ(svm_context_sem.read_locked, 0);
    EXPECT_EQ(svm_context_sem.write_locked, 0);
    EXPECT_EQ(sctx->context_mutex.locked, 0);

    g_hal_put_pages_should_fail = false;
    g_hal_put_pages_ret_val     = 0;

    if (sctx->page_table) {
        g_hal_put_pages_should_fail = false;
        mock_hal_put_pages(sctx->page_table);
        sctx->page_table = nullptr;
    }
}

TEST_F(NDRPutPagesTest, PutPages_ContextNotInited)
{
    void *uninit_ctx = kzalloc(sizeof(struct svm_agent_context), GFP_KERNEL);
    ASSERT_NE(uninit_ctx, nullptr);

    struct svm_agent_context *sctx = (struct svm_agent_context *)uninit_ctx;
    mutex_init(&sctx->context_mutex);
    sctx->inited_flag = 0;
    sctx->page_table  = nullptr;

    ndr_mem_put_pages(&sg_head, uninit_ctx);
    kfree(uninit_ctx);
}

// ============================================================
// 9. DmaUnmap Fixture + Tests
// ============================================================
class NDRDmaUnmapTest : public ::testing::Test {
protected:
    void *ctx = nullptr;
    struct sg_table mapped_sg {};
    struct device mock_dev {};

    void SetUp() override
    {
        hal_get_pages_func     = mock_hal_get_pages;
        g_fail_dma_map         = false;
        g_dma_map_fail_index   = -1;
        g_dma_map_call_count   = 0;
        g_dma_unmap_call_count = 0;
        g_sg_free_call_count   = 0;

        unsigned long va = 0x10000000UL;
        size_t size      = 4096 * 5;
        bool acquire_ret = ndr_mem_acquire(va, size, nullptr, nullptr, &ctx);
        ASSERT_TRUE(acquire_ret);

        sg_alloc_table(&mapped_sg, 5, 0);

        int nmap    = 0;
        int map_ret = ndr_mem_dma_map(&mapped_sg, ctx, &mock_dev, 0, &nmap);
        ASSERT_EQ(map_ret, 0);
        ASSERT_EQ(nmap, 5);
        ASSERT_EQ(g_dma_map_call_count, 5);
    }

    void TearDown() override
    {
        if (ctx) {
            ndr_mem_release(ctx);
            ctx = nullptr;
        }
        g_dma_unmap_call_count = 0;
        g_sg_free_call_count   = 0;
    }
};

TEST_F(NDRDmaUnmapTest, UnmapFail_NullCtx)
{
    struct sg_table dummy_sg {};
    sg_alloc_table(&dummy_sg, 1, 0);

    int ret = ndr_mem_dma_unmap(&dummy_sg, nullptr, &mock_dev);
    EXPECT_EQ(ret, -EINVAL);
    EXPECT_EQ(g_dma_unmap_call_count, 0);
    EXPECT_EQ(g_sg_free_call_count, 0);

    sg_free_table(&dummy_sg);
}

TEST_F(NDRDmaUnmapTest, UnmapFail_NullSgHead)
{
    int ret = ndr_mem_dma_unmap(nullptr, ctx, &mock_dev);
    EXPECT_EQ(ret, -EINVAL);
    EXPECT_EQ(g_dma_unmap_call_count, 0);
    EXPECT_EQ(g_sg_free_call_count, 0);
}

TEST_F(NDRDmaUnmapTest, UnmapFail_NotInitialized)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)ctx;
    sctx->inited_flag              = 0;

    int ret = ndr_mem_dma_unmap(&mapped_sg, ctx, &mock_dev);
    EXPECT_EQ(ret, -EINVAL);
    EXPECT_EQ(g_dma_unmap_call_count, 0);
    EXPECT_EQ(g_sg_free_call_count, 0);
}

TEST_F(NDRDmaUnmapTest, UnmapFail_SgHeadMismatch)
{
    struct sg_table wrong_sg {};
    sg_alloc_table(&wrong_sg, 1, 0);

    int ret = ndr_mem_dma_unmap(&wrong_sg, ctx, &mock_dev);
    EXPECT_EQ(ret, -EINVAL);
    EXPECT_EQ(g_dma_unmap_call_count, 0);
    EXPECT_EQ(g_sg_free_call_count, 0);

    sg_free_table(&wrong_sg);
}

TEST_F(NDRDmaUnmapTest, UnmapFail_PageTableNull)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)ctx;
    sctx->page_table               = nullptr;

    int ret = ndr_mem_dma_unmap(&mapped_sg, ctx, &mock_dev);
    EXPECT_EQ(ret, -EINVAL);
    EXPECT_EQ(g_dma_unmap_call_count, 0);
    EXPECT_EQ(g_sg_free_call_count, 0);
}

TEST_F(NDRDmaUnmapTest, UnmapFail_PageSizeZero)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)ctx;
    sctx->page_table->page_size    = 0;

    int ret = ndr_mem_dma_unmap(&mapped_sg, ctx, &mock_dev);
    EXPECT_EQ(ret, -EINVAL);
    EXPECT_EQ(g_dma_unmap_call_count, 0);
    EXPECT_EQ(g_sg_free_call_count, 0);
}

TEST_F(NDRDmaUnmapTest, UnmapFail_NoDeviceFallback)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)ctx;
    sctx->dma_device               = nullptr;

    int ret = ndr_mem_dma_unmap(&mapped_sg, ctx, nullptr);
    EXPECT_EQ(ret, -EINVAL);
    EXPECT_EQ(g_dma_unmap_call_count, 0);
    EXPECT_EQ(g_sg_free_call_count, 0);
}

TEST_F(NDRDmaUnmapTest, UnmapSuccess_WithFallbackDevice)
{
    struct svm_agent_context *sctx = (struct svm_agent_context *)ctx;
    sctx->dma_device               = nullptr;

    struct device fallback_dev {};
    int ret = ndr_mem_dma_unmap(&mapped_sg, ctx, &fallback_dev);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(g_dma_unmap_call_count, 5);
    EXPECT_EQ(g_sg_free_call_count, 1);
    EXPECT_EQ(sctx->dma_mapped_count, 0);
    EXPECT_EQ(sctx->dma_addr_list, nullptr);
    EXPECT_EQ(sctx->sg_head, nullptr);
}

TEST_F(NDRDmaUnmapTest, UnmapSuccess_Normal)
{
    int ret = ndr_mem_dma_unmap(&mapped_sg, ctx, &mock_dev);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(g_dma_unmap_call_count, 5);
    EXPECT_EQ(g_sg_free_call_count, 1);

    struct svm_agent_context *sctx = (struct svm_agent_context *)ctx;
    EXPECT_EQ(sctx->dma_mapped_count, 0);
    EXPECT_EQ(sctx->dma_addr_list, nullptr);
    EXPECT_EQ(sctx->sg_head, nullptr);
}

TEST_F(NDRDmaUnmapTest, UnmapSuccess_NoMapping)
{
    ndr_mem_dma_unmap(&mapped_sg, ctx, &mock_dev);

    g_dma_unmap_call_count = 0;
    g_sg_free_call_count   = 0;

    int ret = ndr_mem_dma_unmap(&mapped_sg, ctx, &mock_dev);
    EXPECT_EQ(ret, -EINVAL);
    EXPECT_EQ(g_dma_unmap_call_count, 0);
    EXPECT_EQ(g_sg_free_call_count, 0);
}

TEST_F(NDRDmaUnmapTest, UnmapSuccess_DeviceMismatchWarn)
{
    struct device wrong_dev {};
    int ret = ndr_mem_dma_unmap(&mapped_sg, ctx, &wrong_dev);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(g_dma_unmap_call_count, 5);
    EXPECT_EQ(g_sg_free_call_count, 1);
}