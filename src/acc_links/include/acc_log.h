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
#ifndef ACC_LINKS_ACC_LOG_H
#define ACC_LINKS_ACC_LOG_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

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
 * @param func             [in] external function
 * @return 0 if successfully
 */
int32_t AccSetExternalLog(void (*func)(int level, const char* msg));

/**
 * @brief Set log level
 *
 * @param level            [in] level, can be 0, 1, 2, 3
 * @return 0 if successfully
 */
int32_t AccSetLogLevel(int level);

#ifdef __cplusplus
}
#endif

#endif // ACC_LINKS_ACC_LOG_H
