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
#ifndef SRC_SMEM_INCLUDE_HOST_SMEM_DEF_H_
#define SRC_SMEM_INCLUDE_HOST_SMEM_DEF_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dedicated return-code range for external backend C API failures. */
#define SMEM_STORE_BACKEND_CODE_OK       (0)
#define SMEM_STORE_BACKEND_CODE_BASE     (-3000)
#define SMEM_STORE_BACKEND_CODE_INTERNAL (SMEM_STORE_BACKEND_CODE_BASE)
#define SMEM_STORE_BACKEND_CODE_INVAL    (SMEM_STORE_BACKEND_CODE_BASE - 1)
#define SMEM_STORE_BACKEND_CODE_BUFEX    (SMEM_STORE_BACKEND_CODE_BASE - 2)
#define SMEM_STORE_BACKEND_CODE_PERM     (SMEM_STORE_BACKEND_CODE_BASE - 3)
#define SMEM_STORE_BACKEND_CODE_NORES    (SMEM_STORE_BACKEND_CODE_BASE - 4)
#define SMEM_STORE_BACKEND_CODE_NOENT    (SMEM_STORE_BACKEND_CODE_BASE - 5)
#define SMEM_STORE_BACKEND_CODE_LOCKED   (SMEM_STORE_BACKEND_CODE_BASE - 6)
#define SMEM_STORE_BACKEND_CODE_UNLOCKED (SMEM_STORE_BACKEND_CODE_BASE - 7)

typedef struct smem_store_prefix_get_ctx {
    const char *prefix; // common prefix
    const char *marker; // query keys greater than the marker and returns these keys in lexicographical order.
    void *context;
    /*
     * This callback is called each time a key that meets the condition is found. If this callback returns true,
     * the search continues; otherwise, no more keys will be returned.
     */
    bool (*fill)(const char *key, const void *value, uint64_t size, void *context);
} smem_store_prefix_get_ctx_t;

typedef struct {
    /*
     * Return whether the registered backend provides distributed coordination.
     */
    bool (*distributed)(uint32_t flags);

    /*
     * Create a backend handle for the specified backend name and key prefix.
     * The prefix applies to all later key and lock operations handled by the backend.
     */
    int32_t (*create)(const char *name, const char *prefix, uint32_t flags, void **handle);

    /*
     * Destroy the backend handle created by create().
     */
    void (*destroy)(void *handle);

    /*
     * Put a key-value pair and overwrite the existing value if the key already exists.
     */
    int32_t (*put)(void *handle, const char *key, const void *value, uint64_t size, uint32_t flags);

    /*
     * Get a key-value pair. Capacity is the input buffer size and size returns the actual value length.
     */
    int32_t (*get)(void *handle, const char *key, void *value, uint64_t capacity, uint32_t flags, uint64_t *size);

    /*
     * Remove a key-value pair.
     */
    int32_t (*remove)(void *handle, const char *key, uint32_t flags);

    /*
     * Acquire a named distributed lock. This call may block.
     */
    int32_t (*lock)(void *handle, const char *name, uint32_t flags);

    /*
     * Try to acquire a named distributed lock without blocking.
     */
    int32_t (*try_lock)(void *handle, const char *name, uint32_t flags);

    /*
     * Release a named distributed lock.
     */
    int32_t (*unlock)(void *handle, const char *name, uint32_t flags);
} smem_conf_store_backend_op_t;

#ifdef __cplusplus
}
#endif

#endif // SRC_SMEM_INCLUDE_HOST_SMEM_DEF_H_
