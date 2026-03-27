/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Embricks is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <dlfcn.h>

#include "dl_mf_api.h"

namespace ock {
namespace emb {
namespace underapi {
std::mutex DlMfApi::gMutex;
bool DlMfApi::gLoaded = false;
void *DlMfApi::gMfSmemHandle = nullptr;
const char *DlMfApi::gMfLibName = "libmf_smem.so";

mfSmemInitFunc DlMfApi::gMfSmemInit = nullptr;
mfSmemCreateConfigStoreFunc DlMfApi::gMfSmemCreateConfigStore = nullptr;
mfSmemSetExternLoggerFunc DlMfApi::gMfSmemSetExternLogger = nullptr;
mfSmemSetLogLevelFunc DlMfApi::gMfSmemSetLogLevel = nullptr;
mfSmemUnInitFunc DlMfApi::gMfSmemUnInit = nullptr;
mfSmemGetLastErrMsgFunc DlMfApi::gMfSmemGetLastErrMsg = nullptr;
mfSmemGetAndClearErrMsgFunc DlMfApi::gMfSmemGetAndClearErrMsg = nullptr;

Result DlMfApi::LoadLibrary(const std::string &libDirPath)
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return EM_OK;
    }

    std::string realPath;
    if (!Func::LibraryRealPath(libDirPath, std::string(gMfLibName), realPath)) {
        EM_LOG_ERROR(libDirPath << "get real library [" << gMfLibName << "] failed");
        return EM_FILE_NOT_FOUND;
    }

    /* dlopen library */
    gMfSmemHandle = dlopen(realPath.c_str(), RTLD_NOW | RTLD_NODELETE);
    if (gMfSmemHandle == nullptr) {
        EM_LOG_ERROR("Failed to open library [" << realPath << "], error: " << dlerror());
        return EM_DL_OPEN_LIB_FAILED;
    }

    /* load sym */
    DL_LOAD_SYM(gMfSmemInit, mfSmemInitFunc, gMfSmemHandle, "smem_init");
    DL_LOAD_SYM(gMfSmemCreateConfigStore, mfSmemCreateConfigStoreFunc, gMfSmemHandle, "smem_create_config_store");
    DL_LOAD_SYM(gMfSmemSetExternLogger, mfSmemSetExternLoggerFunc, gMfSmemHandle, "smem_set_extern_logger");
    DL_LOAD_SYM(gMfSmemSetLogLevel, mfSmemSetLogLevelFunc, gMfSmemHandle, "smem_set_log_level");
    DL_LOAD_SYM(gMfSmemUnInit, mfSmemUnInitFunc, gMfSmemHandle, "smem_uninit");
    DL_LOAD_SYM(gMfSmemGetLastErrMsg, mfSmemGetLastErrMsgFunc, gMfSmemHandle, "smem_get_last_err_msg");
    DL_LOAD_SYM(gMfSmemGetAndClearErrMsg, mfSmemGetAndClearErrMsgFunc, gMfSmemHandle,
                "smem_get_and_clear_last_err_msg");

    gLoaded = true;
    return EM_OK;
}

void DlMfApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }

    gMfSmemInit = nullptr;
    gMfSmemCreateConfigStore = nullptr;
    gMfSmemSetExternLogger = nullptr;
    gMfSmemSetLogLevel = nullptr;
    gMfSmemUnInit = nullptr;
    gMfSmemGetLastErrMsg = nullptr;
    gMfSmemGetAndClearErrMsg = nullptr;

    if (gMfSmemHandle != nullptr) {
        dlclose(gMfSmemHandle);
        gMfSmemHandle = nullptr;
    }
    gLoaded = false;
}
} // namespace underapi
} // namespace emb
} // namespace ock