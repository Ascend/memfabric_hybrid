/**
 * @file data_utils.h
 *
 * Copyright (C) 2024. Huawei Technologies Co., Ltd. All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef DATA_UTILS_H
#define DATA_UTILS_H
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "acl/acl.h"

typedef enum {
    DT_UNDEFINED = -1,
    FLOAT = 0,
    HALF = 1,
    INT8_T = 2,
    INT32_T = 3,
    UINT8_T = 4,
    INT16_T = 6,
    UINT16_T = 7,
    UINT32_T = 8,
    INT64_T = 9,
    UINT64_T = 10,
    DOUBLE = 11,
    BOOL = 12,
    STRING = 13,
    COMPLEX64 = 16,
    COMPLEX128 = 17,
    BF16 = 27
} printDataType;

#define INFO_LOG(fmt, args...) fprintf(stdout, "[INFO]  " fmt "\n", ##args)
#define WARN_LOG(fmt, args...) fprintf(stdout, "[WARN]  " fmt "\n", ##args)
#define ERROR_LOG(fmt, args...) fprintf(stdout, "[ERROR]  " fmt "\n", ##args)
#define CHECK_ACL(x)                                                                        \
    do {                                                                                    \
        aclError __ret = x;                                                                 \
        if (__ret != ACL_ERROR_NONE) {                                                      \
            std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
        }                                                                                   \
    } while (0);

#define CHECK_ACL_RET(x, msg)                                              \
    do {                                                                   \
        aclError __ret = x;                                                \
        if (__ret != ACL_ERROR_NONE) {                                     \
            std::cerr << msg << ":" << " aclError:" << __ret << std::endl; \
            return __ret;                                                  \
        }                                                                  \
    } while (0);

#endif // DATA_UTILS_H
