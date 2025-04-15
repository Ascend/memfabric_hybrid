/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_H__
#define __MEMFABRIC_SMEM_H__

#include "stdint.h"

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
 * @brief Set external log function
 *
 * @param fun              [in] external log function
 * @return 0 if successful
 */
int32_t smem_set_extern_logger(void (*fun)(int, const char*));

/**
 * @brief set log print level
 *
 * @param level            [in] log level, 0:debug 1:info 2:warn 3:error
 * @return 0 if successful,
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
const char* smem_get_error_msg(int32_t errCode);

#ifdef __cplusplus
}
#endif

#endif  // __MEMFABRIC_SMEM_H__
