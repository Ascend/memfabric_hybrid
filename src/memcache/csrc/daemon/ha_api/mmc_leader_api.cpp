/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <dlfcn.h>
#include "mmc_functions.h"
#include "mmc_leader_api.h"

namespace ock {
namespace mmc {

bool MmcLeaderApi::gLoaded = false;
std::mutex MmcLeaderApi::gMutex;
void *MmcLeaderApi::gLeaderLibHandle = nullptr;
const char *MmcLeaderApi::gLeaderLibName = "libmmc_leader.so";

MmcLeaderInitFunc MmcLeaderApi::gLeaderInit = nullptr;
MmcLeaderUnInitFunc MmcLeaderApi::gLeaderUnInit = nullptr;
MmcLeaderSetLoggerFunc MmcLeaderApi::gLeaderSetLogger = nullptr;

Result MmcLeaderApi::LoadLibrary(const std::string &libDirPath)
{
    MMC_LOG_DEBUG("try to load library: " << gLeaderLibName << ", dir: " << libDirPath);
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return MMC_OK;
    }

    std::string realPath;
    if (!libDirPath.empty()) {
        auto ret = Func::LibraryRealPath(libDirPath, std::string(gLeaderLibName), realPath);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR(libDirPath << "get lib path failed, ret: " << ret);
        }
    } else {
        realPath = std::string(gLeaderLibName);
    }

    /* dlopen library */
    gLeaderLibHandle = dlopen(realPath.c_str(), RTLD_NOW);
    if (gLeaderLibHandle == nullptr) {
        MMC_LOG_ERROR("open library failed, path: " << realPath << ", error: " << dlerror());
        return MMC_ERROR;
    }

    /* load sym */
    DL_LOAD_SYM(gLeaderInit, MmcLeaderInitFunc, gLeaderLibHandle, "mmc_leader_init");
    DL_LOAD_SYM(gLeaderUnInit, MmcLeaderUnInitFunc, gLeaderLibHandle, "mmc_leader_uninit");
    DL_LOAD_SYM(gLeaderSetLogger, MmcLeaderSetLoggerFunc, gLeaderLibHandle, "mmc_leader_set_logger");

    gLoaded = true;
    return MMC_OK;
}
}
}