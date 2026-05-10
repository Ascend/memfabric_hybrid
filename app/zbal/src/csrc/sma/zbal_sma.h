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
#ifndef ZBAL_SMA_H
#define ZBAL_SMA_H

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "zbal_sma_common.h"
#include "zbal_sma_device.h"
#include "zbal_sma_device_pool.h"


using DeviceStats = c10_npu::NPUCachingAllocator::DeviceStats;


namespace zbal {
namespace sma {

class SecondaryMemoryAllocator : public ZReferable {
public:
    std::vector<std::unique_ptr<device::DeviceSMACachingAllocator>> device_allocator_;

    SecondaryMemoryAllocator();
    ~SecondaryMemoryAllocator() override = default;;

    static ZRef<SecondaryMemoryAllocator> GetInstance()
    {
        static ZRef<SecondaryMemoryAllocator> instance = new SecondaryMemoryAllocator();
        return instance;
    }

    /**
     * @brief Initialize the allocator
     *
     * @param options           [in] options for the allocator
     * @param device_cnt        [in] device count
     * @return 0 is successful
     */
    ZResult Initialize(zbal_allocator_options_t *options, int32_t device_count) noexcept;

    /**
     * @brief Un-initialize the allocator
     *
     * @param flags        [in] extra flags
     */
    void UnInitialize(int32_t flags) noexcept;

    /**
     * @brief Allocate memory
     *
     * @param devPtr         [in] ptr of memory pointer wanted to be allocated
     * @param device         [in] device id
     * @param size           [in] allocate size
     * @param stream         [in] aclrtStream
     * @return 0 if successful
     */
    ZResult Allocate(void **devPtr, int device, size_t size, aclrtStream stream) noexcept;

    /**
     * @brief Free memory
     *
     * @param ptr          [in] memory pointer allocated by <i>Allocate</i>
     * @return 0 if successful
     */
    ZResult Free(void *ptr) noexcept;

    /**
     * @brief free cached memory
     *
     * @param check_error  [in] assert when aclrt func error
     * @return 0 if successful
     */
    ZResult EmptyCache(bool check_error);

    /**
     * @brief record a stream on its depend mem block
     *
     * @param ptr         [in] mem ptr of stream rely on
     * @param stream      [in] relied stream
     * @return 0 if successful
     */
    ZResult RecordStream(void *ptr, c10_npu::NPUStream stream);

    /**
     * @brief release a stream on its depend mem block
     *
     * @param ptr         [in] mem ptr of stream rely on
     * @param stream      [in] stream to be released
     * @return 0 if successful
     */
    ZResult EraseStream(void *ptr, c10_npu::NPUStream stream);

    /**
     * @brief begin a NPU graph capture on target mem pool
     *
     * @param device      [in] mem ptr of stream rely on
     * @param mempool_id  [in] the id of mem-pool to be allocated by capture stream
     * @param filter      [in] filter on between capture stream and other stream
     * @return 0 if successful
     */
    ZResult BeginAllocateToPool(int device, c10_npu::MempoolId_t mempool_id, std::function<bool(aclrtStream)> filter);

    /**
     * @brief end a NPU graph capture on target mem pool
     *
     * @param device      [in] mem ptr of stream rely on
     * @param mempool_id  [in] the id of mem-pool to be allocated by capture stream
     * @return 0 if successful
     */
    ZResult EndAllocateToPool(int device, c10_npu::MempoolId_t mempool_id);

    /**
     * @brief release Private Pool(NPU graph created) in target mem pool
     *
     * @param device      [in] mem ptr of stream rely on
     * @param mempool_id  [in] the id of mem-pool to be allocated by capture stream
     * @return 0 if successful
     */
    ZResult ReleasePool(int device, c10_npu::MempoolId_t mempool_id);

    /**
     * @brief get device stats(c10_npu allocator device stats)
     *
     * @param device        [in] checked device id, using current device if < 0
     * @return c10_npu::NPUCachingAllocator::DeviceStats
     */
    DeviceStats GetDeviceStats(int device = -1);

    /**
     * @brief reset accumulate device stats
     *
     * @param device        [in] checked device id, using current device if < 0
     * @return 0 if successful
     */
    ZResult ResetAccumulatedStats(int device = -1);

    /**
     * @brief reset peak device stats
     *
     * @param device        [in] checked device id, using current device if < 0
     * @return 0 if successful
     */
    ZResult ResetPeakStats(int device = -1);

    /**
     * @brief get memory heap state in target device
     *
     * @param in_used_size  [inout] heap currently in used size(including caching blocks)
     * @param total_size    [inout] heap currently in used size(including caching blocks)
     * @param device        [in] checked device id, using current device if < 0
     * @return 0 if successful
     */
    ZResult GetHeapState(size_t &in_used_size, size_t &total_size, int device = -1);

    /**
     * @brief get all device snapshot info
     *
     * @param device_info   [inout] result device_info)
     * @param device        [in] checked device id, using current device if < 0
     * @return 0 if successful
     */
    ZResult SnapShot(device::SnapshotDeviceInfo &device_info, int device = -1);

private:
    std::mutex mutex_;

    // allocated blocks by device pointer
    ska::flat_hash_map<void *, device::DeviceBlock *> allocated_blocks_;

    void add_allocated_block(device::DeviceBlock *block);

    device::DeviceBlock *get_allocated_block(void *ptr, bool remove = false);

    bool initialized();

    void cleanEvent();

    // fix those if pluggable also need?
    bool checkBlockIsSafe(const c10::DataPtr &ptr);
    void markAllBlockUnsafe(int device);
    void updateBlockToSafe(const c10::DataPtr &ptr);

    void assertValidDevice(int device);
};

}  // namespace sma
}  // namespace zbal

extern "C" {
ZBAL_API void *sma_malloc(size_t size, int device, aclrtStream stream);

ZBAL_API void sma_init(int device_count);

ZBAL_API void sma_empty_cache(bool check_error);

ZBAL_API void sma_free(void *ptr, size_t size, int device, aclrtStream stream);

ZBAL_API void sma_record_stream(void *ptr, c10_npu::NPUStream stream);

ZBAL_API void sma_erase_stream(void *ptr, c10_npu::NPUStream stream);

ZBAL_API void sma_begin_allocate_to_pool(int device, c10_npu::MempoolId_t mempool_id,
                                         std::function<bool(aclrtStream)> filter);

ZBAL_API void sma_end_allocate_to_pool(int device, c10_npu::MempoolId_t mempool_id);

ZBAL_API void sma_release_pool(int device, c10_npu::MempoolId_t mempool_id);

ZBAL_API DeviceStats sma_get_device_stats(int device);

ZBAL_API void *sma_get_base_addr(int device = -1);

ZBAL_API void sma_init_heap(void *base_ptr, uint64_t local_mem_size);

ZBAL_API void sma_get_heap_stats(size_t &in_used_size, size_t &total_size, int device = -1);
}

void sma_record_memory_history(std::optional<std::string> enabled, int64_t max_entries);

pybind11::dict sma_dump_snapshot();

#endif  // ZBAL_SMA_H

