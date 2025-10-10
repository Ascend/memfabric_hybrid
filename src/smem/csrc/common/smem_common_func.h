/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H
#define MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H

#include <climits>
#include <string>
#include "smem_types.h"
#include "smem_logger.h"

namespace ock {
namespace smem {

class CommonFunc {
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

    template <class T>
    static std::string SmemArray2String(const T *arr, uint32_t len);
};

inline bool CommonFunc::Realpath(std::string &path)
{
    if (path.empty() || path.size() > PATH_MAX) {
        SM_LOG_ERROR("Failed to get realpath of [" << path << "] as path is invalid");
        return false;
    }

    /* It will allocate memory to store path */
    char *realPath = realpath(path.c_str(), nullptr);
    if (realPath == nullptr) {
        SM_LOG_ERROR("Failed to get realpath of [" << path << "] as error " << errno);
        return false;
    }

    path = realPath;
    free(realPath);
    realPath = nullptr;
    return true;
}

inline Result CommonFunc::LibraryRealPath(const std::string &libDirPath, const std::string &libName,
                                          std::string &realPath)
{
    std::string tmpFullPath = libDirPath;
    if (!Realpath(tmpFullPath)) {
        return SM_INVALID_PARAM;
    }

    if (tmpFullPath.back() != '/') {
        tmpFullPath.push_back('/');
    }

    tmpFullPath.append(libName);
    if (tmpFullPath.size() >  PATH_MAX) {
        SM_LOG_ERROR("The path:" << tmpFullPath << " length is " << tmpFullPath.size() << " > " << PATH_MAX);
        return SM_INVALID_PARAM;
    }
    auto ret = ::access(tmpFullPath.c_str(), F_OK);
    if (ret != 0) {
        SM_LOG_ERROR(tmpFullPath << " cannot be accessed, ret: " << ret);
        return SM_ERROR;
    }

    realPath = tmpFullPath;
    return SM_OK;
}

template <class T>
inline std::string CommonFunc::SmemArray2String(const T *arr, uint32_t len)
{
    std::ostringstream oss;
    oss << "[";
    if (arr == nullptr) {
        oss << "]";
        return oss.str();
    }
    for (uint32_t i = 0; i < len; i++) {
        oss << arr[i] << (i + 1 == len ? "]" : ",");
    }
    return oss.str();
}
}  // namespace smem
}  // namespace ock
#endif  // MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H