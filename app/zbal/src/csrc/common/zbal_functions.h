/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef ZBAL_FUNCTIONS_H
#define ZBAL_FUNCTIONS_H

#include <set>
#include <string>
#include <climits>
#include <unistd.h>
#include <sys/param.h>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>
#include <ctime>

#include "zbal_defines.h"

namespace zbal {
class Func {
public:
    /**
     * @brief Get real path with dir and file path
     *
     * @param libDirPath   [in] library path
     * @param libName      [in] library name
     * @param realPath     [out] realpath
     * @return true if get real path successfully
     */
    static bool LibraryRealPath(const std::string &libDirPath, const std::string &libName, std::string &realPath);

    /**
     * @brief Get real path for secure consideration
     *
     * @param path         [in/out] source path and output path
     *
     * @return true if get real path successfully
     */
    static bool Realpath(std::string &path);

    /**
     * @brief Get max length of file path
     */
    static constexpr size_t GetSafePathMax();

    /**
     * @brief Get environment variable value
     *
     * @param name                [in] environment variable name
     * @param defaultValue        [in] default value if environment variable is not set or invalid
     * @return the value of the environment variable if set and valid, otherwise the default value
     */
    template<typename T>
    static T GetEnv(const char *name, T defaultValue = T{});

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
     * @brief Find whether the path is a directory or not
     *
     * @param path         [in] input path
     * @return true if it is a directory
     */
    static bool IsDir(const std::string &path);

    /**
     * @brief Get current date time format string
     *
     * @return format date string
     */
    static std::string GetCurrentDateTime();

    /**
     * @brief Get env and split by comma
     *
     * @return the set of the env split by comma
     */
    static std::set<std::string> GetEnvSplitByComma(const char *name);
};

ALWAYS_INLINE bool Func::LibraryRealPath(const std::string &libDirPath, const std::string &libName,
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
        return false;
    }

    realPath = tmpFullPath;
    return true;
}

ALWAYS_INLINE bool Func::Realpath(std::string &path)
{
    if (path.empty() || path.size() > PATH_MAX_LIMIT) {
        return false;
    }

    /* It will allocate memory to store path */
    char *tmp = new (std::nothrow) char[GetSafePathMax() + 1];
    if (tmp == nullptr) {
        return false;
    }

    char *realPath = realpath(path.c_str(), tmp);
    if (realPath == nullptr) {
        delete[] tmp;
        return false;
    }

    path = realPath;
    realPath = nullptr;
    delete[] tmp;
    return true;
}

ALWAYS_INLINE constexpr size_t Func::GetSafePathMax()
{
#ifdef PATH_MAX
    return (PATH_MAX < PATH_MAX_LIMIT) ? PATH_MAX : PATH_MAX_LIMIT;
#else
    return PATH_MAX_LIMIT;
#endif
}

template<typename T>
ALWAYS_INLINE T Func::GetEnv(const char *name, T defaultValue)
{
    const char *envValue = std::getenv(name);
    if (envValue == nullptr) {
        return defaultValue;
    }

    std::string envStr(envValue);
    T result;
    try {
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
            result = static_cast<T>(std::stod(envStr));
        } else if constexpr (std::is_integral_v<T>) {
            result = static_cast<T>(std::stoi(envStr));
        } else if constexpr (std::is_same_v<T, std::string>) {
            result = envStr;
        } else {
            static_assert(std::is_same_v<T, float> || std::is_same_v<T, double> || std::is_integral_v<T>,
                          "Unsupported type for GetEnv");
        }
        return result;
    } catch (const std::invalid_argument &ia) {
        ZBAL_OP_LOGE("", "Invalid argument when parsing %s: %s", name, ia.what());
    } catch (const std::out_of_range &oor) {
        ZBAL_OP_LOGE("", "Out of range when parsing %s: %s", name, oor.what());
    } catch (...) {
        ZBAL_OP_LOGE("", "Unexpected error when parsing %s", name);
    }
    return defaultValue;
}

ALWAYS_INLINE bool Func::Exist(const std::string &path)
{
    return access(path.c_str(), 0) != -1;
}

ALWAYS_INLINE bool Func::Readable(const std::string &path)
{
    return access(path.c_str(), F_OK | R_OK) != -1;
}

ALWAYS_INLINE bool Func::Writable(const std::string &path)
{
    return access(path.c_str(), F_OK | W_OK) != -1;
}

ALWAYS_INLINE bool Func::ReadAndWritable(const std::string &path)
{
    return access(path.c_str(), F_OK | R_OK | W_OK) != -1;
}

ALWAYS_INLINE bool Func::MakeDir(const std::string &path, uint32_t mode)
{
    if (path.empty()) {
        return false;
    }

    if (Exist(path)) {
        return true;
    }

    return ::mkdir(path.c_str(), mode) == 0;
}

ALWAYS_INLINE bool Func::MakeDirRecursive(const std::string &path, uint32_t mode)
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

ALWAYS_INLINE bool Func::Remove(const std::string &path, bool canonicalPath)
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

ALWAYS_INLINE bool Func::RemoveDirRecursive(const std::string &path)
{
    if (path.empty() || path.size() > PATH_MAX_LIMIT) {
        return false;
    }

    std::string realPath = path;
    if (!Realpath(realPath)) {
        return false;
    }

    DIR *dir = opendir(realPath.c_str());
    if (dir == nullptr) {
        return false;
    }

    struct dirent *entry = nullptr;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        struct stat statBuf{};
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

ALWAYS_INLINE bool Func::IsDir(const std::string &path)
{
    struct stat buf;
    if (lstat(path.c_str(), &buf) != 0) {
        return false;
    }
    return S_ISDIR(buf.st_mode);
}

ALWAYS_INLINE std::string Func::GetCurrentDateTime()
{
    std::time_t now = std::time(nullptr);
    struct tm localTime;
    localtime_r(&now, &localTime);

    char buffer[128];
    std::strftime(buffer, sizeof(buffer), "%Y_%m_%d_%H_%M_%S", &localTime);
    return std::string(buffer);
}

ALWAYS_INLINE std::set<std::string> Func::GetEnvSplitByComma(const char *name)
{
    const char *envStr = std::getenv(name);
    if (envStr == nullptr) {
        return {};
    }

    auto trim = [](const std::string &str) -> std::string {
        size_t start = str.find_first_not_of(" \t\n\r\f\v");
        if (start == std::string::npos) {
            return "";
        }
        size_t end = str.find_last_not_of(" \t\n\r\f\v");
        return str.substr(start, end - start + 1);
    };

    std::set<std::string> rets;
    std::stringstream ss(envStr);
    std::string item;

    while (std::getline(ss, item, ',')) {
        std::string trimmed = trim(item);
        if (!trimmed.empty()) {
            rets.insert(trimmed);
        }
    }

    return rets;
}

} // namespace zbal

#endif // ZBAL_FUNCTIONS_H
