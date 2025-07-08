/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <dlfcn.h>
#include <mutex>
#include "dl_hal_api.h"

namespace ock {
namespace mf {
bool DlHalApi::gLoaded = false;
std::mutex DlHalApi::gMutex;
void *DlHalApi::halHandle;

const char *DlHalApi::gAscendHalLibName = "libascend_hal.so";

halGvaReserveMemoryFun DlHalApi::pHalGvaReserveMemory = nullptr;
halGvaUnreserveMemoryFun DlHalApi::pHalGvaUnreserveMemory = nullptr;
halGvaAllocFun DlHalApi::pHalGvaAlloc = nullptr;
halGvaFreeFun DlHalApi::pHalGvaFree = nullptr;
halGvaOpenFun DlHalApi::pHalGvaOpen = nullptr;
halGvaCloseFun DlHalApi::pHalGvaClose = nullptr;

Result DlHalApi::LoadLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }

    halHandle = dlopen(gAscendHalLibName, RTLD_NOW);
    if (halHandle == nullptr) {
        BM_LOG_ERROR(
            "Failed to open library ["
            << gAscendHalLibName
            << "], please source ascend-toolkit set_env.sh, or add ascend driver lib path into LD_LIBRARY_PATH,"
            << " error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    /* load sym */
    DL_LOAD_SYM(pHalGvaReserveMemory, halGvaReserveMemoryFun, halHandle, "halGvaReserveMemory");
    DL_LOAD_SYM(pHalGvaUnreserveMemory, halGvaUnreserveMemoryFun, halHandle, "halGvaUnreserveMemory");
    DL_LOAD_SYM(pHalGvaAlloc, halGvaAllocFun, halHandle, "halGvaAlloc");
    DL_LOAD_SYM(pHalGvaFree, halGvaFreeFun, halHandle, "halGvaFree");
    DL_LOAD_SYM(pHalGvaOpen, halGvaOpenFun, halHandle, "halGvaOpen");
    DL_LOAD_SYM(pHalGvaClose, halGvaCloseFun, halHandle, "halGvaClose");

    gLoaded = true;
    return BM_OK;
}

void DlHalApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }

    pHalGvaReserveMemory = nullptr;
    pHalGvaUnreserveMemory = nullptr;
    pHalGvaAlloc = nullptr;
    pHalGvaFree = nullptr;
    pHalGvaOpen = nullptr;
    pHalGvaClose = nullptr;

    if (halHandle != nullptr) {
        dlclose(halHandle);
        halHandle = nullptr;
    }

    gLoaded = false;
}
}
}