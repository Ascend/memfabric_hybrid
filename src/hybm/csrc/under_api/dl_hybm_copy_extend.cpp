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
#include <dlfcn.h>
#include "dl_hybm_copy_extend.h"

namespace ock {
namespace mf {
bool DlHybmExtendApi::gLoaded = false;
std::mutex DlHybmExtendApi::gMutex;
void *DlHybmExtendApi::libHandle;
const char *DlHybmExtendApi::gHybmExtendLibName = "libmf_hybm_copy_extend.so";

hybmCopyExtendFunc DlHybmExtendApi::pHybmCopyExtend = nullptr;
hybmBatchCopyExtendFunc DlHybmExtendApi::pHybmBatchCopyExtend = nullptr;

Result DlHybmExtendApi::TryLoadLibrary()
{
    std::unique_lock<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }

    char *path = std::getenv("MEMFABRIC_HYBRID_EXTEND_LIB_PATH");
    if (path == nullptr) {
        BM_LOG_WARN("Environment MEMFABRIC_HYBRID_EXTEND_LIB_PATH is not set.");
        return BM_ERROR;
    }
    std::string libPath = std::string(path);
    if (!ock::mf::FileUtil::Realpath(libPath) || !ock::mf::FileUtil::IsDir(libPath)) {
        BM_LOG_WARN("Environment MEMFABRIC_HYBRID_EXTEND_LIB_PATH check failed.");
        return BM_ERROR;
    }

    std::string realPath;
    if (!FileUtil::LibraryRealPath(libPath, std::string(gHybmExtendLibName), realPath)) {
        BM_LOG_WARN(libPath << " get lib path failed");
        return BM_ERROR;
    }

    /* dlopen library */
    libHandle = dlopen(realPath.c_str(), RTLD_NOW | RTLD_NODELETE);
    if (libHandle == nullptr) {
        BM_LOG_WARN("Failed to open library [" << realPath << "], error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    /* load sym */
    DL_LOAD_SYM_OPTIONAL(pHybmCopyExtend, hybmCopyExtendFunc, libHandle, "hybm_copy_extend");
    DL_LOAD_SYM_OPTIONAL(pHybmBatchCopyExtend, hybmBatchCopyExtendFunc, libHandle, "hybm_batch_copy_extend");

    gLoaded = true;
    if (pHybmCopyExtend != nullptr && pHybmBatchCopyExtend != nullptr) {
        return BM_OK;
    } else {
        BM_LOG_WARN("not dlopen hybm fast api.");
        guard.unlock();
        CleanupLibrary();
        return BM_DL_FUNCTION_FAILED;
    }
}

void DlHybmExtendApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }

    pHybmCopyExtend = nullptr;
    pHybmBatchCopyExtend = nullptr;

    if (libHandle != nullptr) {
        dlclose(libHandle);
        libHandle = nullptr;
    }
    gLoaded = false;
}
}
}