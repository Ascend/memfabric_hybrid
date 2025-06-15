/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <dlfcn.h>
#include "dl_hccl_api.h"

namespace ock {
namespace mf {
bool DlHcclApi::gLoaded = false;
std::mutex DlHcclApi::gMutex;
void *DlHcclApi::hcclHandle;

const char *DlHcclApi::gHcclLibName = "libhccl.so";

hcclGetRootInfoFunc DlHcclApi::gHcclGetRootInfo;
hcclCommInitRootInfoFunc DlHcclApi::gHcclCommInitRootInfo;

Result DlHcclApi::LoadLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }

    hcclHandle = dlopen(gHcclLibName, RTLD_NOW);
    if (hcclHandle == nullptr) {
        BM_LOG_ERROR(
            "Failed to open library ["
            << gHcclLibName
            << "], please source ascend-toolkit set_env.sh, or add ascend driver lib path into LD_LIBRARY_PATH,"
            << " error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    /* load sym */
    DL_LOAD_SYM(gHcclGetRootInfo, hcclGetRootInfoFunc, hcclHandle, "HcclGetRootInfo");
    DL_LOAD_SYM(gHcclCommInitRootInfo, hcclCommInitRootInfoFunc, hcclHandle, "HcclCommInitRootInfo");

    gLoaded = true;
    return BM_OK;
}
}
}