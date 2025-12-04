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
#ifndef COMMON_DEF_H
#define COMMON_DEF_H

#include <iostream>
#include <string>
#include <unistd.h>

#define LOG_INFO(msg) std::cout << getpid() << " " << __FILE__ << ":" << __LINE__ << "[INFO]" << msg << std::endl
#define LOG_WARN(msg) std::cout << getpid() << " " << __FILE__ << ":" << __LINE__ << "[WARN]" << msg << std::endl
#define LOG_ERROR(msg) std::cout << getpid() << " " << __FILE__ << ":" << __LINE__ << "[ERROR]" << msg << std::endl

#define CHECK_RET_ERR(x, msg)   \
do {                            \
    if ((x) != 0) {             \
        LOG_ERROR(msg);         \
        return -1;              \
    }                           \
} while (0)

#define CHECK_RET_VOID(x, msg)  \
do {                            \
    if ((x) != 0) {             \
        LOG_ERROR(msg);         \
        return;                 \
    }                           \
} while (0)

const int32_t RANK_SIZE_MAX = 16;

#endif