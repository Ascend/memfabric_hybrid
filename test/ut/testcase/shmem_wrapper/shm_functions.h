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
#ifndef SHMEM_SHM_DEFINE_H
#define SHMEM_SHM_DEFINE_H

#include <string>
#include <iostream>
#include <unistd.h>
#include <limits.h>
#include "stdint.h"
#include "dlfcn.h"

namespace shm {

#define LOG_ERROR(ARGS) std::cout << "[SHM][ERROR] " << ARGS << std::endl
#define LOG_INFO(ARGS) std::cout << "[SHM][INFO] " << ARGS << std::endl

class Func {
public:
    /**
     * @brief Get real path
     *
     * @param path         [in/out] input path, converted realpath
     * @return true if successful
     */
    static bool Realpath(std::string &path);

    /**
     * @brief Get real path of a library and check if exists
     *
     * @param libDirPath   [in] dir path of the library
     * @param libName      [in] library name
     * @param realPath     [out] realpath of the library
     * @return true if successful
     */
    static bool LibraryRealPath(const std::string &libDirPath, const std::string &libName, std::string &realPath);
};

inline bool Func::Realpath(std::string &path)
{
    if (path.empty() || path.size() >= PATH_MAX) {
        LOG_ERROR("Failed to get realpath of [" << path << "] as path is invalid");
        return false;
    }

    /* It will allocate memory to store path */
    char *realPath = realpath(path.c_str(), nullptr);
    if (realPath == nullptr) {
        LOG_ERROR("Failed to get realpath of [" << path << "] as error " << errno);
        return false;
    }

    path = realPath;
    free(realPath);
    realPath = nullptr;
    return true;
}

inline bool Func::LibraryRealPath(const std::string &libDirPath, const std::string &libName,
                                  std::string &realPath)
{
    std::string tmpFullPath = libDirPath;
    if (!Realpath(tmpFullPath)) {
        return false;
    }

    if (tmpFullPath.back() != '/') {
        tmpFullPath.push_back('/');
    }

    tmpFullPath.append(libName);
    auto ret = ::access(tmpFullPath.c_str(), F_OK);
    if (ret != 0) {
        LOG_ERROR(tmpFullPath << " cannot be accessed, ret: " << ret);
        return false;
    }

    realPath = tmpFullPath;
    return true;
}

#define DL_LOAD_SYM(TARGET_FUNC_VAR, TARGET_FUNC_TYPE, FILE_HANDLE, SYMBOL_NAME)       \
    do {                                                                               \
        (TARGET_FUNC_VAR) = (TARGET_FUNC_TYPE)dlsym(FILE_HANDLE, SYMBOL_NAME);           \
        if ((TARGET_FUNC_VAR) == nullptr) {                                              \
            LOG_ERROR("Failed to call dlsym to load SYMBOL_NAME, error" << dlerror()); \
            dlclose(FILE_HANDLE);                                                      \
            return false;                                                              \
        }                                                                              \
    } while (0)
}  // namespace shm

#endif  // SHMEM_SHM_DEFINE_H