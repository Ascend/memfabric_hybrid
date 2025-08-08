/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H
#define MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H

#include <climits>
#include <string>
#include <sys/stat.h>
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
     * @brief Find whether a file/dict is a symbolic link
     *
     * @param path         [in] input path
     * @return true if it is a symbolic link
     */
    static bool IsSymlink(const std::string &path);

    /**
     * @brief Find whether the path is a file or not
     *
     * @param path         [in] input path
     * @return true if it is a file
     */
    static bool IsFile(const std::string &path);

    /**
     * @brief Find whether the path is a directory or not
     *
     * @param path         [in] input path
     * @return true if it is a directory
     */
    static bool IsDir(const std::string &path);

    /**
     * @brief Find whether the path exceed the max size or not
     *
     * @param path         [in] input path
     * @param maxSize      [in] the max size allowed
     * @return true if the file size is less or equals to maxSize
     */
    static bool CheckFileSize(const std::string &path, uint32_t maxSize = MAX_FILE_SIZE);

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
    if (path.empty() || path.size() >= PATH_MAX) {
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

inline bool CommonFunc::IsSymlink(const std::string &path)
{
    /* remove / at tail */
    std::string cleanPath = path;
    while (!cleanPath.empty() && cleanPath.back() == '/') {
        cleanPath.pop_back();
    }

    struct stat buf;
    if (lstat(cleanPath.c_str(), &buf) != 0) {
        return false;
    }
    return S_ISLNK(buf.st_mode);
}

inline bool CommonFunc::IsFile(const std::string &path)
{
    struct stat buf;
    if (lstat(path.c_str(), &buf) != 0) {
        return false;
    }
    return S_ISREG(buf.st_mode);
}

inline bool CommonFunc::IsDir(const std::string &path)
{
    struct stat buf;
    if (lstat(path.c_str(), &buf) != 0) {
        return false;
    }
    return S_ISDIR(buf.st_mode);
}

inline bool CommonFunc::CheckFileSize(const std::string &path, uint32_t maxSize)
{
    struct stat buf;
    if (lstat(path.c_str(), &buf) != 0) {
        return false;
    }
    return buf.st_size <= static_cast<off_t>(maxSize);
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
