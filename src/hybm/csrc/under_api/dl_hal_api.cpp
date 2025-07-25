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

Result DlHalApi::LoadLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }

    halHandle = dlopen(gAscendHalLibName, RTLD_NOW);
    if (halHandle == nullptr) {
        BM_LOG_ERROR("Failed to open library [" << gAscendHalLibName << "], please source ascend-toolkit set_env.sh,"
                     << " or add ascend driver lib path into LD_LIBRARY_PATH," << " error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    BM_ASSERT_RETURN(HybmGetGvaVersion() != HYBM_GVA_UNKNOWN, BM_NOT_INITIALIZED);
    /* load sym */
    DL_LOAD_SYM(pHalFd, int *, halHandle, "g_devmm_mem_dev");
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

    if (HybmGetGvaVersion() == HYBM_GVA_V1) {
        DL_LOAD_SYM(pVirtDestroyHeapV1, halVirtDestroyHeapV1Func, halHandle, "devmm_virt_destroy_heap");
        DL_LOAD_SYM(pAssignNodeData, halAssignNodeDataFunc, halHandle, "devmm_assign_rbtree_node_data");
        DL_LOAD_SYM(pInsertIdleSizeTree, halInsertIdleSizeTreeFunc, halHandle,
                    "devmm_rbtree_insert_idle_size_tree");
        DL_LOAD_SYM(pInsertIdleVaTree, halInsertIdleVaTreeFunc, halHandle,
                    "devmm_rbtree_insert_idle_va_tree");
        DL_LOAD_SYM(pAllocRbtreeNode, halAllocRbtreeNodeFunc, halHandle, "devmm_alloc_rbtree_node");
        DL_LOAD_SYM(pEraseIdleVaTree, halEraseIdleVaTreeFunc, halHandle, "devmm_rbtree_erase_idle_va_tree");
        DL_LOAD_SYM(pEraseIdleSizeTree, halEraseIdleSizeTreeFunc, halHandle,
                    "devmm_rbtree_erase_idle_size_tree");
        DL_LOAD_SYM(pGetAllocedNodeInRange, halGetAllocedNodeInRangeFunc, halHandle,
                    "devmm_rbtree_get_alloced_node_in_range");
        DL_LOAD_SYM(pGetIdleVaNodeInRange, halGetIdleVaNodeInRangeFunc, halHandle,
                    "devmm_rbtree_get_idle_va_node_in_range");
        DL_LOAD_SYM(pInsertAllocedTree, halInsertAllocedTreeFunc, halHandle,
                    "devmm_rbtree_insert_alloced_tree");
        DL_LOAD_SYM(pFreeRbtreeNode, halFreeRbtreeNodeFunc, halHandle, "devmm_free_rbtree_node");
    } else { // HYBM_GVA_V2
        DL_LOAD_SYM(pSvmModuleAllocedSizeInc, halSvmModuleAllocedSizeIncFunc, halHandle, "svm_module_alloced_size_inc");
        DL_LOAD_SYM(pVirtDestroyHeapV2, halVirtDestroyHeapV2Func, halHandle, "devmm_virt_destroy_heap");
    }

    gLoaded = true;
    return BM_OK;
}

void DlHalApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }

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

    if (halHandle != nullptr) {
        dlclose(halHandle);
        halHandle = nullptr;
    }

    gLoaded = false;
}
}
}