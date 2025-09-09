/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_FILE_UTIL_H
#define MEMFABRIC_FILE_UTIL_H

#include <cstring>
#include <dirent.h>
#include <string>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mf_out_logger.h"

#define PATH_MAX_LIMIT 4096L
namespace ock {
namespace mf {
class FileUtil {
    static constexpr uint32_t MAX_FILE_SIZE = 10 * 1024 * 1024; // 10MB
public:
    /**
       * @brief Get the lengthiest of path
       *
       * @return the lengthiest of path
     */
    static constexpr size_t GetSafePathMax();

    /**
     * @brief Check if file or dir exists
     */
    static bool Exist(const std::string &path);

    /**
     * @brief Check if the file or dir readable
     */
    static bool Readable(const std::string &path);

    /**
     * @brief Check if the file or dir writable
     */
    static bool Writable(const std::string &path);

    /**
     * @brief Check if the file or dir readable and writable
     */
    static bool ReadAndWritable(const std::string &path);

    /**
     * @brief Create dir
     */
    static bool MakeDir(const std::string &path, uint32_t mode);

    /**
     * @brief Create dir recursively if parent doesn't exist
     */
    static bool MakeDirRecursive(const std::string &path, uint32_t mode);

    /**
     * @brief Remove the dir without sub dirs
     */
    static bool Remove(const std::string &path, bool canonicalPath = true);

    /**
     * @brief Remove the dir recursively, its sub dir will be removed
     */
    static bool RemoveDirRecursive(const std::string &path);

    /**
     * @brief Get the realpath for security consideration
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
    static bool LibraryRealPath(const std::string &libDirPath, const std::string &libName, std::string &realPath);

    /**
     * @brief Get size of a file
     */
    static size_t GetFileSize(const std::string &filePath);
    
    /**
     * @brief Close file
     */
    static void CloseFile(FILE* fp);

    /**
     * @brief Check if the file or dir is symbol link
     */
    static bool IsSymlink(const std::string &filePath);

    /**
     * @brief Check if the file is empty one
     */
    static bool IsEmptyFile(const std::string &filePath);

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
};

inline bool FileUtil::Exist(const std::string &path)
{
    return access(path.c_str(), 0) != -1;
}

inline bool FileUtil::Readable(const std::string &path)
{
    return access(path.c_str(), F_OK | R_OK) != -1;
}

inline bool FileUtil::Writable(const std::string &path)
{
    return access(path.c_str(), F_OK | W_OK) != -1;
}

inline bool FileUtil::ReadAndWritable(const std::string &path)
{
    return access(path.c_str(), F_OK | R_OK | W_OK) != -1;
}

inline bool FileUtil::MakeDir(const std::string &path, uint32_t mode)
{
    if (path.empty()) {
        return false;
    }

    if (Exist(path)) {
        return true;
    }

    return ::mkdir(path.c_str(), mode) == 0;
}

inline bool FileUtil::MakeDirRecursive(const std::string &path, uint32_t mode)
{
    if (path.empty()) {
        return false;
    }

    if (Exist(path)) {
        return true;
    }

    const char* chPath = path.c_str();
    std::string mutablePath(chPath);
    char* p = strchr(&mutablePath[1], '/');
    for (; p != nullptr; (p = strchr(p + 1, '/'))) {
        *p = '\0';
        if (mkdir(chPath, mode) == -1) {
            if (errno != EEXIST) {
                *p = '/';
                return false;
            }
        }
        *p = '/';
    }

    return ::mkdir(chPath, mode) == 0;
}

inline bool FileUtil::Remove(const std::string &path, bool canonicalPath)
{
    if (path.empty() || path.size() > PATH_MAX_LIMIT) {
        return false;
    }

    std::string realPath = path;
    if (canonicalPath && !Realpath(realPath)) {
        return false;
    }

    return ::remove(realPath.c_str()) == 0;
}

inline bool FileUtil::RemoveDirRecursive(const std::string &path)
{
    if (path.empty() || path.size() > PATH_MAX_LIMIT) {
        return false;
    }

    std::string realPath = path;
    if (!Realpath(realPath)) {
        return false;
    }

    DIR* dir = opendir(realPath.c_str());
    if (dir == nullptr) {
        return false;
    }

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        struct stat statBuf {};
        std::string absPath = realPath + "/" + entry->d_name;
        if (!stat(absPath.c_str(), &statBuf) && S_ISDIR(statBuf.st_mode)) {
            RemoveDirRecursive(absPath);
        }

        ::remove(absPath.c_str());
    }

    ::closedir(dir);

    ::remove(realPath.c_str());
    return true;
}

inline bool FileUtil::Realpath(std::string &path)
{
    if (path.empty() || path.size() > PATH_MAX_LIMIT) {
        return false;
    }

    /* It will allocate memory to store path */
    char tmp[FileUtil::GetSafePathMax() + 1] = {0x00};
    char* realPath = realpath(path.c_str(), tmp);
    if (realPath == nullptr) {
        return false;
    }

    path = realPath;
    realPath = nullptr;
    return true;
}

inline bool FileUtil::LibraryRealPath(const std::string &libDirPath, const std::string &libName, std::string &realPath)
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
        return false;
    }

    realPath = tmpFullPath;
    return true;
}

inline void FileUtil::CloseFile(FILE* fp)
{
    if (fp == nullptr) {
        return;
    }
    
    auto ret = fclose(fp);
    if (ret != 0) {
        MF_OUT_LOG("util", WARN_LEVEL, "fclose failed, ret = " << ret);
    }
}

inline size_t FileUtil::GetFileSize(const std::string &path)
{
    if (!Exist(path)) {
        return 0;
    }

    std::string realFilePath;
    if (!Realpath(realFilePath)) {
        return 0;
    }

    FILE* fp = fopen(realFilePath.c_str(), "rb");
    if (fp == nullptr) {
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        CloseFile(fp);
        return 0;
    }

    size_t fileSize = static_cast<size_t>(ftell(fp));
    if (fseek(fp, 0, SEEK_END) != 0) {
        CloseFile(fp);
        return 0;
    }

    CloseFile(fp);

    return fileSize;
}

inline bool FileUtil::IsSymlink(const std::string &filePath)
{
    /* remove / at tail */
    std::string cleanPath = filePath;
    while (!cleanPath.empty() && cleanPath.back() == '/') {
        cleanPath.pop_back();
    }

    struct stat buf;
    if (lstat(cleanPath.c_str(), &buf) != 0) {
        return false;
    }
    return S_ISLNK(buf.st_mode);
}

inline bool FileUtil::IsEmptyFile(const std::string &filePath)
{
    if (!Exist(filePath)) {
        return false;
    }

    return GetFileSize(filePath) == 0;
}

inline bool FileUtil::IsFile(const std::string &path)
{
    struct stat buf;
    if (lstat(path.c_str(), &buf) != 0) {
        return false;
    }
    return S_ISREG(buf.st_mode);
}

inline bool FileUtil::IsDir(const std::string &path)
{
    struct stat buf;
    if (lstat(path.c_str(), &buf) != 0) {
        return false;
    }
    return S_ISDIR(buf.st_mode);
}

inline bool FileUtil::CheckFileSize(const std::string &path, uint32_t maxSize)
{
    if (!Exist(path)) {
        return false;
    }

    return GetFileSize(path) <= static_cast<size_t>(maxSize);
}

inline constexpr size_t FileUtil::GetSafePathMax()
{
#ifdef PATH_MAX
    return (PATH_MAX < PATH_MAX_LIMIT) ? PATH_MAX : PATH_MAX_LIMIT;
#else
    return PATH_MAX_LIMIT;
#endif
}
}  // namespace mf
}  // namespace ock

#endif