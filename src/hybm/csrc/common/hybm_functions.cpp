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

#include "hybm_functions.h"

namespace ock {
namespace mf {
bool Func::Realpath(std::string &path)
{
    if (path.empty() || path.size() > PATH_MAX) {
        BM_LOG_ERROR("Failed to get realpath as path is invalid");
        return false;
    }

    /* It will allocate memory to store path */
    char *realPath = realpath(path.c_str(), nullptr);
    if (realPath == nullptr) {
        BM_LOG_ERROR("Failed to get realpath as error " << errno);
        return false;
    }

    path = realPath;
    free(realPath);
    realPath = nullptr;
    return true;
}

Result Func::LibraryRealPath(const std::string &libDirPath, const std::string &libName, std::string &realPath)
{
    std::string tmpFullPath = libDirPath;
    if (!Realpath(tmpFullPath)) {
        return BM_INVALID_PARAM;
    }

    if (tmpFullPath.back() != '/') {
        tmpFullPath.push_back('/');
    }

    tmpFullPath.append(libName);
    auto ret = ::access(tmpFullPath.c_str(), F_OK);
    if (ret != 0) {
        BM_LOG_ERROR("libDirPath cannot be accessed, ret: " << ret);
        return BM_FILE_NOT_ACCESS;
    }

    realPath = tmpFullPath;
    return BM_OK;
}
}
}