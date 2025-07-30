/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_H__
#define __MEMFABRIC_SMEM_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the smem running environment
 *
 * @param flags            [in] optional flags, reserved
 * @return 0 if successful,
 */
int32_t smem_init(uint32_t flags);

/**
 * @brief Create configure store server for SMEM used.
 *
 * @param storeURL         [in] configure store url for control, e.g. tcp:://ip:port
 * @return 0 if successful
 */
int32_t smem_create_config_store(const char *storeUrl);

/**
 * @brief Set external log function, user can set customized logger function,
 * in the customized logger function, user can use unified logger utility,
 * then the log message can be written into the same log file as caller's,
 * if it is not set, acc_links log message will be printed to stdout.
 *
 * level description:
 * 0 DEBUG,
 * 1 INFO,
 * 2 WARN,
 * 3 ERROR
 *
 * @param func             [in] external logger function
 * @return 0 if successful
 */
int32_t smem_set_extern_logger(void (*func)(int level, const char *msg));

/**
 * @brief set log print level
 *
 * @param level            [in] log level, 0:debug 1:info 2:warn 3:error
 * @return 0 if successful
 */
int32_t smem_set_log_level(int level);

/**
 * @brief Un-Initialize the smem running environment
 */
void smem_uninit();

/**
 * @brief Get the last error message
 *
 * @return last error message
 */
const char *smem_get_last_err_msg();

/**
 * @brief Get the last error message and clear
 *
 * @return last error message
 */
const char *smem_get_and_clear_last_err_msg();

#ifdef __cplusplus
}
#endif

#endif  // __MEMFABRIC_SMEM_H__
