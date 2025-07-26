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
halDevmmVirtAllocMemFromBaseFunc DlHalApi::pDevmmVirtAllocMemFromBase = nullptr;
halDevmmIoctlEnableHeapFunc DlHalApi::pDevmmIoctlEnableHeap = nullptr;
halDevmmGetHeapListByTypeFunc DlHalApi::pDevmmGetHeapListByType = nullptr;
halDevmmVirtSetHeapIdleFunc DlHalApi::pDevmmVirtSetHeapIdle = nullptr;
halDevmmVirtDestroyHeapFunc DlHalApi::pDevmmVirtDestroyHeap = nullptr;
halDevmmVirtGetHeapMgmtFunc DlHalApi::pDevmmVirtGetHeapMgmt = nullptr;
halDevmmIoctlFreePagesFunc DlHalApi::pDevmmIoctlFreePages = nullptr;
halDevmmVaToHeapIdxFunc DlHalApi::pDevmmVaToHeapIdx = nullptr;
halDevmmVirtGetHeapFromQueueFunc DlHalApi::pDevmmVirtGetHeapFromQueue = nullptr;
halDevmmVirtNormalHeapUpdateInfoFunc DlHalApi::pDevmmVirtNormalHeapUpdateInfo = nullptr;
halDevmmVaToHeapFunc DlHalApi::pDevmmVaToHeap = nullptr;
int *DlHalApi::pHalDevmmFd = nullptr;

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
    DL_LOAD_SYM(pHalDevmmFd, int *, halHandle, "g_devmm_mem_dev");
    DL_LOAD_SYM(pSvmModuleAllocedSizeInc, halSvmModuleAllocedSizeIncFunc, halHandle, "svm_module_alloced_size_inc");
    DL_LOAD_SYM(pDevmmVirtAllocMemFromBase, halDevmmVirtAllocMemFromBaseFunc, halHandle,
                "devmm_virt_alloc_mem_from_base");
    DL_LOAD_SYM(pDevmmIoctlEnableHeap, halDevmmIoctlEnableHeapFunc, halHandle, "devmm_ioctl_enable_heap");
    DL_LOAD_SYM(pDevmmGetHeapListByType, halDevmmGetHeapListByTypeFunc, halHandle, "devmm_get_heap_list_by_type");
    DL_LOAD_SYM(pDevmmVirtSetHeapIdle, halDevmmVirtSetHeapIdleFunc, halHandle, "devmm_virt_set_heap_idle");
    DL_LOAD_SYM(pDevmmVirtDestroyHeap, halDevmmVirtDestroyHeapFunc, halHandle, "devmm_virt_destroy_heap");
    DL_LOAD_SYM(pDevmmVirtGetHeapMgmt, halDevmmVirtGetHeapMgmtFunc, halHandle, "devmm_virt_get_heap_mgmt");
    DL_LOAD_SYM(pDevmmIoctlFreePages, halDevmmIoctlFreePagesFunc, halHandle, "devmm_ioctl_free_pages");
    DL_LOAD_SYM(pDevmmVaToHeapIdx, halDevmmVaToHeapIdxFunc, halHandle, "devmm_va_to_heap_idx");
    DL_LOAD_SYM(pDevmmVirtGetHeapFromQueue, halDevmmVirtGetHeapFromQueueFunc, halHandle,
                "devmm_virt_get_heap_from_queue");
    DL_LOAD_SYM(pDevmmVirtNormalHeapUpdateInfo, halDevmmVirtNormalHeapUpdateInfoFunc, halHandle,
                "devmm_virt_normal_heap_update_info");
    DL_LOAD_SYM(pDevmmVaToHeap, halDevmmVaToHeapFunc, halHandle, "devmm_va_to_heap");

    gLoaded = true;
    return BM_OK;
}

void DlHalApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }

    pHalDevmmFd = nullptr;
    pSvmModuleAllocedSizeInc = nullptr;
    pDevmmVirtAllocMemFromBase = nullptr;
    pDevmmIoctlEnableHeap = nullptr;
    pDevmmGetHeapListByType = nullptr;
    pDevmmVirtSetHeapIdle = nullptr;
    pDevmmVirtDestroyHeap = nullptr;
    pDevmmVirtGetHeapMgmt = nullptr;
    pDevmmIoctlFreePages = nullptr;
    pDevmmVaToHeapIdx = nullptr;
    pDevmmVirtGetHeapFromQueue = nullptr;
    pDevmmVirtNormalHeapUpdateInfo = nullptr;
    pDevmmVaToHeap = nullptr;

    if (halHandle != nullptr) {
        dlclose(halHandle);
        halHandle = nullptr;
    }
    gLoaded = false;
}
}
}