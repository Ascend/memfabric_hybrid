/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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