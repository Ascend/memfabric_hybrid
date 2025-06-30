/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <dlfcn.h>
#include "smem_bm_api.h"

namespace ock {
namespace mmc {

bool MFSmemApi::gLoaded = false;
std::mutex MFSmemApi::gMutex;
void *MFSmemApi::gSmemHandle = nullptr;
const char *MFSmemApi::gSmemLibName = "libmf_smem.so";

smemInitFunc MFSmemApi::gSmemInit = nullptr;
smemUnInitFunc MFSmemApi::gSmemUnInit = nullptr;
smemSetExternLoggerFunc MFSmemApi::gSmemSetExternLogger = nullptr;
smemSetLogLevelFunc MFSmemApi::gSmemSetLogLevel = nullptr;
smemGetLastErrMsgFunc MFSmemApi::gSemGetLastErrMsg = nullptr;
smemGetAndClearLastErrMsgFunc MFSmemApi::gSmemGetAndClearLastErrMsg = nullptr;

smemBmConfigInitFunc MFSmemApi::gSmemBmConfigInit = nullptr;
smemBmInitFunc MFSmemApi::gSmemBmInit = nullptr;
smemBmUnInitFunc MFSmemApi::gSmemBmUnInit = nullptr;
smemBmGetRankIdFunc MFSmemApi::gSmemBmGetRankId = nullptr;
smemBmCreateFunc MFSmemApi::gSmemBmCreate = nullptr;
smemBmDestroyFunc MFSmemApi::gSmemBmDestroy = nullptr;
smemBmJoinFunc MFSmemApi::gSmemBmJoin = nullptr;
smemBmLeaveFunc MFSmemApi::gSmemBmLeave = nullptr;
smemBmGetLocalMemSizeFunc MFSmemApi::gSmemBmGetLocalMemSizeFunc = nullptr;
smemBmPtrFunc MFSmemApi::gSmemBmPtr = nullptr;
smemBmCopyFunc MFSmemApi::gSmemBmCopy = nullptr;
smemBmCopy2dFunc MFSmemApi::gSmemBmCopy2d = nullptr;

#define DL_LOAD_SYM(TARGET_FUNC_VAR, TARGET_FUNC_TYPE, FILE_HANDLE, SYMBOL_NAME)           \
    do {                                                                                   \
        TARGET_FUNC_VAR = (TARGET_FUNC_TYPE)dlsym(FILE_HANDLE, SYMBOL_NAME);               \
        if (TARGET_FUNC_VAR == nullptr) {                                                  \
            MMC_LOG_ERROR("Failed to call dlsym to load SYMBOL_NAME, error" << dlerror()); \
            dlclose(FILE_HANDLE);                                                          \
            return MMC_ERROR;                                                              \
        }                                                                                  \
    } while (0)

Result MFSmemApi::LoadLibrary(const std::string &libDirPath)
{
    MMC_LOG_DEBUG("try to load library: " << gSmemLibName << ", dir: " << libDirPath);
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return MMC_OK;
    }

    std::string realPath;
    if (!libDirPath.empty()) {
        auto ret = Func::LibraryRealPath(libDirPath, std::string(gSmemLibName), realPath);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR(libDirPath << "get lib path failed, ret: " << ret);
        }
    } else {
        realPath = std::string(gSmemLibName);
    }

    /* dlopen library */
    gSmemHandle = dlopen(realPath.c_str(), RTLD_NOW);
    if (gSmemHandle == nullptr) {
        MMC_LOG_ERROR("open library failed, path: " << realPath << ", error: " << dlerror());
        return MMC_ERROR;
    }

    /* load sym */
    DL_LOAD_SYM(gSmemInit, smemInitFunc, gSmemHandle, "smem_init");
    DL_LOAD_SYM(gSmemUnInit, smemUnInitFunc, gSmemHandle, "smem_uninit");
    DL_LOAD_SYM(gSmemSetExternLogger, smemSetExternLoggerFunc, gSmemHandle, "smem_set_extern_logger");
    DL_LOAD_SYM(gSmemSetLogLevel, smemSetLogLevelFunc, gSmemHandle, "smem_set_log_level");
    DL_LOAD_SYM(gSemGetLastErrMsg, smemGetLastErrMsgFunc, gSmemHandle, "smem_get_last_err_msg");
    DL_LOAD_SYM(gSmemGetAndClearLastErrMsg, smemGetAndClearLastErrMsgFunc, gSmemHandle, "smem_get_last_err_msg");

    DL_LOAD_SYM(gSmemBmConfigInit, smemBmConfigInitFunc, gSmemHandle, "smem_bm_config_init");
    DL_LOAD_SYM(gSmemBmInit, smemBmInitFunc, gSmemHandle, "smem_bm_init");
    DL_LOAD_SYM(gSmemBmUnInit, smemBmUnInitFunc, gSmemHandle, "smem_bm_uninit");
    DL_LOAD_SYM(gSmemBmGetRankId, smemBmGetRankIdFunc, gSmemHandle, "smem_bm_get_rank_id");
    DL_LOAD_SYM(gSmemBmCreate, smemBmCreateFunc, gSmemHandle, "smem_bm_create");
    DL_LOAD_SYM(gSmemBmDestroy, smemBmDestroyFunc, gSmemHandle, "smem_bm_destroy");
    DL_LOAD_SYM(gSmemBmJoin, smemBmJoinFunc, gSmemHandle, "smem_bm_join");
    DL_LOAD_SYM(gSmemBmLeave, smemBmLeaveFunc, gSmemHandle, "smem_bm_leave");
    DL_LOAD_SYM(gSmemBmGetLocalMemSizeFunc, smemBmGetLocalMemSizeFunc, gSmemHandle, "smem_bm_get_local_mem_size");
    DL_LOAD_SYM(gSmemBmPtr, smemBmPtrFunc, gSmemHandle, "smem_bm_ptr");
    DL_LOAD_SYM(gSmemBmCopy, smemBmCopyFunc, gSmemHandle, "smem_bm_copy");
    DL_LOAD_SYM(gSmemBmCopy2d, smemBmCopy2dFunc, gSmemHandle, "smem_bm_copy_2d");

    gLoaded = true;
    return MMC_OK;
}
}
}