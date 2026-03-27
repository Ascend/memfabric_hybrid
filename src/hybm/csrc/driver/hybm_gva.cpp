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

#include "hybm_common_include.h"
#include "devmm_svm_gva.h"
#include "dl_api.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "hybm_cmd.h"
#include "hybm_gva.h"

namespace ock {
namespace mf {

namespace {
int32_t initedLogicDeviceId = -1;
} // namespace

int32_t HybmGetInitedLogicDeviceId()
{
    return initedLogicDeviceId; // logicDeviceId
}

int32_t HybmModernInitMetaGva(void *globalMemoryBase, size_t allocSize, void **allocHandle)
{
    drv_mem_handle_t **handle = (drv_mem_handle_t **)allocHandle;
    uint64_t va = SVM_END_ADDR - GB;
    auto ret = DlHalApi::HalMemAddressReserve(&globalMemoryBase, GB, 0, reinterpret_cast<void *>(va), 0);
    if (ret != 0) {
        BM_LOG_ERROR("prepare virtual memory size(" << GB << ") failed. ret: " << ret);
        return BM_ERROR;
    }
    drv_mem_prop memprop;
    memprop.side = MEM_DEV_SIDE;
    memprop.devid = initedLogicDeviceId;
    memprop.module_id = 0;
    memprop.pg_type = MEM_HUGE_PAGE_TYPE;
    memprop.mem_type = MEM_HBM_TYPE;
    memprop.reserve = 0;
    ret = DlHalApi::HalMemCreate(handle, allocSize, &memprop, 0);
    if (ret != BM_OK) {
        BM_LOG_ERROR("HalMemCreate failed: " << ret);
        return BM_ERROR;
    }
    ret = DlHalApi::HalMemMap(reinterpret_cast<void *>(HYBM_DEVICE_META_ADDR), allocSize, 0, *handle, 0);
    if (ret != BM_OK) {
        BM_LOG_DEBUG("HalMemMap failed: " << ret);
        DlHalApi::HalMemRelease(*handle);
        return BM_ERROR;
    }
    return BM_OK;
}

int32_t HybmLegacyInitMetaGva(void *globalMemoryBase, size_t allocSize, uint64_t flags)
{
    auto ret = drv::HalGvaReserveMemory((uint64_t *)&globalMemoryBase, allocSize, initedLogicDeviceId, flags);
    if (ret != 0 || reinterpret_cast<uint64_t>(globalMemoryBase) != (SVM_END_ADDR - GB)) {
        if (ret == 0 && globalMemoryBase != nullptr) {
            (void)drv::HalGvaUnreserveMemory((uint64_t)globalMemoryBase);
        }
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
    return BM_OK;
}

int32_t hybm_init_hbm_gva(uint16_t deviceId, uint64_t flags, uint64_t &baseAddress,
                          AscendSocType socType, void **allocHandle)
{
#if !defined(ASCEND_NPU)
    return BM_OK;
#else
    initedLogicDeviceId = Func::GetLogicDeviceId(deviceId);
    if (initedLogicDeviceId < 0) {
        BM_LOG_ERROR("Failed to get logic deviceId: " << deviceId);
        return BM_ERROR;
    }
    BM_LOG_INFO("Success get deviceId: " << deviceId << ", logicDeviceId: " << initedLogicDeviceId
                << " sco:" << (int)socType << " ver:" << HybmGvaVersion());
    auto ret = DlAclApi::AclrtSetDevice(deviceId);
    if (ret != BM_OK) {
        BM_LOG_ERROR("set device id to be " << deviceId << " failed: " << ret);
        return BM_ERROR;
    }
    drv::HybmInitialize(initedLogicDeviceId, DlHalApi::GetFd());

    if ((flags & HYBM_FLAG_INIT_SHMEM_META) == 0) {
        BM_LOG_DEBUG("skip init shm meta space:" << flags);
        baseAddress = 0;
        return BM_OK;
    } else {
        BM_LOG_DEBUG("restore init flag");
        flags &= ~HYBM_FLAG_INIT_SHMEM_META;
    }

    void *globalMemoryBase = nullptr;
    size_t allocSize = HYBM_DEVICE_INFO_SIZE; // 申请meta空间
    if ((socType == AscendSocType::ASCEND_950) || (HybmGetGvaVersion() == HYBM_GVA_V4)) {
        ret = HybmModernInitMetaGva(globalMemoryBase, allocSize, allocHandle);
    } else {
        ret = HybmLegacyInitMetaGva(globalMemoryBase, allocSize, flags);
    }
    if (ret != BM_OK) {
        BM_LOG_ERROR("hybm init meta gva failed: " << ret);
        return BM_ERROR;
    }
    baseAddress = reinterpret_cast<uint64_t>(globalMemoryBase);
    return BM_OK;
#endif
}

} // namespace mf
} // namespace ock