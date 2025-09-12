/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#pragma once

#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <unistd.h>

namespace ock {
namespace mf {
class FileUtil {
public:
    /*
     * @brief Check if file or dir exists
     */
    static bool Exist(const std::string &path);

    /*
     * @brief Check if the file or dir readable
     */
    static bool Readable(const std::string &path);

    /*
     * @brief Check if the file or dir writable
     */
    static bool Writable(const std::string &path);

    /*
     * @brief Check if the file or dir readable and writable
     */
    static bool ReadAndWritable(const std::string &path);

    /*
     * @brief Create dir
     */
    static bool MakeDir(const std::string &path, uint32_t mode);

    /*
     * @brief Create dir recursively if parent doesn't exist
     */
    static bool MakeDirRecursive(const std::string &path, uint32_t mode);

    /*
     * @brief Remove the dir without sub dirs
     */
    static bool Remove(const std::string &path, bool canonicalPath = true);

    /*
     * @brief Get the realpath for security consideration
     */
    static bool CanonicalPath(std::string &path);

     /*
     * @brief 检查文件大小，失败后不会关闭文件流
     */
    static bool CheckFileSize(std::ifstream &inConfFile, int maxSize);

    /*
     * @brief Check if this file is a regular file
     */
    static bool CheckFileIsREG(std::string &dir);
};

inline bool FileUtil::Exist(const std::string &path)
{
    return access(reinterpret_cast<const char *>(path.c_str()), 0) != -1;
}

inline bool FileUtil::CheckFileSize(std::ifstream &inConfFile, int maxSize)
{
    if (!inConfFile) {
        return false;
    }
    inConfFile.seekg(0, std::ios::end);
    if (inConfFile.fail()) {
        return false;
    }
    auto fileSize = inConfFile.tellg();
    inConfFile.seekg(0, std::ios::beg);
    if (inConfFile.fail()) {
        return false;
    }
    if (fileSize <= 0 || fileSize > maxSize) {
        std::cerr << "Configuration file fileSize is Invalid, fileSize is:" << fileSize << "." << std::endl;
        return false;
    }
    return true;
}

inline bool FileUtil::Readable(const std::string &path)
{
    return access(reinterpret_cast<const char *>(path.c_str()), F_OK | R_OK) != -1;
}

inline bool FileUtil::Writable(const std::string &path)
{
    return access(reinterpret_cast<const char *>(path.c_str()), F_OK | W_OK) != -1;
}

inline bool FileUtil::ReadAndWritable(const std::string &path)
{
    return access(reinterpret_cast<const char *>(path.c_str()), F_OK | R_OK | W_OK) != -1;
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

    auto chPath = const_cast<char *>(path.c_str());
    auto p = strchr(chPath + 1, '/');
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
    if (path.empty() || path.size() > 4096L) {
        return false;
    }

    std::string realPath = path;
    if (canonicalPath && !CanonicalPath(realPath)) {
        return false;
    }

    return ::remove(realPath.c_str()) == 0;
}

inline bool FileUtil::CanonicalPath(std::string &path)
{
    if (path.empty() || path.size() > PATH_MAX) {
        return false;
    }

    /* It will allocate memory to store path */
    char *realPath = realpath(path.c_str(), nullptr);
    if (realPath == nullptr) {
        return false;
    }

    path = realPath;
    free(realPath);
    return true;
}

inline bool FileUtil::CheckFileIsREG(std::string &file)
{
    struct stat st {};
    if (lstat(file.c_str(), &st) < 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
}

}
}
