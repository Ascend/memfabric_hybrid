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
#include <mutex>

#include "hybm_define.h"
#include "hybm_logger.h"
#include "dl_hal_api.h"

namespace ock {
namespace mf {
bool DlHalApi::gLoaded = false;
std::mutex DlHalApi::gMutex;
void *DlHalApi::halHandle;

const char *DlHalApi::gAscendHalLibName = "libascend_hal.so";

halSvmModuleAllocedSizeIncFunc DlHalApi::pSvmModuleAllocedSizeInc = nullptr;
halVirtAllocMemFromBaseFunc DlHalApi::pVirtAllocMemFromBase = nullptr;
halIoctlEnableHeapFunc DlHalApi::pIoctlEnableHeap = nullptr;
halGetHeapListByTypeFunc DlHalApi::pGetHeapListByType = nullptr;
halVirtSetHeapIdleFunc DlHalApi::pVirtSetHeapIdle = nullptr;
halVirtDestroyHeapV1Func DlHalApi::pVirtDestroyHeapV1 = nullptr;
halVirtDestroyHeapV2Func DlHalApi::pVirtDestroyHeapV2 = nullptr;
halVirtGetHeapMgmtFunc DlHalApi::pVirtGetHeapMgmt = nullptr;
halIoctlFreePagesFunc DlHalApi::pIoctlFreePages = nullptr;
halVaToHeapIdxFunc DlHalApi::pVaToHeapIdx = nullptr;
halVirtGetHeapFromQueueFunc DlHalApi::pVirtGetHeapFromQueue = nullptr;
halVirtNormalHeapUpdateInfoFunc DlHalApi::pVirtNormalHeapUpdateInfo = nullptr;
halVaToHeapFunc DlHalApi::pVaToHeap = nullptr;
int *DlHalApi::pHalFd = nullptr;

halAssignNodeDataFunc DlHalApi::pAssignNodeData = nullptr;
halInsertIdleSizeTreeFunc DlHalApi::pInsertIdleSizeTree = nullptr;
halInsertIdleVaTreeFunc DlHalApi::pInsertIdleVaTree = nullptr;
halAllocRbtreeNodeFunc DlHalApi::pAllocRbtreeNode = nullptr;
halEraseIdleVaTreeFunc DlHalApi::pEraseIdleVaTree = nullptr;
halEraseIdleSizeTreeFunc DlHalApi::pEraseIdleSizeTree = nullptr;
halGetAllocedNodeInRangeFunc DlHalApi::pGetAllocedNodeInRange = nullptr;
halGetIdleVaNodeInRangeFunc DlHalApi::pGetIdleVaNodeInRange = nullptr;
halInsertAllocedTreeFunc DlHalApi::pInsertAllocedTree = nullptr;
halFreeRbtreeNodeFunc DlHalApi::pFreeRbtreeNode = nullptr;

halSqTaskSendFunc DlHalApi::pHalSqTaskSend = nullptr;
halCqReportRecvFunc DlHalApi::pHalCqReportRecv = nullptr;
halSqCqAllocateFunc DlHalApi::pHalSqCqAllocate = nullptr;
halSqCqFreeFunc DlHalApi::pHalSqCqFree = nullptr;
halResourceIdAllocFunc DlHalApi::pHalResourceIdAlloc = nullptr;
halResourceIdFreeFunc DlHalApi::pHalResourceIdFree = nullptr;
halResourceConfigFunc DlHalApi::pHalResourceConfig = nullptr;
halSqCqQueryFunc DlHalApi::pHalSqCqQuery = nullptr;
halHostRegisterFunc DlHalApi::pHalHostRegister = nullptr;
halHostUnregisterExFunc DlHalApi::pHalHostUnregisterEx = nullptr;
drvNotifyIdAddrOffsetFunc DlHalApi::pDrvNotifyIdAddrOffset = nullptr;

halMemAddressReserveFunc DlHalApi::pHalMemAddressReserve = nullptr;
halMemAddressFreeFunc DlHalApi::pHalMemAddressFree = nullptr;
halMemCreateFunc DlHalApi::pHalMemCreate = nullptr;
halMemReleaseFunc DlHalApi::pHalMemRelease = nullptr;
halMemMapFunc DlHalApi::pHalMemMap = nullptr;
halMemUnmapFunc DlHalApi::pHalMemUnmap = nullptr;
halMemExportFunc DlHalApi::pHalMemExport = nullptr;
halMemImportFunc DlHalApi::pHalMemImport = nullptr;
halMemShareHandleSetAttributeFunc DlHalApi::pHalMemShareHandleSetAttribute = nullptr;
halMemTransShareableHandleFunc DlHalApi::pHalMemTransShareableHandle = nullptr;
halMemGetAllocationGranularityFunc DlHalApi::pHalMemGetAllocationGranularity = nullptr;

Result DlHalApi::LoadHybmVmmLibrary(uint32_t gvaVersion)
{
    if (gvaVersion != HYBM_GVA_V4) {
        BM_LOG_INFO("driver version is too old, not support vmm.");
        return BM_OK;
    }

    DL_LOAD_SYM(pHalMemAddressReserve, halMemAddressReserveFunc, halHandle, "halMemAddressReserve");
    DL_LOAD_SYM(pHalMemAddressFree, halMemAddressFreeFunc, halHandle, "halMemAddressFree");
    DL_LOAD_SYM(pHalMemCreate, halMemCreateFunc, halHandle, "halMemCreate");
    DL_LOAD_SYM(pHalMemRelease, halMemReleaseFunc, halHandle, "halMemRelease");
    DL_LOAD_SYM(pHalMemMap, halMemMapFunc, halHandle, "halMemMap");
    DL_LOAD_SYM(pHalMemUnmap, halMemUnmapFunc, halHandle, "halMemUnmap");
    DL_LOAD_SYM(pHalMemExport, halMemExportFunc, halHandle, "halMemExportToShareableHandleV2");
    DL_LOAD_SYM(pHalMemImport, halMemImportFunc, halHandle, "halMemImportFromShareableHandleV2");
    DL_LOAD_SYM(pHalMemShareHandleSetAttribute, halMemShareHandleSetAttributeFunc,
                halHandle, "halMemShareHandleSetAttribute");
    DL_LOAD_SYM(pHalMemTransShareableHandle, halMemTransShareableHandleFunc, halHandle, "halMemTransShareableHandle");
    DL_LOAD_SYM(pHalMemGetAllocationGranularity, halMemGetAllocationGranularityFunc,
                halHandle, "halMemGetAllocationGranularity");

    return BM_OK;
}

Result DlHalApi::LoadHybmV1V2Library(uint32_t gvaVersion)
{
    if (gvaVersion == HYBM_GVA_V1 || gvaVersion == HYBM_GVA_V2) {
        if (gvaVersion == HYBM_GVA_V1) {
            DL_LOAD_SYM(pVirtDestroyHeapV1, halVirtDestroyHeapV1Func, halHandle, "devmm_virt_destroy_heap");
        } else {
            DL_LOAD_SYM(pSvmModuleAllocedSizeInc, halSvmModuleAllocedSizeIncFunc, halHandle,
                        "svm_module_alloced_size_inc");
        }
        DL_LOAD_SYM(pAssignNodeData, halAssignNodeDataFunc, halHandle, "devmm_assign_rbtree_node_data");
        DL_LOAD_SYM(pInsertIdleSizeTree, halInsertIdleSizeTreeFunc, halHandle, "devmm_rbtree_insert_idle_size_tree");
        DL_LOAD_SYM(pInsertIdleVaTree, halInsertIdleVaTreeFunc, halHandle, "devmm_rbtree_insert_idle_va_tree");
        DL_LOAD_SYM(pAllocRbtreeNode, halAllocRbtreeNodeFunc, halHandle, "devmm_alloc_rbtree_node");
        DL_LOAD_SYM(pEraseIdleVaTree, halEraseIdleVaTreeFunc, halHandle, "devmm_rbtree_erase_idle_va_tree");
        DL_LOAD_SYM(pEraseIdleSizeTree, halEraseIdleSizeTreeFunc, halHandle, "devmm_rbtree_erase_idle_size_tree");
        DL_LOAD_SYM(pGetAllocedNodeInRange, halGetAllocedNodeInRangeFunc, halHandle,
                    "devmm_rbtree_get_alloced_node_in_range");
        DL_LOAD_SYM(pGetIdleVaNodeInRange, halGetIdleVaNodeInRangeFunc, halHandle,
                    "devmm_rbtree_get_idle_va_node_in_range");
        DL_LOAD_SYM(pInsertAllocedTree, halInsertAllocedTreeFunc, halHandle, "devmm_rbtree_insert_alloced_tree");
        DL_LOAD_SYM(pFreeRbtreeNode, halFreeRbtreeNodeFunc, halHandle, "devmm_free_rbtree_node");
    } else { // HYBM_GVA_V3
        DL_LOAD_SYM(pSvmModuleAllocedSizeInc, halSvmModuleAllocedSizeIncFunc, halHandle, "svm_module_alloced_size_inc");
        DL_LOAD_SYM(pVirtDestroyHeapV2, halVirtDestroyHeapV2Func, halHandle, "devmm_virt_destroy_heap");
    }
    return BM_OK;
}

Result DlHalApi::LoadLibrary(uint32_t gvaVersion)
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }

    halHandle = dlopen(gAscendHalLibName, RTLD_NOW | RTLD_NODELETE);
    if (halHandle == nullptr) {
        BM_LOG_ERROR("Failed to open library [" << gAscendHalLibName
            << "], please source ascend-toolkit set_env.sh, or add ascend driver lib path into LD_LIBRARY_PATH,"
            << " error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    BM_ASSERT_RETURN(gvaVersion != HYBM_GVA_UNKNOWN, BM_NOT_INITIALIZED);
    /* load sym */
    DL_LOAD_SYM(pHalFd, int *, halHandle, "g_devmm_mem_dev");
    DL_LOAD_SYM(pSvmModuleAllocedSizeInc, halSvmModuleAllocedSizeIncFunc, halHandle, "svm_module_alloced_size_inc");
    DL_LOAD_SYM(pVirtAllocMemFromBase, halVirtAllocMemFromBaseFunc, halHandle, "devmm_virt_alloc_mem_from_base");
    DL_LOAD_SYM(pIoctlEnableHeap, halIoctlEnableHeapFunc, halHandle, "devmm_ioctl_enable_heap");
    DL_LOAD_SYM(pGetHeapListByType, halGetHeapListByTypeFunc, halHandle, "devmm_get_heap_list_by_type");
    DL_LOAD_SYM(pVirtSetHeapIdle, halVirtSetHeapIdleFunc, halHandle, "devmm_virt_set_heap_idle");
    DL_LOAD_SYM(pVirtGetHeapMgmt, halVirtGetHeapMgmtFunc, halHandle, "devmm_virt_get_heap_mgmt");
    DL_LOAD_SYM(pIoctlFreePages, halIoctlFreePagesFunc, halHandle, "devmm_ioctl_free_pages");
    DL_LOAD_SYM(pVaToHeapIdx, halVaToHeapIdxFunc, halHandle, "devmm_va_to_heap_idx");
    DL_LOAD_SYM(pVirtGetHeapFromQueue, halVirtGetHeapFromQueueFunc, halHandle, "devmm_virt_get_heap_from_queue");
    DL_LOAD_SYM(pVirtNormalHeapUpdateInfo, halVirtNormalHeapUpdateInfoFunc, halHandle,
                "devmm_virt_normal_heap_update_info");
    DL_LOAD_SYM(pVaToHeap, halVaToHeapFunc, halHandle, "devmm_va_to_heap");

    auto ret = LoadHybmV1V2Library(gvaVersion) | LoadHybmVmmLibrary(gvaVersion);
    if (ret != 0) {
        return ret;
    }

    DL_LOAD_SYM(pHalSqTaskSend, halSqTaskSendFunc, halHandle, "halSqTaskSend");
    DL_LOAD_SYM(pHalCqReportRecv, halCqReportRecvFunc, halHandle, "halCqReportRecv");
    DL_LOAD_SYM(pHalSqCqAllocate, halSqCqAllocateFunc, halHandle, "halSqCqAllocate");
    DL_LOAD_SYM(pHalSqCqFree, halSqCqFreeFunc, halHandle, "halSqCqFree");
    DL_LOAD_SYM(pHalResourceIdAlloc, halResourceIdAllocFunc, halHandle, "halResourceIdAlloc");
    DL_LOAD_SYM(pHalResourceIdFree, halResourceIdFreeFunc, halHandle, "halResourceIdFree");
    DL_LOAD_SYM(pHalResourceConfig, halResourceConfigFunc, halHandle, "halResourceConfig");
    DL_LOAD_SYM(pHalSqCqQuery, halSqCqQueryFunc, halHandle, "halSqCqQuery");
    DL_LOAD_SYM(pHalHostRegister, halHostRegisterFunc, halHandle, "halHostRegister");
    DL_LOAD_SYM(pHalHostUnregisterEx, halHostUnregisterExFunc, halHandle, "halHostUnregisterEx");
    DL_LOAD_SYM(pDrvNotifyIdAddrOffset, drvNotifyIdAddrOffsetFunc, halHandle, "drvNotifyIdAddrOffset");

    gLoaded = true;
    return BM_OK;
}

void DlHalApi::CleanupHalApi()
{
    pHalFd = nullptr;
    pSvmModuleAllocedSizeInc = nullptr;
    pVirtAllocMemFromBase = nullptr;
    pIoctlEnableHeap = nullptr;
    pGetHeapListByType = nullptr;
    pVirtSetHeapIdle = nullptr;
    pVirtDestroyHeapV1 = nullptr;
    pVirtDestroyHeapV2 = nullptr;
    pVirtGetHeapMgmt = nullptr;
    pIoctlFreePages = nullptr;
    pVaToHeapIdx = nullptr;
    pVirtGetHeapFromQueue = nullptr;
    pVirtNormalHeapUpdateInfo = nullptr;
    pVaToHeap = nullptr;

    pAssignNodeData = nullptr;
    pInsertIdleSizeTree = nullptr;
    pInsertIdleVaTree = nullptr;
    pAllocRbtreeNode = nullptr;
    pEraseIdleVaTree = nullptr;
    pEraseIdleSizeTree = nullptr;
    pGetAllocedNodeInRange = nullptr;
    pGetIdleVaNodeInRange = nullptr;
    pInsertAllocedTree = nullptr;
    pFreeRbtreeNode = nullptr;

    pHalSqTaskSend = nullptr;
    pHalCqReportRecv = nullptr;
    pHalSqCqAllocate = nullptr;
    pHalSqCqFree = nullptr;
    pHalResourceIdAlloc = nullptr;
    pHalResourceIdFree = nullptr;
    pHalResourceConfig = nullptr;
    pHalSqCqQuery = nullptr;
    pHalHostRegister = nullptr;
    pHalHostUnregisterEx = nullptr;

    pHalMemAddressReserve = nullptr;
    pHalMemAddressFree = nullptr;
    pHalMemCreate = nullptr;
    pHalMemRelease = nullptr;
    pHalMemMap = nullptr;
    pHalMemUnmap = nullptr;
    pHalMemExport = nullptr;
    pHalMemImport = nullptr;
    pHalMemGetAllocationGranularity = nullptr;
}

void DlHalApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }

    CleanupHalApi();

    if (halHandle != nullptr) {
        dlclose(halHandle);
        halHandle = nullptr;
    }
    gLoaded = false;
}
} // namespace mf
} // namespace ock
