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
#include <climits>
#include <regex>
#include <cstdint>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

#include "acc_includes.h"
#include "acc_file_validator.h"

namespace {
constexpr long MIN_MALLOC_SIZE = 1;
constexpr long DEFAULT_MAX_DATA_SIZE = 1024 * 1024 * 1024;
constexpr mode_t PER_PERMISSION_MASK_RWX = 0b111;
}  // namespace

namespace ock {
namespace acc {
static long g_defaultMaxDataSize = DEFAULT_MAX_DATA_SIZE;
static const mode_t FILE_MODE = 0740;

static bool CheckDataSize(long size)
{
    if ((size > g_defaultMaxDataSize) || (size < MIN_MALLOC_SIZE)) {
        std::cerr << "Input data size(" << size << ") out of range[" << MIN_MALLOC_SIZE << "," << g_defaultMaxDataSize
                  << "]." << std::endl;
        return false;
    }

    return true;
}

bool FileValidator::RegularFilePath(const std::string &filePath, const std::string &baseDir, std::string &errMsg)
{
    if (filePath.empty()) {
        errMsg = "The file path is empty.";
        return false;
    }
    if (baseDir.empty()) {
        errMsg = "The file path basedir is empty.";
        return false;
    }
    if (filePath.size() >= ock::mf::FileUtil::GetSafePathMax()) {
        errMsg = "The file path exceeds the maximum value set by PATH_MAX.";
        return false;
    }
    if (baseDir.size() >= ock::mf::FileUtil::GetSafePathMax()) {
        errMsg = "The file path basedir exceeds the maximum value set by PATH_MAX.";
        return false;
    }
    if (ock::mf::FileUtil::IsSymlink(filePath)) {
        errMsg = "The file is a link.";
        return false;
    }

    char* path = new(std::nothrow) char[ock::mf::FileUtil::GetSafePathMax() + UNO_1];
    if (path == nullptr) {
        errMsg = "Memory allocation failed.";
        return false;
    }
    bzero(path, ock::mf::FileUtil::GetSafePathMax() + UNO_1);

    char* ret = realpath(filePath.c_str(), path);
    if (ret == nullptr) {
        errMsg = "The path realpath parsing failed.";
        delete[] path;
        return false;
    }

    std::string realFilePath(path, path + strlen(path));

    std::string dir = baseDir.back() == '/' ? baseDir : baseDir + "/";
    if (realFilePath.rfind(dir, 0) != 0) {
        errMsg = "The file is invalid, it's not in baseDir directory.";
        delete[] path;
        return false;
    }

    delete[] path;
    return true;
}

bool FileValidator::IsFileValid(const std::string &configFile, std::string &errMsg)
{
    if (!ock::mf::FileUtil::Exist(configFile)) {
        errMsg = "The input file is not a regular file or not exists";
        return false;
    }

    size_t fileSize = ock::mf::FileUtil::GetFileSize(configFile);
    if (fileSize == 0) {
        errMsg = "The input file is empty";
    } else if (!CheckDataSize(fileSize)) {
        errMsg = "Read input file failed, file is too large.";
        return false;
    }
    return true;
}

bool FileValidator::CheckPermission(const std::string &filePath, const mode_t &mode, bool onlyCurrentUserOp,
                                    std::string &errMsg)
{
    struct stat buf;
    int ret = stat(filePath.c_str(), &buf);
    if (ret != 0) {
        errMsg = "Get file stat failed.";
        return false;
    }

    mode_t mask = PER_PERMISSION_MASK_RWX;
    const int perPermWidth = 3;
    std::vector<std::string> permMsg = {"Other group permission", "Owner group permission", "Owner permission"};
    for (int i = perPermWidth; i > 0; i--) {
        uint32_t curPerm = (buf.st_mode & (mask << ((i - 1) * perPermWidth))) >> ((i - 1) * perPermWidth);
        uint32_t maxPerm = (mode & (mask << ((i - 1) * perPermWidth))) >> ((i - 1) * perPermWidth);
        if ((curPerm | maxPerm) != maxPerm) {
            errMsg = " Check " + permMsg[i - 1] + " failed: Current permission is " + std::to_string(curPerm) +
                     ", but required no greater than " + std::to_string(maxPerm) + ".";
            return false;
        }
        const uint32_t readPerm = 4;
        const uint32_t noPerm = 0;
        if (onlyCurrentUserOp && i != perPermWidth && curPerm != noPerm && curPerm != readPerm) {
            errMsg = " Check " + permMsg[i - 1] + " failed: Current permission is " + std::to_string(curPerm) +
                     ", but required no write or execute permission.";
            return false;
        }
    }
    return true;
}
}  // namespace acc
}  // namespace ock