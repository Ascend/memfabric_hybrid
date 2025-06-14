/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H
#define MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H

#include <climits>
#include <string>
#include "mmc_types.h"
#include "mmc_logger.h"

namespace ock {
namespace mmc {

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
}  // namespace smem
}  // namespace ock
#endif  // MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H
