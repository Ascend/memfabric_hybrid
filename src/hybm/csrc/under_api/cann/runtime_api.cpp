/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <dlfcn.h>

#include "runtime_api.h"
#include "hybm_common_include.h"

namespace ock {
namespace mf {
bool RuntimeApi::gLoaded = false;
std::mutex RuntimeApi::gMutex;
void *RuntimeApi::rtHandle;
void *RuntimeApi::halHandle;

const char *RuntimeApi::gAscendAclLibName = "libascendcl.so";
const char *RuntimeApi::gAscendHalLibName = "libascend_hal.so";

aclrtGetDeviceFunc RuntimeApi::pAclrtGetDevice = nullptr;
aclrtSetDeviceFunc RuntimeApi::pAclrtSetDevice = nullptr;
aclrtDeviceEnablePeerAccessFunc RuntimeApi::pAclrtDeviceEnablePeerAccess = nullptr;
aclrtCreateStreamFunc RuntimeApi::pAclrtCreateStream = nullptr;
aclrtDestroyStreamFunc RuntimeApi::pAclrtDestroyStream = nullptr;
aclrtSynchronizeStreamFunc RuntimeApi::pAclrtSynchronizeStream = nullptr;
aclrtMallocFunc RuntimeApi::pAclrtMalloc = nullptr;
aclrtFreeFunc RuntimeApi::pAclrtFree = nullptr;
aclrtMemcpyFunc RuntimeApi::pAclrtMemcpy = nullptr;
aclrtMemcpyAsyncFunc RuntimeApi::pAclrtMemcpyAsync = nullptr;
aclrtMemsetFunc RuntimeApi::pAclrtMemset = nullptr;
rtDeviceGetBareTgidFunc RuntimeApi::pRtDeviceGetBareTgid = nullptr;
rtGetDeviceInfoFunc RuntimeApi::pRtGetDeviceInfo = nullptr;
rtSetIpcMemorySuperPodPidFunc RuntimeApi::pRtSetIpcMemorySuperPodPid = nullptr;
rtIpcDestroyMemoryNameFunc RuntimeApi::pRtIpcDestroyMemoryName = nullptr;
rtIpcSetMemoryNameFunc RuntimeApi::pRtIpcSetMemoryName = nullptr;

halGvaReserveMemoryFun RuntimeApi::pHalGvaReserveMemory = nullptr;
halGvaUnreserveMemoryFun RuntimeApi::pHalGvaUnreserveMemory = nullptr;
halGvaAllocFun RuntimeApi::pHalGvaAlloc = nullptr;
halGvaFreeFun RuntimeApi::pHalGvaFree = nullptr;
halGvaOpenFun RuntimeApi::pHalGvaOpen = nullptr;
halGvaCloseFun RuntimeApi::pHalGvaClose = nullptr;

Result RuntimeApi::LoadLibrary(const std::string &libDirPath)
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }

    std::string realPath;
    auto result = Func::LibraryRealPath(libDirPath, std::string(gAscendAclLibName), realPath);
    if (result != BM_OK) {
        BM_LOG_ERROR(libDirPath << " get lib path failed, ret:" << result);
        return result;
    }

    /* dlopen library */
    rtHandle = dlopen(realPath.c_str(), RTLD_NOW);
    if (rtHandle == nullptr) {
        BM_LOG_ERROR("Failed to open library [" << realPath << "], error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    halHandle = dlopen(gAscendHalLibName, RTLD_NOW);
    if (halHandle == nullptr) {
        dlclose(rtHandle);
        BM_LOG_ERROR("Failed to open library [" << gAscendHalLibName <<
            "], please source ascend-toolkit set_env.sh, or add ascend driver lib path into LD_LIBRARY_PATH," <<
            " error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    /* load sym */
    DL_LOAD_SYM(pAclrtGetDevice, aclrtGetDeviceFunc, rtHandle, "aclrtGetDevice");
    DL_LOAD_SYM(pAclrtSetDevice, aclrtSetDeviceFunc, rtHandle, "aclrtSetDevice");
    DL_LOAD_SYM(pAclrtDeviceEnablePeerAccess, aclrtDeviceEnablePeerAccessFunc, rtHandle, "aclrtDeviceEnablePeerAccess");
    DL_LOAD_SYM(pAclrtCreateStream, aclrtCreateStreamFunc, rtHandle, "aclrtCreateStream");
    DL_LOAD_SYM(pAclrtDestroyStream, aclrtDestroyStreamFunc, rtHandle, "aclrtDestroyStream");
    DL_LOAD_SYM(pAclrtSynchronizeStream, aclrtSynchronizeStreamFunc, rtHandle, "aclrtSynchronizeStream");
    DL_LOAD_SYM(pAclrtMalloc, aclrtMallocFunc, rtHandle, "aclrtMalloc");
    DL_LOAD_SYM(pAclrtFree, aclrtFreeFunc, rtHandle, "aclrtFree");
    DL_LOAD_SYM(pAclrtMemcpy, aclrtMemcpyFunc, rtHandle, "aclrtMemcpy");
    DL_LOAD_SYM(pAclrtMemcpyAsync, aclrtMemcpyAsyncFunc, rtHandle, "aclrtMemcpyAsync");
    DL_LOAD_SYM(pAclrtMemset, aclrtMemsetFunc, rtHandle, "aclrtMemset");
    DL_LOAD_SYM(pRtDeviceGetBareTgid, rtDeviceGetBareTgidFunc, rtHandle, "rtDeviceGetBareTgid");
    DL_LOAD_SYM(pRtGetDeviceInfo, rtGetDeviceInfoFunc, rtHandle, "rtGetDeviceInfo");
    DL_LOAD_SYM(pRtSetIpcMemorySuperPodPid, rtSetIpcMemorySuperPodPidFunc, rtHandle, "rtSetIpcMemorySuperPodPid");
    DL_LOAD_SYM(pRtIpcSetMemoryName, rtIpcSetMemoryNameFunc, rtHandle, "rtIpcSetMemoryName");
    DL_LOAD_SYM(pRtIpcDestroyMemoryName, rtIpcDestroyMemoryNameFunc, rtHandle, "rtIpcDestroyMemoryName");

    DL_LOAD_SYM(pHalGvaReserveMemory, halGvaReserveMemoryFun, halHandle, "halGvaReserveMemory");
    DL_LOAD_SYM(pHalGvaUnreserveMemory, halGvaUnreserveMemoryFun, halHandle, "halGvaUnreserveMemory");
    DL_LOAD_SYM(pHalGvaAlloc, halGvaAllocFun, halHandle, "halGvaAlloc");
    DL_LOAD_SYM(pHalGvaFree, halGvaFreeFun, halHandle, "halGvaFree");
    DL_LOAD_SYM(pHalGvaOpen, halGvaOpenFun, halHandle, "halGvaOpen");
    DL_LOAD_SYM(pHalGvaClose, halGvaCloseFun, halHandle, "halGvaClose");

    gLoaded = true;
    return BM_OK;
}
}
}