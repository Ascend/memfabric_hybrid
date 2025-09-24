/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H
#define MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H

#include <climits>
#include <string>
#include <sys/stat.h>
#include "mmc_types.h"
#include "mmc_logger.h"

namespace ock {
namespace mmc {

#define DL_LOAD_SYM(TARGET_FUNC_VAR, TARGET_FUNC_TYPE, FILE_HANDLE, SYMBOL_NAME)           \
    do {                                                                                   \
        TARGET_FUNC_VAR = (TARGET_FUNC_TYPE)dlsym(FILE_HANDLE, SYMBOL_NAME);               \
        if (TARGET_FUNC_VAR == nullptr) {                                                  \
            MMC_LOG_ERROR("Failed to call dlsym to load SYMBOL_NAME, error" << dlerror()); \
            dlclose(FILE_HANDLE);                                                          \
            return MMC_ERROR;                                                              \
        }                                                                                  \
    } while (0)

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
     * @return 0 if successful
     */
    static Result LibraryRealPath(const std::string &libDirPath, const std::string &libName, std::string &realPath);
};

inline bool Func::Realpath(std::string &path)
{
    if (path.empty() || path.size() > PATH_MAX) {
        MMC_LOG_ERROR("Failed to get realpath of [" << path << "] as path is invalid");
        return false;
    }

    /* It will allocate memory to store path */
    char *realPath = realpath(path.c_str(), nullptr);
    if (realPath == nullptr) {
        MMC_LOG_ERROR("Failed to get realpath of [" << path << "] as error " << errno);
        return false;
    }

    path = realPath;
    free(realPath);
    realPath = nullptr;
    return true;
}

inline Result Func::LibraryRealPath(const std::string &libDirPath, const std::string &libName,
                                    std::string &realPath)
{
    std::string tmpFullPath = libDirPath;
    if (!Realpath(tmpFullPath)) {
        return MMC_INVALID_PARAM;
    }

    if (tmpFullPath.back() != '/') {
        tmpFullPath.push_back('/');
    }

    tmpFullPath.append(libName);
    auto ret = ::access(tmpFullPath.c_str(), F_OK);
    if (ret != 0) {
        MMC_LOG_ERROR(tmpFullPath << " cannot be accessed, ret: " << ret);
        return MMC_ERROR;
    }

    realPath = tmpFullPath;
    return MMC_OK;
}

/**
 * 校验路径存在且不是软链接
 * @param path 要校验的路径
 * @return true: 路径存在且不是软链接, false: 路径不存在或是软链接
 */
inline int ValidatePathNotSymlink(const char* path)
{
    struct stat path_stat{};

    // 检查路径是否存在
    if (access(path, F_OK) != 0) {
        MMC_LOG_ERROR("path " << path << " does not exist. ");
        return MMC_ERROR;
    }

    // 使用lstat检查是否为软链接
    if (lstat(path, &path_stat) != 0) {
        MMC_LOG_ERROR("lstat failed for path " << path << ", error: " << errno);
        return MMC_ERROR;
    }

    // 检查是否为软链接
    if (S_ISLNK(path_stat.st_mode)) {
        MMC_LOG_ERROR("path " << path << " is a symlink. ");
        return MMC_ERROR;
    }

    return MMC_OK;
}

}  // namespace mmc
}  // namespace ock
#endif  // MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H
