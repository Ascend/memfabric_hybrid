/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "devmm_svm_gva.h"
#include "dl_api.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "hybm_cmd.h"
#include "hybm_gvm_user.h"
#include "hybm_functions.h"

#include "hybm_gva.h"

namespace ock {
namespace mf {

namespace {
bool initedGvm = false;
int32_t initedLogicDeviceId = -1;
} // namespace

bool HybmGvmHasInited()
{
    return initedGvm;
}

int32_t HybmGetInitedLogicDeviceId()
{
    return initedLogicDeviceId;
}

int32_t hybm_init_hbm_gva(uint16_t deviceId, uint64_t flags, uint64_t& baseAddress)
{
#ifndef USE_CANN
    return BM_OK;
#else
    initedLogicDeviceId = Func::GetLogicDeviceId(deviceId);
    if (initedLogicDeviceId < 0) {
        BM_LOG_ERROR("Failed to get logic deviceId: " << deviceId);
        return BM_ERROR;
    }

    auto ret = DlAclApi::AclrtSetDevice(deviceId);
    if (ret != BM_OK) {
        DlApi::CleanupLibrary();
        BM_LOG_ERROR("set device id to be " << deviceId << " failed: " << ret);
        return BM_ERROR;
    }
#ifndef USE_VMM
    if (flags & HYBM_INIT_GVM_FLAG) {
        ret = hybm_gvm_init(initedLogicDeviceId);
        if (ret != 0) {
            BM_LOG_ERROR("init hybm gvm failed: " << ret);
            initedGvm = false;
        } else {
            initedGvm = true;
        }
    } else {
        initedGvm = false;
    }
#endif

    void *globalMemoryBase = nullptr;
    size_t allocSize = HYBM_DEVICE_INFO_SIZE; // 申请meta空间
    drv::HybmInitialize(initedLogicDeviceId, DlHalApi::GetFd());
    ret = drv::HalGvaReserveMemory((uint64_t *)&globalMemoryBase, allocSize, initedLogicDeviceId, flags);
    if (ret != 0) {
        DlApi::CleanupLibrary();
        BM_LOG_ERROR("initialize mete memory with size: " << allocSize << ", flag: " << flags << " failed: " << ret);
        return BM_ERROR;
    }

    ret = drv::HalGvaAlloc(HYBM_DEVICE_META_ADDR, HYBM_DEVICE_INFO_SIZE, 0);
    if (ret != BM_OK) {
        DlApi::CleanupLibrary();
        (void)drv::HalGvaUnreserveMemory((uint64_t)globalMemoryBase);
        BM_LOG_ERROR("HalGvaAlloc hybm meta memory failed: " << ret);
        return BM_MALLOC_FAILED;
    }

    baseAddress = reinterpret_cast<uint64_t>(globalMemoryBase);
    return BM_OK;
#endif
}

} // namespace mf
} // namespace ock