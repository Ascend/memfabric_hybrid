/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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