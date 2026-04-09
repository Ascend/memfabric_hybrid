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
#include "dl_hccl_api.h"

namespace ock {
namespace mf {
bool DlHcclApi::gLoaded = false;
std::mutex DlHcclApi::gMutex;
void *DlHcclApi::hcclHandle = nullptr;

const char *gHcclLibName = "libhccl.so";
const char *gHcommLibName = "libhcomm.so";

hcclCommInitClusterInfoFunc DlHcclApi::gHcclCommInitClusterInfo = nullptr;
hcclCommDestroyFunc DlHcclApi::gHcclCommDestroy = nullptr;

Result DlHcclApi::LoadLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }

    hcclHandle = dlopen(gHcommLibName, RTLD_NOW | RTLD_NODELETE);
    if (hcclHandle == nullptr) {
        hcclHandle = dlopen(gHcclLibName, RTLD_NOW | RTLD_NODELETE);
    }
    if (hcclHandle == nullptr) {
        BM_LOG_ERROR(
                "Failed to open library ["
                << gHcclLibName << "," << gHcommLibName
                << "], please source ascend-toolkit set_env.sh, or add ascend driver lib path into LD_LIBRARY_PATH,"
                << " error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    /* load sym */
    DL_LOAD_SYM(gHcclCommInitClusterInfo, hcclCommInitClusterInfoFunc, hcclHandle, "HcclCommInitClusterInfoMemConfig");
    DL_LOAD_SYM(gHcclCommDestroy, hcclCommDestroyFunc, hcclHandle, "HcclCommDestroy");
    BM_LOG_INFO("LoadLibrary for DlHcclApi success");
    gLoaded = true;
    return BM_OK;
}

void DlHcclApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }

    gHcclCommInitClusterInfo = nullptr;

    if (hcclHandle != nullptr) {
        dlclose(hcclHandle);
        hcclHandle = nullptr;
    }

    gLoaded = false;
}
} // namespace mf
} // namespace ock