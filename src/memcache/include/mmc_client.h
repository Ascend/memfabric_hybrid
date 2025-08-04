/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_MMC_CLIENT_H__
#define __MEMFABRIC_MMC_CLIENT_H__

#include "mmc_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize client of Distributed Memory Cache with config, which is a singleton
 *
 * @param config           [in] config of client
 * @return 0 if successful
 */
int32_t mmcc_init(mmc_client_config_t *config);

/**
 * @brief Un-initialize client
 */
void mmcc_uninit();

/**
 * @brief Put data of object with key into Distributed Memory Cache
 * This data operation supports both sync and async
 *
 * @param key              [in] key of data, less than 256
 * @param buf              [in] data to be put
 * @param options          [in] options for put operation
 * @param flags            [in] optional flags, reserved
 * @return 0 if successful
 */
int32_t mmcc_put(const char *key, mmc_buffer *buf, mmc_put_options options, uint32_t flags);

/**
 * @brief Get data of object by key from Distributed Memory Cache
 * This data operation supports both sync and async
 *
 * @param key              [in] key of data, less than 256
 * @param buf              [in] data to be gotten
 * @param flags            [in] optional flags, reserved
 * @return 0 if successful
 */
int32_t mmcc_get(const char *key, mmc_buffer *buf, uint32_t flags);

/**
 * @brief Get data info of object by key from Distributed Memory Cache
 * This data operation supports both sync and async
 *
 * @param key              [in] key of data, less than 256
 * @param info             [in] data to be gotten
 * @param flags            [in] optional flags, reserved
 * @return 0 if successful
 */
int32_t mmcc_query(const char *key, mmc_data_info *info, uint32_t flags);

/**
 * @brief query blob info with key
 *
 * @param keys             [in] keys of data, less than 256
 * @param keys_count       [in] Count of keys
 * @param info             [out] Blob info of keys
 * @param flags            [in] Flags for the operation
 * @return 0 if successfully
 */
int32_t mmcc_batch_query(const char **keys, size_t keys_count, mmc_data_info *info, uint32_t flags);

/**
 * @brief Get the locations of object
 * This data operation only supports sync mode
 *
 * @param key              [in] key of data, less than 256
 * @param flags            [in] optional flags, reserved
 * @return locations if exists
 */
mmc_location_t mmcc_get_location(const char *key, uint32_t flags);

/**
 * @brief Remove the object with key from Distributed Memory Cache
 * This data operation supports both sync and async
 *
 * @param key              [in] key of data, less than 256
 * @param flags            [in] optional flags, reserved
 * @return  0 if successful
 */
int32_t mmcc_remove(const char *key, uint32_t flags);

/**
 * @brief Remove multiple keys from the BM
 *
 * @param keys             [in] List of keys to be removed from the BM
 * @param keys_count       [in] Count of keys
 * @param remove_results   [out] Results of each removal operation
 * @param flags            [in] Flags for the operation
 * @return 0 if successfully, positive value if error happens
 */
int32_t mmcc_batch_remove(const char **keys, uint32_t keys_count, int32_t *remove_results, uint32_t flags);

/**
 * @brief Wait for async operation to object
 *
 * @param waitHandle       [in] handle created by data operation, i.e. mobsc_put, mobsc_get, mobsc_remove
 * @param timeoutSec       [in] timeout of wait, in seconds
 * @return 0 if successfully, 1 if timeout, positive value if error happens
 */
int32_t mmcc_wait(int32_t waitHandle, int32_t timeoutSec);

/**
 * @brief Determine whether the key is within the BM
 *
 * @param key              [in] key of data, less than 256
 * @return 0 if successfully
 */
int32_t mmcc_exist(const char *key, uint32_t flags);

/**
 * @brief Determine whether the list of keys is within the BM
 *
 * @param keys             [in] keys of data, the length of key is less than 256
 * @param keys_count       [in] Count of keys
 * @param exist_results    [out] existence status list of keys in BM
 * @return 0 if successfully
 */
int32_t mmcc_batch_exist(const char **keys, uint32_t keys_count, int32_t *exist_results, uint32_t flags);

/**
 * @brief Put multiple data objects into Distributed Memory Cache
 * This data operation supports both sync and async
 *
 * @param keys           [in] Array of keys for the data objects
 * @param keys_count     [in] Number of keys in the array
 * @param bufs           [in] Array of data buffers to be put
 * @param options        [in] Options for the batch put operation
 * @param flags          [in] Optional flags, reserved
 * @return 0 if successful
 */
int32_t mmcc_batch_put(const char **keys, uint32_t keys_count, const mmc_buffer *bufs,
                       mmc_put_options& options, uint32_t flags);

/**
 * @brief Get multiple data objects by keys from Distributed Memory Cache
 * This data operation supports both sync and async
 *
 * @param keys           [in] Array of keys for the data objects
 * @param keys_count     [in] Number of keys in the array
 * @param bufs           [out] Array of data buffers to store the retrieved data
 * @param flags          [in] Optional flags, reserved
 * @return 0 if successful
 */
int32_t mmcc_batch_get(const char **keys, uint32_t keys_count, mmc_buffer *bufs, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif  //__MEMFABRIC_MMC_CLIENT_H__
