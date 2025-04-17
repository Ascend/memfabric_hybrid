/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
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
    static bool Realpath(std::string &path)
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

    static Result LibraryRealPath(const std::string &libDirPath, const std::string &libName, std::string &realPath)
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
};

}
}
#endif // MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H
