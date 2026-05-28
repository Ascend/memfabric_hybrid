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
#include "dl_hcomm_api.h"

namespace ock {
namespace mf {

bool DlHcommApi::gLoaded = false;
std::mutex DlHcommApi::gMutex;
void *DlHcommApi::hcommHandle = nullptr;
const char *DlHcommApi::hcommLibName = "libhcomm.so";

hcommEndpointCreateFunc DlHcommApi::gHcommEndpointCreate = nullptr;
hcommEndpointDestroyFunc DlHcommApi::gHcommEndpointDestroy = nullptr;
hcommMemRegFunc DlHcommApi::gHcommMemReg = nullptr;
hcommMemUnregFunc DlHcommApi::gHcommMemUnreg = nullptr;
hcommMemExportFunc DlHcommApi::gHcommMemExport = nullptr;
hcommMemImportFunc DlHcommApi::gHcommMemImport = nullptr;
hcommMemUnimportFunc DlHcommApi::gHcommMemUnimport = nullptr;
hcommChannelCreateFunc DlHcommApi::gHcommChannelCreate = nullptr;
hcommChannelDestroyFunc DlHcommApi::gHcommChannelDestroy = nullptr;
hcommThreadAllocFunc DlHcommApi::gHcommThreadAlloc = nullptr;
hcommThreadFreeFunc DlHcommApi::gHcommThreadFree = nullptr;
hcommReadOnThreadFunc DlHcommApi::gHcommReadOnThread = nullptr;
hcommWriteOnThreadFunc DlHcommApi::gHcommWriteOnThread = nullptr;
hcommChannelFenceOnThreadFunc DlHcommApi::gHcommChannelFenceOnThread = nullptr;
hcommBatchModeStartFunc DlHcommApi::gHcommBatchModeStart = nullptr;
hcommBatchModeEndFunc DlHcommApi::gHcommBatchModeEnd = nullptr;
hcommBatchTransferOnThreadFunc DlHcommApi::gHcommBatchTransferOnThread = nullptr;

Result DlHcommApi::LoadLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }

    hcommHandle = dlopen(hcommLibName, RTLD_NOW | RTLD_NODELETE);
    if (hcommHandle == nullptr) {
        BM_LOG_ERROR(
            "Failed to open library ["
            << hcommLibName
            << "], please source ascend-toolkit set_env.sh, or add ascend driver lib path into LD_LIBRARY_PATH,"
            << " error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    DL_LOAD_SYM(gHcommEndpointCreate, hcommEndpointCreateFunc, hcommHandle, "HcommEndpointCreate");
    DL_LOAD_SYM(gHcommEndpointDestroy, hcommEndpointDestroyFunc, hcommHandle, "HcommEndpointDestroy");
    DL_LOAD_SYM(gHcommMemReg, hcommMemRegFunc, hcommHandle, "HcommMemReg");
    DL_LOAD_SYM(gHcommMemUnreg, hcommMemUnregFunc, hcommHandle, "HcommMemUnreg");
    DL_LOAD_SYM(gHcommMemExport, hcommMemExportFunc, hcommHandle, "HcommMemExport");
    DL_LOAD_SYM(gHcommMemImport, hcommMemImportFunc, hcommHandle, "HcommMemImport");
    DL_LOAD_SYM(gHcommMemUnimport, hcommMemUnimportFunc, hcommHandle, "HcommMemUnimport");
    DL_LOAD_SYM(gHcommChannelCreate, hcommChannelCreateFunc, hcommHandle, "HcommChannelCreate");
    DL_LOAD_SYM(gHcommChannelDestroy, hcommChannelDestroyFunc, hcommHandle, "HcommChannelDestroy");
    DL_LOAD_SYM(gHcommThreadAlloc, hcommThreadAllocFunc, hcommHandle, "HcommThreadAlloc");
    DL_LOAD_SYM(gHcommThreadFree, hcommThreadFreeFunc, hcommHandle, "HcommThreadFree");
    DL_LOAD_SYM(gHcommReadOnThread, hcommReadOnThreadFunc, hcommHandle, "HcommReadOnThread");
    DL_LOAD_SYM(gHcommWriteOnThread, hcommWriteOnThreadFunc, hcommHandle, "HcommWriteOnThread");
    DL_LOAD_SYM(gHcommChannelFenceOnThread, hcommChannelFenceOnThreadFunc, hcommHandle,
                "HcommChannelFenceOnThread");
    DL_LOAD_SYM_OPTIONAL(gHcommBatchModeStart, hcommBatchModeStartFunc, hcommHandle, "HcommBatchModeStart");
    DL_LOAD_SYM_OPTIONAL(gHcommBatchModeEnd, hcommBatchModeEndFunc, hcommHandle, "HcommBatchModeEnd");
    DL_LOAD_SYM_OPTIONAL(gHcommBatchTransferOnThread, hcommBatchTransferOnThreadFunc, hcommHandle,
                         "HcommBatchTransferOnThread");

    BM_LOG_INFO("LoadLibrary for DlHcommApi success");
    gLoaded = true;
    return BM_OK;
}

void DlHcommApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }

    gHcommEndpointCreate = nullptr;
    gHcommEndpointDestroy = nullptr;
    gHcommMemReg = nullptr;
    gHcommMemUnreg = nullptr;
    gHcommMemExport = nullptr;
    gHcommMemImport = nullptr;
    gHcommMemUnimport = nullptr;
    gHcommChannelCreate = nullptr;
    gHcommChannelDestroy = nullptr;
    gHcommThreadAlloc = nullptr;
    gHcommThreadFree = nullptr;
    gHcommReadOnThread = nullptr;
    gHcommWriteOnThread = nullptr;
    gHcommChannelFenceOnThread = nullptr;
    gHcommBatchModeStart = nullptr;
    gHcommBatchModeEnd = nullptr;
    gHcommBatchTransferOnThread = nullptr;

    if (hcommHandle != nullptr) {
        dlclose(hcommHandle);
        hcommHandle = nullptr;
    }
    gLoaded = false;
}

} // namespace mf
} // namespace ock
