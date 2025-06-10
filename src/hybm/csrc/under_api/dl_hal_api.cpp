/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <dlfcn.h>
#include <mutex>
#include "dl_hal_api.h"

namespace ock {
namespace mf {
bool RuntimeHalApi::gLoaded = false;
std::mutex RuntimeHalApi::gMutex;
void *RuntimeHalApi::halHandle;

const char *RuntimeHalApi::gAscendHalLibName = "libascend_hal.so";

halSvmModuleAllocedSizeIncFunc RuntimeHalApi::pSvmModuleAllocedSizeInc = nullptr;
halDevmmVirtAllocMemFromBaseFunc RuntimeHalApi::pDevmmVirtAllocMemFromBase = nullptr;
halDevmmIoctlEnableHeapFunc RuntimeHalApi::pDevmmIoctlEnableHeap = nullptr;
halDevmmGetHeapListByTypeFunc RuntimeHalApi::pDevmmGetHeapListByType = nullptr;
halDevmmVirtSetHeapIdleFunc RuntimeHalApi::pDevmmVirtSetHeapIdle = nullptr;
halDevmmVirtDestroyHeapFunc RuntimeHalApi::pDevmmVirtDestroyHeap = nullptr;
halDevmmVirtGetHeapMgmtFunc RuntimeHalApi::pDevmmVirtGetHeapMgmt = nullptr;
halDevmmIoctlFreePagesFunc RuntimeHalApi::pDevmmIoctlFreePages = nullptr;
halDevmmVaToHeapIdxFunc RuntimeHalApi::pDevmmVaToHeapIdx = nullptr;
halDevmmVirtGetHeapFromQueueFunc RuntimeHalApi::pDevmmVirtGetHeapFromQueue = nullptr;
halDevmmVirtNormalHeapUpdateInfoFunc RuntimeHalApi::pDevmmVirtNormalHeapUpdateInfo = nullptr;
halDevmmVaToHeapFunc RuntimeHalApi::pDevmmVaToHeap = nullptr;
int *RuntimeHalApi::pHalDevmmFd = nullptr;

halGvaReserveMemoryFun RuntimeHalApi::pHalGvaReserveMemory = nullptr;
halGvaUnreserveMemoryFun RuntimeHalApi::pHalGvaUnreserveMemory = nullptr;
halGvaAllocFun RuntimeHalApi::pHalGvaAlloc = nullptr;
halGvaFreeFun RuntimeHalApi::pHalGvaFree = nullptr;
halGvaOpenFun RuntimeHalApi::pHalGvaOpen = nullptr;
halGvaCloseFun RuntimeHalApi::pHalGvaClose = nullptr;

Result RuntimeHalApi::LoadLibrary()
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