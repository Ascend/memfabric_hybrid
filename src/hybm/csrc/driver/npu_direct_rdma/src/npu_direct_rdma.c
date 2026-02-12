/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* MemFabric_Hybrid is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/limits.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>
#include <linux/types.h>
#include <linux/iommu.h>
#include <linux/uaccess.h>
#include <linux/kprobes.h>

#include <rdma/peer_mem.h>
#include "ascend_kernel_hal.h"

#define DRV_ROCE_PEER_MEM_NAME "NDR_PEER_MEM"
#define DRV_ROCE_PEER_MEM_VERSION "0.0.1"

#define logflow_printk(level, module, fmt, ...) \
    (void)printk(level "[NDRPeerMem] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)
#define drv_err(module, fmt...) logflow_printk(KERN_ERR, module, fmt)
#define drv_info(module, fmt...) logflow_printk(KERN_INFO, module, fmt)
#define NDR_PEER_MEM_ERR(fmt, ...) drv_err("NDR_peer_mem", "<%d,%d> " fmt, current->tgid, current->pid, ##__VA_ARGS__)
#define NDR_PEER_MEM_INFO(fmt, ...) drv_info("NDR_peer_mem", "<%d,%d> " fmt, current->tgid, current->pid, ##__VA_ARGS__)

typedef void (*free_callback_t)(void *data);
typedef int (*hal_get_pages_t)(u64, u64, free_callback_t, void *, struct p2p_page_table **);
static hal_get_pages_t hal_get_pages_func = NULL;

typedef int (*hal_put_pages_t)(struct p2p_page_table *);
static hal_put_pages_t hal_put_pages_func = NULL;

uint64_t (*g_kallsymsLookupName)(const char *) = NULL;

#define NPU_PAGE_SHIFT 12
#define NPU_PAGE_SIZE ((u64)1 << NPU_PAGE_SHIFT)
#define NPU_PAGE_OFFSET (NPU_PAGE_SIZE - 1)
#define NPU_PAGE_MASK (~NPU_PAGE_OFFSET)

struct process_id {
    int hostpid;
    unsigned int devid;
    unsigned int vfid;
};

// svm agent context
struct svm_agent_context {
    struct p2p_page_table *page_table;
    struct device *dma_device;  // The device used when saving the mapping
    dma_addr_t *dma_addr_list;  // Save the DMA address array after each page mapping
    size_t dma_mapped_count;    // The number of pages that have been successfully mapped, used for error rollback
    struct mutex context_mutex;
    u64 core_context;
    struct sg_table *sg_head;
    uint32_t inited_flag;
    uint64_t va;
    size_t len;
    uint64_t va_aligned_start;
    uint64_t va_aligned_end;
    uint64_t aligned_size;
    uint32_t page_size;
    uint32_t pa_num;

    int get_flag;
    struct process_id process_id;
    void *pa_list;
};

DECLARE_RWSEM(svm_context_sem);

unsigned long ndr_mem_get_page_size(void *client_context)
{
    struct svm_agent_context *svm_agent_context = (struct svm_agent_context *)client_context;
    unsigned long page_size;
    down_read(&svm_context_sem);
    if ((svm_agent_context == NULL) || (svm_agent_context->inited_flag != 1)) {
        up_read(&svm_context_sem);
        NDR_PEER_MEM_ERR("svm_agent_context is NULL.\n");
        return 0;
    }
    page_size = svm_agent_context->page_size;
    up_read(&svm_context_sem);
    return page_size;
}

/* At that function we don't call IB core - no ticket exists */
static void ndr_mem_dummy_callback(void *data)
{
    __module_get(THIS_MODULE);

    module_put(THIS_MODULE);
    return;
}

/**
 * @brief Register an NPU memory region for RDMA peer access and create its management context.
 *
 * This function implements the acquire_peer_mem() callback for the MLNX_OFED peer memory client
 * interface. It registers a virtual memory region residing in NPU device memory for direct RDMA
 * access by CX-series NICs. The function performs critical address alignment to NPU page boundaries
 * (4KB), allocates an SVM (Shared Virtual Memory) context to track the region's metadata, and
 * delegates physical page table acquisition to the HAL layer via hal_get_pages_func(). Upon
 * successful registration, it increments the kernel module reference count to prevent unloading
 * while peer memory contexts remain active. The allocated context is returned via client_context
 * for subsequent get_pages()/put_pages() operations in the RDMA workflow.
 *
 * Key behaviors:
 * - Input VA is aligned downward to NPU_PAGE_MASK; end address aligned upward to ensure
 *   hardware-compliant DMA boundaries for CX-7 NICs.
 * - Page table acquisition is delegated to HAL layer which queries NPU MMU for physical pages.
 * - Module reference counting (__module_get) prevents driver unload during active RDMA transfers.
 * - Thread-safe context initialization via mutex_init() for subsequent concurrent operations.
 *
 * Note: peer_mem_data and peer_mem_name parameters are part of the MLNX_OFED interface contract
 * but unused in this implementation as NPU memory identification relies solely on VA range.
 *
 * @param va              Virtual address of the NPU memory region (device memory space).
 * @param size            Size of the memory region in bytes.
 * @param peer_mem_data   Reserved parameter per MLNX_OFED interface; unused in this implementation.
 * @param peer_mem_name   Reserved parameter per MLNX_OFED interface; unused in this implementation.
 * @param[out] client_context  Output parameter receiving the allocated svm_agent_context pointer.
 *                             Must be non-NULL; caller must release via ndr_mem_release() later.
 * @return                1 (true) on success; 0 (false) on failure (e.g., allocation failure,
 *                        HAL page acquisition error). Note: Function returns bool values despite
 *                        int declaration per kernel callback convention.
 */
int ndr_mem_acquire(unsigned long va, size_t size, void *peer_mem_data, char *peer_mem_name, void **client_context)
{
    struct svm_agent_context *svm_agent_context = NULL;
    int ret                                     = 0;
    unsigned long end_va;

    if (!client_context) {
        NDR_PEER_MEM_ERR("client_context is NULL\n");
        return false;
    }

    svm_agent_context = (struct svm_agent_context *)kzalloc(sizeof(*svm_agent_context), GFP_KERNEL);
    if (!svm_agent_context) {
        NDR_PEER_MEM_INFO("kzalloc svm_agent_context failed\n");
        return false;
    }

    end_va = va + size;
    if (end_va < va) {
        NDR_PEER_MEM_ERR("va + size overflow: va=%lx, size=%zu\n", va, size);
        kfree(svm_agent_context);
        return false;
    }

    NDR_PEER_MEM_INFO("va=%pK, size=%zu\n", (void *)va, size);
    svm_agent_context->va               = va;
    svm_agent_context->len              = size;
    svm_agent_context->sg_head          = NULL;
    svm_agent_context->va_aligned_start = va & NPU_PAGE_MASK;
    svm_agent_context->inited_flag      = 1;
    svm_agent_context->va_aligned_end   = (va + size + NPU_PAGE_SIZE - 1) & NPU_PAGE_MASK;
    svm_agent_context->aligned_size     = svm_agent_context->va_aligned_end - svm_agent_context->va_aligned_start;
    if (svm_agent_context->aligned_size < size) {
        NDR_PEER_MEM_ERR("aligned_size (%llu) < size (%zu), invalid\n", svm_agent_context->aligned_size, size);
        kfree(svm_agent_context);
        return false;
    }
    mutex_init(&svm_agent_context->context_mutex);

    ret = hal_get_pages_func(svm_agent_context->va_aligned_start, svm_agent_context->aligned_size,
                             ndr_mem_dummy_callback, svm_agent_context, &svm_agent_context->page_table);
    if (ret != 0) {
        NDR_PEER_MEM_ERR("hal_get_pages_func failed: %d\n", ret);
        kfree(svm_agent_context);
        return false;
    }
    if (!svm_agent_context->page_table) {
        NDR_PEER_MEM_ERR("page_table is NULL after hal_get_pages_func\n");
        kfree(svm_agent_context);
        return false;
    }
    /* Set page table information */
    svm_agent_context->pa_num    = svm_agent_context->page_table->page_num;
    svm_agent_context->page_size = svm_agent_context->page_table->page_size;

    NDR_PEER_MEM_INFO("pa_num=%u page_size=%u, aligned_size=%llu\n", svm_agent_context->pa_num,
                      svm_agent_context->page_size, svm_agent_context->aligned_size);

    *client_context = (void *)svm_agent_context;
    __module_get(THIS_MODULE);
    NDR_PEER_MEM_INFO("Acquire success for size %zu\n", size);

    return true;
}

/**
 * @brief Acquire the page table for a remote peer memory region.
 *
 * This function retrieves the physical page table corresponding to the given virtual address
 * and size from a pre-registered SVM (Shared Virtual Memory) context. It ensures idempotency
 * by skipping re-acquisition if the page table is already present, and validates input
 * parameters against the context. The actual page table acquisition is delegated to a HAL
 * (Hardware Abstraction Layer) callback.
 *
 * @param va        Virtual address of the memory region.
 * @param size      Size of the memory region in bytes.
 * @param write     Write permission flag (currently unused).
 * @param force     Force re-acquisition flag (currently unused).
 * @param sg_head   Scatter-gather table pointer (currently unused).
 * @param context   Pointer to the initialized svm_agent_context.
 * @param core_context  Core-specific context (currently unused).
 * @return          0 on success, negative errno on failure.
 */
int ndr_mem_get_pages(unsigned long va, size_t size, int write, int force, struct sg_table *sg_head, void *context,
                      u64 core_context)
{
    struct svm_agent_context *ctx = (struct svm_agent_context *)context;
    int ret                       = 0;

    NDR_PEER_MEM_INFO("va=%pK, size=%zu\n", (void *)va, size);
    // 1. Basic verification
    if (!ctx || ctx->inited_flag != 1) {
        NDR_PEER_MEM_ERR("svm_agent_context is NULL or not initialized.\n");
        return -EINVAL;
    }

    if (va != ctx->va || size != ctx->len) {
        NDR_PEER_MEM_ERR("Parameters mismatch! ctx_va=%pK/%zu, input_va=%pK/%zu", (void *)ctx->va, ctx->len, (void *)va,
                         size);
        return -EINVAL;
    }

    mutex_lock(&ctx->context_mutex);  // Use only the internal mutex of the context

    // 2. Check if page table is already acquired (idempotency)
    if (ctx->page_table) {
        NDR_PEER_MEM_INFO("Page table already exists; skipping redundant acquisition.");
        mutex_unlock(&ctx->context_mutex);
        return 0;
    }

    // 4. Call HAL interface to acquire page table
    ret = hal_get_pages_func(va, size, ndr_mem_dummy_callback, ctx, &ctx->page_table);
    if (ret != 0) {
        NDR_PEER_MEM_ERR("hal_get_pages_func failed, ret=%d", ret);
        mutex_unlock(&ctx->context_mutex);
        return ret;
    }

    // 5. Record and log key information
    NDR_PEER_MEM_INFO("GetPages succeeded: page_num=%u, page_size=%u", (unsigned int)ctx->page_table->page_num,
                      (unsigned int)ctx->page_table->page_size);

    // 6. Alignment check (based on actual page size)
    if (va & (ctx->page_table->page_size - 1)) {
        NDR_PEER_MEM_ERR("va=%pK is not aligned to page_size=%zu", (void *)va, (size_t)ctx->page_table->page_size);
        // Note: 4KB alignment is sufficient per constraints; this is a warning only.
    }

    mutex_unlock(&ctx->context_mutex);
    return 0;
}

/**
 * @brief Maps remote peer memory pages to DMA address space for device access.
 *
 * This function performs DMA mapping of pre-acquired physical pages (from a P2P page table)
 * into the given device's DMA address space. It allocates a scatter-gather table,
 * maps each physical page via dma_map_resource(), and stores the resulting DMA addresses.
 * The operation is idempotent and includes comprehensive error handling with partial rollback.
 *
 * @param sg_head       Pointer to the scatter-gather table to be filled.
 * @param agent_ctx     Pointer to the initialized svm_agent_context containing page table.
 * @param dma_device    The DMA-capable device requesting the mapping.
 * @param dmasync       DMA synchronization flag (currently unused), dmasync is obsolete, always 0
 * @param nmap          Output: number of successfully mapped pages.
 * @return              0 on success, negative errno on failure.
 */
int ndr_mem_dma_map(struct sg_table *sg_head, void *agent_ctx, struct device *dma_device, int dmasync, int *nmap)
{
    int i;
    int ret                                     = 0;
    struct scatterlist *sg                      = NULL;
    struct svm_agent_context *svm_agent_context = (struct svm_agent_context *)agent_ctx;
    struct p2p_page_table *page_table           = NULL;
    dma_addr_t dma_addr;
    u32 page_num;
    u64 page_size;

    // 1. Input validation
    if (!svm_agent_context || !sg_head || !nmap || !dma_device) {
        NDR_PEER_MEM_ERR("Invalid input params: ctx=%pK, sg=%pK, nmap=%pK, dma_device=%pK\n", svm_agent_context,
                         sg_head, nmap, dma_device);
        return -EINVAL;
    }

    down_read(&svm_context_sem);
    if (svm_agent_context->inited_flag != 1) {
        NDR_PEER_MEM_ERR("Context not initialized.\n");
        up_read(&svm_context_sem);
        return -EINVAL;
    }

    mutex_lock(&svm_agent_context->context_mutex);

    // 2. State check (idempotency)
    page_table = svm_agent_context->page_table;
    if (!page_table) {
        NDR_PEER_MEM_ERR("Page table not ready\n");
        ret = -EINVAL;
        goto err_unlock;
    }
    if (svm_agent_context->sg_head) {
        NDR_PEER_MEM_ERR("sg_head already allocated\n");
        ret = -EBUSY;
        goto err_unlock;
    }

    // 3. Prepare mapping parameters
    page_num  = page_table->page_num;
    page_size = page_table->page_size;
    WARN_ON(page_num == 0 || page_size == 0);

    svm_agent_context->dma_device    = dma_device;
    svm_agent_context->dma_addr_list = kcalloc(page_num, sizeof(dma_addr_t), GFP_KERNEL);
    if (!svm_agent_context->dma_addr_list) {
        NDR_PEER_MEM_ERR("Failed to allocate dma_addr_list for %u pages\n", page_num);
        ret = -ENOMEM;
        goto err_unlock;
    }
    svm_agent_context->dma_mapped_count = 0;

    // 4. Allocate Scatter-Gather table
    ret = sg_alloc_table(sg_head, page_num, GFP_KERNEL);
    if (ret) {
        NDR_PEER_MEM_ERR("sg_alloc_table failed for %u pages, ret=%d\n", page_num, ret);
        goto err_free_list;
    }

    // 5. map each physical page to DMA address space
    for_each_sg(sg_head->sgl, sg, page_num, i)
    {
        dma_addr = dma_map_resource(dma_device, page_table->pages_info[i].pa, page_size, DMA_BIDIRECTIONAL, 0);
        if (dma_mapping_error(dma_device, dma_addr)) {
            dev_err(dma_device, "NDRPeerMem: dma_map_resource failed at index %d, PA=0x%016llx\n", i,
                    page_table->pages_info[i].pa);
            ret = -EFAULT;
            // Current page i failed; pages [0, i-1] were successfully mapped
            svm_agent_context->dma_mapped_count = i;  // Record successful count
            goto err_unmap_partial;
        }

        svm_agent_context->dma_addr_list[i] = dma_addr;
        sg_dma_address(sg)                  = dma_addr;
        sg_dma_len(sg)                      = page_size;

        sg->page_link = 0UL;
        sg->length    = (unsigned int)page_size;
        sg->offset    = 0;

        svm_agent_context->dma_mapped_count++;  // Increment only on success
    }

    // 6. Success: set final state
    svm_agent_context->sg_head = sg_head;
    *nmap                      = page_num;
    NDR_PEER_MEM_INFO("DmaMap success for %u pages, total size %llu bytes\n", page_num, (u64)page_num * page_size);

    mutex_unlock(&svm_agent_context->context_mutex);
    up_read(&svm_context_sem);
    return 0;

// 7.  Error handling (strict reverse-order resource cleanup)
err_unmap_partial:
    // Roll back partially successful DMA mapping
    for (i = 0; i < svm_agent_context->dma_mapped_count; i++) {
        dma_unmap_resource(svm_agent_context->dma_device, svm_agent_context->dma_addr_list[i], page_size,
                           DMA_BIDIRECTIONAL, 0);
    }
    sg_free_table(sg_head);
err_free_list:
    kfree(svm_agent_context->dma_addr_list);
    svm_agent_context->dma_addr_list    = NULL;
    svm_agent_context->dma_mapped_count = 0;
err_unlock:
    mutex_unlock(&svm_agent_context->context_mutex);
    up_read(&svm_context_sem);
    return ret;
}

/**
 * @brief Unmaps DMA-mapped remote peer memory and releases associated resources.
 *
 * This function safely unmaps previously mapped DMA addresses (from ndr_mem_dma_map),
 * frees the scatter-gather table, and resets the context state. It includes robust
 * validation to ensure consistency between input parameters and the stored context,
 * and handles partial or already-unmapped states gracefully.
 *
 * @param sg_head       Pointer to the scatter-gather table to be unmapped.
 * @param context       Pointer to the svm_agent_context containing mapping state.
 * @param dma_device    The DMA-capable device (used for validation; actual unmap uses context's device).
 * @return              0 on success, negative errno on failure.
 */
int ndr_mem_dma_unmap(struct sg_table *sg_head, void *context, struct device *dma_device)
{
    struct svm_agent_context *ctx = (struct svm_agent_context *)context;
    struct p2p_page_table *pt     = NULL;
    int i;
    int ret                     = 0;
    u64 page_size               = 0;
    struct device *unmap_device = NULL;

    /* 1. Basic parameter validation */
    if (!ctx || !sg_head) {
        NDR_PEER_MEM_ERR("Invalid input: ctx=%pK, sg_head=%pK\n", ctx, sg_head);
        return -EINVAL;
    }

    down_read(&svm_context_sem);
    /* 2.  Context state validation */
    if (ctx->inited_flag != 1) {
        NDR_PEER_MEM_ERR("Context not initialized or already released.\n");
        ret = -EINVAL;
        goto out_unlock_sem;
    }

    mutex_lock(&ctx->context_mutex);

    /* 3. Mapping consistency check */
    if (sg_head != ctx->sg_head) {
        NDR_PEER_MEM_ERR("sg_head mismatch. ctx->sg_head=%pK, input=%pK\n", ctx->sg_head, sg_head);
        ret = -EINVAL;
        goto out_unlock_mutex;
    }
    /* Device consistency check. RDMA core usually passes the correct device. */
    if (dma_device && ctx->dma_device && dma_device != ctx->dma_device) {
        WARN_ONCE(1, "NDRPeerMem: dma_device mismatch. Stored:%pK, Passed:%pK\n", ctx->dma_device, dma_device);
    }

    pt = ctx->page_table;
    if (!pt) {
        NDR_PEER_MEM_ERR("Page table is NULL\n");
        ret = -EINVAL;
        goto out_unlock_mutex;
    }

    page_size = pt->page_size;
    if (page_size == 0) {
        NDR_PEER_MEM_ERR("Invalid page size: 0\n");
        ret = -EINVAL;
        goto out_unlock_mutex;
    }

    /* 4. Determine the device to be used for demapping */
    unmap_device = ctx->dma_device;
    if (!unmap_device) {
        if (dma_device) {
            unmap_device = dma_device;
            NDR_PEER_MEM_INFO("Using passed dma_device=%pK as stored device is NULL\n", dma_device);
        } else {
            NDR_PEER_MEM_ERR("No valid DMA device available for unmapping\n");
            ret = -EINVAL;
            goto out_unlock_mutex;
        }
    }

    if (ctx->dma_addr_list && ctx->dma_mapped_count > 0) {
        for (i = 0; i < ctx->dma_mapped_count; i++) {
            if (ctx->dma_addr_list[i]) {
                dma_unmap_resource(ctx->dma_device, ctx->dma_addr_list[i], page_size, DMA_BIDIRECTIONAL, 0);
                ctx->dma_addr_list[i] = 0;
            }
        }
        NDR_PEER_MEM_INFO("Unmapped %ld DMA pages\n", ctx->dma_mapped_count);
    }

    /* 5. Release resources and reset state */
    if (ctx->dma_addr_list) {
        kfree(ctx->dma_addr_list);
        ctx->dma_addr_list = NULL;
    }

    ctx->dma_mapped_count = 0;
    ctx->dma_device       = NULL;  // Device reference is managed by the caller
    if (ctx->sg_head) {
        sg_free_table(ctx->sg_head);
        ctx->sg_head = NULL;
    }
    /* Note: page_table is not freed here; it is managed by PutPages */

    NDR_PEER_MEM_INFO("DmaUnmap success for VA %pK\n", (void *)ctx->va);

out_unlock_mutex:
    mutex_unlock(&ctx->context_mutex);
out_unlock_sem:
    up_read(&svm_context_sem);
    return ret;
}

/**
 * @brief Release physical pages of an NPU memory region previously acquired for RDMA access.
 *
 * This function implements the put_pages() callback for the MLNX_OFED peer memory client interface.
 * It safely releases the physical page table resources associated with an NPU memory region that
 * was previously registered via ndr_mem_acquire() and mapped via ndr_mem_get_pages(). The function
 * delegates actual page release to the HAL layer (hal_put_pages_func()), which invalidates DMA
 * mappings and returns physical pages to the NPU MMU. Critical thread-safety is ensured through:
 *   - Read semaphore (svm_context_sem) for context existence validation
 *   - Per-context mutex (context_mutex) for serialized HAL operations
 * This prevents race conditions during concurrent RDMA operations on the same memory region.
 *
 * Resource lifecycle note: This function releases page mappings but does NOT free the SVM context
 * itself. The context must remain valid until ndr_mem_release() is called to match the initial
 * ndr_mem_acquire() registration. Failure to follow this acquire→get_pages→put_pages→release
 * sequence may cause resource leaks or DMA faults.
 *
 * Error handling: HAL layer errors (e.g., invalid page table) are logged but not propagated upward,
 * as the RDMA Core considers page release a best-effort operation. The context remains valid for
 * subsequent operations, and module reference count is NOT decremented here (handled in release).
 *
 * @param sg_head   Scatter-gather table pointer from RDMA Core (unused in this implementation;
 *                  page table is retrieved directly from context). Required by MLNX_OFED interface.
 * @param context   Pointer to svm_agent_context returned by ndr_mem_acquire(). Must be non-NULL
 *                  and previously initialized. Ownership remains with caller until ndr_mem_release().
 */
void ndr_mem_put_pages(struct sg_table *sg_head, void *context)
{
    int ret                                     = 0;
    struct svm_agent_context *svm_agent_context = (struct svm_agent_context *)context;

    down_read(&svm_context_sem);
    if (!svm_agent_context) {
        NDR_PEER_MEM_ERR("svm_agent_context is NULL\n");
        up_read(&svm_context_sem);
        return;
    }

    mutex_lock(&svm_agent_context->context_mutex);
    ret = hal_put_pages_func(svm_agent_context->page_table);
    if (ret != 0) {
        NDR_PEER_MEM_INFO("put pages failed.\n");
    }
    mutex_unlock(&svm_agent_context->context_mutex);
    up_read(&svm_context_sem);
    NDR_PEER_MEM_INFO("PutPages completed with status: %s\n", ret == 0 ? "success" : "failed");
}

static void ndr_mem_release_dma_mapping(struct svm_agent_context *ctx)
{
    int i;
    u64 page_size = 0;

    if (!ctx || !ctx->dma_addr_list || ctx->dma_mapped_count == 0) {
        return;
    }

    if (ctx->page_table) {
        page_size = ctx->page_table->page_size;
    } else {
        page_size = NPU_PAGE_SIZE;
    }

    if (ctx->dma_device && page_size > 0) {
        for (i = 0; i < ctx->dma_mapped_count; i++) {
            if (ctx->dma_addr_list[i]) {
                dma_unmap_resource(ctx->dma_device,
                                   ctx->dma_addr_list[i],
                                   page_size,
                                   DMA_BIDIRECTIONAL,
                                   0);
                ctx->dma_addr_list[i] = 0;
            }
        }
        NDR_PEER_MEM_INFO("Unmapped %zu residual DMA pages during release\n",
                          ctx->dma_mapped_count);
    } else {
        NDR_PEER_MEM_INFO("Cannot unmap DMA: device=%pK, page_size=%llu\n",
                          ctx->dma_device, page_size);
    }
}
/**
 * @brief Release NPU peer memory context and associated resources.
 *
 * Implements the release() callback for MLNX_OFED peer memory client interface.
 * Finalizes the peer memory lifecycle by:
 *   - Invalidating context (inited_flag = 0) under write lock for safe teardown
 *   - Freeing scatter-gather table if allocated
 *   - Releasing kernel memory (kfree) and decrementing module reference count
 *
 * Thread-safe via write semaphore (svm_context_sem) to prevent races during context destruction.
 * Must be called exactly once after ndr_mem_acquire() completes the lifecycle:
 *   acquire → get_pages → put_pages → release
 * Failure to call release() causes module reference leakage (prevents driver unload).
 *
 * @param agent_ctx  SVM context from ndr_mem_acquire(); NULL handled defensively.
 */
void ndr_mem_release(void *agent_ctx)
{
    struct svm_agent_context *ctx = (struct svm_agent_context *)agent_ctx;
    int ret;

    if (!ctx) {
        NDR_PEER_MEM_INFO("Attempt to release NULL context\n");
        return;
    }

    mutex_lock(&ctx->context_mutex);

    if (ctx->inited_flag != 1) {
        NDR_PEER_MEM_INFO("Context %pK already released or not initialized\n", ctx);
        mutex_unlock(&ctx->context_mutex);
        return;
    }

    ctx->inited_flag = 0;

    ndr_mem_release_dma_mapping(ctx);

    kfree(ctx->dma_addr_list);
    ctx->dma_addr_list = NULL;
    ctx->dma_mapped_count = 0;
    ctx->dma_device = NULL;

    if (ctx->sg_head) {
        sg_free_table(ctx->sg_head);
        ctx->sg_head = NULL;
    }

    if (ctx->page_table) {
        ret = hal_put_pages_func(ctx->page_table);
        if (ret != 0) {
            NDR_PEER_MEM_INFO("Failed to put pages during release: %d\n", ret);
        }
        ctx->page_table = NULL;
    }

    mutex_unlock(&ctx->context_mutex);

    kfree(ctx);
    module_put(THIS_MODULE);

    NDR_PEER_MEM_INFO("Context released successfully\n");
}

static void *reg_handle                               = NULL;
static struct peer_memory_client roce_peer_mem_client = {
    .name          = DRV_ROCE_PEER_MEM_NAME,
    .version       = DRV_ROCE_PEER_MEM_VERSION,
    .acquire       = ndr_mem_acquire,
    .get_pages     = ndr_mem_get_pages,
    .dma_map       = ndr_mem_dma_map,
    .dma_unmap     = ndr_mem_dma_unmap,
    .put_pages     = ndr_mem_put_pages,
    .get_page_size = ndr_mem_get_page_size,
    .release       = ndr_mem_release,
};

typedef int (*invalidate_peer_memory)(void *reg_handle, u64 core_context);
static invalidate_peer_memory mem_invalidate_callback;

static int ndr_mem_kallsym_lookup(void)
{
    struct kprobe kp;
    memset(&kp, 0, sizeof(struct kprobe));
    kp.symbol_name = "kallsyms_lookup_name";
    if (register_kprobe(&kp) < 0) {
        return -ENOENT;
    }
    g_kallsymsLookupName = (uint64_t(*)(const char *))kp.addr;
    unregister_kprobe(&kp);

    /* HAL function */
    hal_get_pages_func = (typeof(hal_get_pages_t))g_kallsymsLookupName("hal_kernel_p2p_get_pages");
    if (!hal_get_pages_func) {
        NDR_PEER_MEM_ERR("hal_get_pages_t is not inited.\n");
        return -ENOENT;
    }

    hal_put_pages_func = (typeof(hal_put_pages_t))g_kallsymsLookupName("hal_kernel_p2p_put_pages");
    if (!hal_put_pages_func) {
        NDR_PEER_MEM_ERR("hal_put_pages_t is not inited.\n");
        return -ENOENT;
    }
    return 0;
}

static int __init ndr_mem_agent_init(void)
{
    int rc;

    NDR_PEER_MEM_INFO("Peer-Mem_RocE Init .\n");

    rc = ndr_mem_kallsym_lookup();
    if (rc) {
        NDR_PEER_MEM_ERR("syms look up failed");
        return -EINVAL;
    }

    reg_handle = ib_register_peer_memory_client(&roce_peer_mem_client, &mem_invalidate_callback);
    if (reg_handle == NULL) {
        NDR_PEER_MEM_ERR("ib register failed");
        return -ENODEV;
    }

    NDR_PEER_MEM_INFO("Peer-Mem_RocE Init Success.\n");
    return 0;
}

static void __exit ndr_mem_agent_de_init(void)
{
    if (reg_handle) {
        ib_unregister_peer_memory_client(reg_handle);
        reg_handle = NULL;
    }
    NDR_PEER_MEM_INFO("Exit Peer Mem!\n");
}

MODULE_LICENSE("GPL");
module_init(ndr_mem_agent_init);
module_exit(ndr_mem_agent_de_init);