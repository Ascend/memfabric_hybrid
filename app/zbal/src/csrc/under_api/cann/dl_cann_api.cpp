/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <dlfcn.h>

#include "dl_cann_api.h"

namespace zbal {
namespace underapi {
bool DlCannApi::gLoaded = false;
std::mutex DlCannApi::gMutex;
void *DlCannApi::gAclHandle;
const char *DlCannApi::gAscendAclLibName = "libascendcl.so";

aclrtGetSocNameFunc DlCannApi::pAclrtGetSocName = nullptr;
rtGetDeviceInfoFunc DlCannApi::pRtGetDeviceInfo = nullptr;
aclrtGetDeviceFunc DlCannApi::pAclrtGetDevice = nullptr;
aclrtSetDeviceFunc DlCannApi::pAclrtSetDevice = nullptr;
aclrtSynchronizeStreamFunc DlCannApi::pAclrtSynchronizeStream = nullptr;
aclrtMallocFunc DlCannApi::pAclrtMalloc = nullptr;
aclrtFreeFunc DlCannApi::pAclrtFree = nullptr;
aclrtMallocHostFunc DlCannApi::pAclrtMallocHost = nullptr;
aclrtFreeHostFunc DlCannApi::pAclrtFreeHost = nullptr;
aclrtMemcpyFunc DlCannApi::pAclrtMemcpy = nullptr;
aclrtMemcpyAsyncFunc DlCannApi::pAclrtMemcpyAsync = nullptr;
aclrtMemsetFunc DlCannApi::pAclrtMemset = nullptr;
rtGetLogicDevIdByUserDevIdFunc DlCannApi::pRtGetLogicDevIdByUserDevId = nullptr;
rtGetC2cCtrlAddrFunc DlCannApi::pRtGetC2cCtrlAddr = nullptr;
aclrtGetResInCurrentThreadFunc DlCannApi::pAclrtGetResInCurrentThread = nullptr;
aclrtHostRegisterFunc DlCannApi::pAclrtHostRegister = nullptr;
aclrtHostUnregisterFunc DlCannApi::pAclrtHostUnregister = nullptr;
aclrtSynchronizeEventFunc DlCannApi::pAclrtSynchronizeEvent = nullptr;
aclrtReserveMemAddressFunc DlCannApi::pAclrtReserveMemAddress = nullptr;
aclrtReleaseMemAddressFunc DlCannApi::pAclrtReleaseMemAddress = nullptr;

ZResult DlCannApi::LoadLibrary(const std::string &libDirPath)
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return Z_OK;
    }

    std::string realPath;
    if (!Func::LibraryRealPath(libDirPath, std::string(gAscendAclLibName), realPath)) {
        ZBAL_LOG_ERROR(libDirPath << " get real library [" << gAscendAclLibName << "] failed");
        return Z_FILE_NOT_FOUND;
    }

    /* dlopen library */
    gAclHandle = dlopen(realPath.c_str(), RTLD_NOW | RTLD_NODELETE);
    if (gAclHandle == nullptr) {
        ZBAL_LOG_ERROR("Failed to open library [" << realPath << "], error: " << dlerror());
        return Z_DL_OPEN_LIB_FAILED;
    }

    /* load sym */
    DL_LOAD_SYM(pAclrtGetSocName, aclrtGetSocNameFunc, gAclHandle, "aclrtGetSocName");
    DL_LOAD_SYM(pRtGetDeviceInfo, rtGetDeviceInfoFunc, gAclHandle, "rtGetDeviceInfo");
    DL_LOAD_SYM(pAclrtGetDevice, aclrtGetDeviceFunc, gAclHandle, "aclrtGetDevice");
    DL_LOAD_SYM(pAclrtSetDevice, aclrtSetDeviceFunc, gAclHandle, "aclrtSetDevice");
    DL_LOAD_SYM(pAclrtSynchronizeStream, aclrtSynchronizeStreamFunc, gAclHandle, "aclrtSynchronizeStream");
    DL_LOAD_SYM(pAclrtMalloc, aclrtMallocFunc, gAclHandle, "aclrtMalloc");
    DL_LOAD_SYM(pAclrtFree, aclrtFreeFunc, gAclHandle, "aclrtFree");
    DL_LOAD_SYM(pAclrtMallocHost, aclrtMallocHostFunc, gAclHandle, "aclrtMallocHost");
    DL_LOAD_SYM(pAclrtFreeHost, aclrtFreeHostFunc, gAclHandle, "aclrtFreeHost");
    DL_LOAD_SYM(pAclrtMemcpy, aclrtMemcpyFunc, gAclHandle, "aclrtMemcpy");
    DL_LOAD_SYM(pAclrtMemcpyAsync, aclrtMemcpyAsyncFunc, gAclHandle, "aclrtMemcpyAsync");
    DL_LOAD_SYM(pAclrtMemset, aclrtMemsetFunc, gAclHandle, "aclrtMemset");
    DL_LOAD_SYM(pRtGetLogicDevIdByUserDevId, rtGetLogicDevIdByUserDevIdFunc, gAclHandle, "rtGetLogicDevIdByUserDevId");
    DL_LOAD_SYM(pRtGetC2cCtrlAddr, rtGetC2cCtrlAddrFunc, gAclHandle, "rtGetC2cCtrlAddr");
    DL_LOAD_SYM(pAclrtGetResInCurrentThread, aclrtGetResInCurrentThreadFunc, gAclHandle, "aclrtGetResInCurrentThread");
    DL_LOAD_SYM(pAclrtHostRegister, aclrtHostRegisterFunc, gAclHandle, "aclrtHostRegister");
    DL_LOAD_SYM(pAclrtHostUnregister, aclrtHostUnregisterFunc, gAclHandle, "aclrtHostUnregister");
    DL_LOAD_SYM(pAclrtSynchronizeEvent, aclrtSynchronizeEventFunc, gAclHandle, "aclrtSynchronizeEvent");
    DL_LOAD_SYM(pAclrtReserveMemAddress, aclrtReserveMemAddressFunc, gAclHandle, "aclrtReserveMemAddress");
    DL_LOAD_SYM(pAclrtReleaseMemAddress, aclrtReleaseMemAddressFunc, gAclHandle, "aclrtReleaseMemAddress");

    gLoaded = true;

    return Z_OK;
}

void DlCannApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }

    pAclrtGetSocName = nullptr;
    pRtGetDeviceInfo = nullptr;
    pAclrtGetDevice = nullptr;
    pAclrtSetDevice = nullptr;
    pAclrtSynchronizeStream = nullptr;
    pAclrtMalloc = nullptr;
    pAclrtFree = nullptr;
    pAclrtMallocHost = nullptr;
    pAclrtFreeHost = nullptr;
    pAclrtMemcpy = nullptr;
    pAclrtMemcpyAsync = nullptr;
    pAclrtMemset = nullptr;
    pRtGetLogicDevIdByUserDevId = nullptr;
    pRtGetC2cCtrlAddr = nullptr;
    pAclrtGetResInCurrentThread = nullptr;
    pAclrtHostRegister = nullptr;
    pAclrtHostUnregister = nullptr;
    pAclrtSynchronizeEvent = nullptr;
    pAclrtReserveMemAddress = nullptr;
    pAclrtReleaseMemAddress = nullptr;

    if (gAclHandle != nullptr) {
        dlclose(gAclHandle);
        gAclHandle = nullptr;
    }
    gLoaded = false;
}
} // namespace underapi
} // namespace zbal