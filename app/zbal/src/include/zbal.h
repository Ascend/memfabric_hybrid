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
#ifndef ZBAL_H_
#define ZBAL_H_

#include "zbal_bootstrap.h"
#include "zbal_mem_allocator.h"
#include "zbal_operations.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get version of zbal
 *
 * @return string of version
 */
const char *zbal_version();

/**
 * @brief Set external log function
 *
 * @param func             [in] logger function
 *
 * @return 0 if successful
 */
int32_t zbal_set_logger(void (*func)(int, const char *));

/**
 * @brief Set logger level
 *
 * @param level            [in] level, 0:debug 1:info 2:warn 3:error
 *
 * @return 0 if successful
 */
int32_t zbal_set_logger_level(int level);

/**
 * @brief Get last error message if have
 *
 * @return error message, empty string if no error
 */
const char *zbal_get_last_error_msg();

/**
 * @brief Get and clear last error message
 *
 * @return error message, empty string if no error
 */
const char *zbal_get_and_clear_last_error_msg();

#ifdef __cplusplus
}
#endif

#endif // ZBAL_H_
