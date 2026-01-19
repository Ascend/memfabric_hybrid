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

#include "devmm_svm_gva.h"
#include "dl_api.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "hybm_cmd.h"
#include "hybm_functions.h"

#include "hybm_gva.h"

namespace ock {
namespace mf {

namespace {
int32_t initedLogicDeviceId = -1;
} // namespace

int32_t HybmGetInitedLogicDeviceId()
{
    return initedLogicDeviceId;
}

int32_t hybm_init_hbm_gva(uint16_t deviceId, uint64_t flags, uint64_t &baseAddress)
{
#if !defined(ASCEND_NPU)
    return BM_OK;
#else
    initedLogicDeviceId = Func::GetLogicDeviceId(deviceId);
    if (initedLogicDeviceId < 0) {
        BM_LOG_ERROR("Failed to get logic deviceId: " << deviceId);
        return BM_ERROR;
    }
    BM_LOG_INFO("Success get deviceId: " << deviceId << ", logicDeviceId: " << initedLogicDeviceId);
    auto ret = DlAclApi::AclrtSetDevice(deviceId);
    if (ret != BM_OK) {
        BM_LOG_ERROR("set device id to be " << deviceId << " failed: " << ret);
        return BM_ERROR;
    }

    if (flags & HYBM_FLAG_INIT_SHMEM_META == 0) {
        BM_LOG_DEBUG("skip init shm meta space:" << flags);
        baseAddress = 0;
        return BM_OK;
    } else {
        BM_LOG_DEBUG("restore init flag");
        flags &= ~HYBM_FLAG_INIT_SHMEM_META;
    }

    void *globalMemoryBase = nullptr;
    size_t allocSize = HYBM_DEVICE_INFO_SIZE; // 申请meta空间
    drv::HybmInitialize(initedLogicDeviceId, DlHalApi::GetFd());
    ret = drv::HalGvaReserveMemory((uint64_t *)&globalMemoryBase, allocSize, initedLogicDeviceId, flags);
    if (ret != 0 || reinterpret_cast<uint64_t>(globalMemoryBase) != (SVM_END_ADDR - GB)) {
        BM_LOG_ERROR("initialize mete memory failed: " << ret << " size:0x" << std::hex << allocSize <<
                     " flag:0x" << flags << " ret_addr:" << globalMemoryBase);
        return BM_ERROR;
    }

    ret = drv::HalGvaAlloc(HYBM_DEVICE_META_ADDR, HYBM_DEVICE_INFO_SIZE, 0);
    if (ret != BM_OK) {
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