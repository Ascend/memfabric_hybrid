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
#ifndef ZBAL_MEM_ALLOCATOR_H_
#define ZBAL_MEM_ALLOCATOR_H_

#include "zbal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the allocator with options, this is allocator is secondary memory allocator,
 * the original memory is already allocated from Device, for example from mem fabric on Ascend.
 * The allocator can be plugged into torch, any address allocated from this allocator can be accessed by
 * other device directly, then we don't need to have COMM buffer to store the data temporarily.
 * this action must be called before pluggable allocator automatic use zbal_pluggable_init.
 *
 * @param options          [in] the options of allocator
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t zbal_sma_init(zbal_allocator_options_t *options, int32_t flags);

/**
 * @brief Un-initialize allocator
 *
 * @param flags            [in] optional flags
 */
void zbal_sma_uninit(int32_t flags);

/**
 * @brief Initialize API of official torch pluggable memory allocator
 *
 * @param device_count     [in] number of devices
 * @return memory ptr is successful, nullptr is failed
 */
void zbal_pluggable_init(int32_t device_count);

/**
 * @brief Allocate memory API of official torch pluggable memory allocator
 *
 * @param size             [in] size of memory to be allocated
 * @param device           [in] device id
 * @param stream           [in] current stream
 * @return memory ptr is successful, nullptr is failed
 */
void *zbal_pluggable_malloc(size_t size, int32_t device, aclrtStream stream);

/**
 * @brief Free memory API of official torch pluggable memory allocator
 *
 * @param ptr              [in] pointer allocated by zbal_torch_malloc
 * @param size             [in] size of memory
 * @param device           [in] device id
 * @param stream           [in] stream
 */
void zbal_pluggable_free(void *ptr, size_t size, int32_t device, aclrtStream stream);

/**
 * @brief Empty cache API of official torch pluggable memory allocator
 *
 * @param check_error      [in] whether check on error allocate
 */
void zbal_pluggable_empty_cache(bool check_error);

/**
 * @brief Simulate sma alloc for testing with out npu purpose
 *
 * @param addr      [in] mock mem heap addr(will not use in simulate)
 * @param size      [in] mock mem heap size
 */
void zbal_simulate_init(int64_t addr, int64_t size);

#ifdef __cplusplus
}
#endif

#endif // ZBAL_MEM_ALLOCATOR_H_
