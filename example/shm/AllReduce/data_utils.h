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
#include <cstring>

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

const int OUTPUT_WIDTH = 10U;

#ifndef LOG_FILENAME_SHORT
#define LOG_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define INFO_LOG(fmt, args...) fprintf(stdout, "[INFO]  " fmt "\n", ##args)
#define WARN_LOG(fmt, args...) fprintf(stdout, "[WARN]  " fmt "\n", ##args)
#define ERROR_LOG(fmt, args...) fprintf(stdout, "[ERROR]  " fmt "\n", ##args)
#define CHECK_ACL(x)                                                                        \
    do {                                                                                    \
        aclError __ret = x;                                                                 \
        if (__ret != ACL_ERROR_NONE) {                                                      \
            std::cerr << LOG_FILENAME_SHORT << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
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

template <typename T> void DoPrintData(const T *data, size_t count, size_t elementsPerRow)
{
    ASSERT(elementsPerRow != 0);
    for (size_t i = 0; i < count; ++i) {
        std::cout << std::setw(OUTPUT_WIDTH) << data[i];
        if (i % elementsPerRow == elementsPerRow - 1) {
            std::cout << std::endl;
        }
    }
}

/**
 * @brief Read data from file
 * @param [in] filePath: file path
 * @param [out] fileSize: file size
 * @return read result
 */
bool ReadFile(const std::string &filePath, size_t &fileSize, void *buffer, size_t bufferSize);

/**
 * @brief Write data to file
 * @param [in] filePath: file path
 * @param [in] buffer: data to write to file
 * @param [in] size: size to write
 * @return write result
 */
bool WriteFile(const std::string &filePath, const void *buffer, size_t size);

void DoPrintHalfData(const aclFloat16 *data, size_t count, size_t elementsPerRow);
void PrintData(const void *data, size_t count, printDataType dataType, size_t elementsPerRow = 16);

#endif // DATA_UTILS_H
