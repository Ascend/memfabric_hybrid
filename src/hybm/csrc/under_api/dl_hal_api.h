/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBM_CORE_DL_HAL_API_H
#define MF_HYBM_CORE_DL_HAL_API_H

#include "hybm_common_include.h"

namespace ock {
namespace mf {
using halSvmModuleAllocedSizeIncFunc = void (*)(void *, uint32_t, uint32_t, uint64_t);
using halDevmmVirtAllocMemFromBaseFunc = uint64_t (*)(void *, size_t, uint32_t, uint64_t);
using halDevmmIoctlEnableHeapFunc = int32_t (*)(uint32_t, uint32_t, uint32_t, uint64_t, uint32_t);
using halDevmmGetHeapListByTypeFunc = int32_t (*)(void *, void *, void *);
using halDevmmVirtSetHeapIdleFunc = int32_t (*)(void *, void *);
using halDevmmVirtDestroyHeapFunc = int32_t (*)(void *, void *, bool);
using halDevmmVirtGetHeapMgmtFunc = void *(*)(void);
using halDevmmIoctlFreePagesFunc = int32_t (*)(uint64_t);
using halDevmmVaToHeapIdxFunc = uint32_t (*)(const void *, uint64_t);
using halDevmmVirtGetHeapFromQueueFunc = void *(*)(void *, uint32_t, size_t);
using halDevmmVirtNormalHeapUpdateInfoFunc = void (*)(void *, void *, void *, void *, uint64_t);
using halDevmmVaToHeapFunc = void *(*)(uint64_t);

using halGvaReserveMemoryFun = int32_t (*)(void **, size_t, int32_t, uint64_t);
using halGvaUnreserveMemoryFun = int32_t (*)(void);
using halGvaAllocFun = int32_t (*)(void *, size_t, uint64_t);
using halGvaFreeFun = int32_t (*)(void *, size_t);
using halGvaOpenFun = int32_t (*)(void *, const char *, size_t, uint64_t);
using halGvaCloseFun = int32_t (*)(void *, uint64_t);


class RuntimeHalApi {
public:
    static Result LoadLibrary();

    static inline void HalSvmModuleAllocedSizeInc(void *type, uint32_t devid, uint32_t moduleId, uint64_t size)
    {
        return pSvmModuleAllocedSizeInc(type, devid, moduleId, size);
    }

    static inline uint64_t HalDevmmVirtAllocMemFromBase(void *mgmt, size_t size, uint32_t advise, uint64_t allocPtr)
    {
        return pDevmmVirtAllocMemFromBase(mgmt, size, advise, allocPtr);
    }

    static inline Result HalDevmmIoctlEnableHeap(uint32_t heapIdx, uint32_t heapType, uint32_t subType,
                                                 uint64_t heapSize, uint32_t heapListType)
    {
        return pDevmmIoctlEnableHeap(heapIdx, heapType, subType, heapSize, heapListType);
    }

    static inline Result HalDevmmGetHeapListByType(void *mgmt, void *heapType, void *heapList)
    {
        return pDevmmGetHeapListByType(mgmt, heapType, heapList);
    }

    static inline Result HalDevmmVirtSetHeapIdle(void *mgmt, void *heap)
    {
        return pDevmmVirtSetHeapIdle(mgmt, heap);
    }

    static inline Result HalDevmmVirtDestroyHeap(void *mgmt, void *heap, bool needDec)
    {
        return pDevmmVirtDestroyHeap(mgmt, heap, needDec);
    }

    static inline void *HalDevmmVirtGetHeapMgmt(void)
    {
        return pDevmmVirtGetHeapMgmt();
    }

    static inline Result HalDevmmIoctlFreePages(uint64_t ptr)
    {
        return pDevmmIoctlFreePages(ptr);
    }

    static inline uint32_t HalDevmmVaToHeapIdx(void *mgmt, uint64_t va)
    {
        return pDevmmVaToHeapIdx(mgmt, va);
    }

    static inline void *HalDevmmVirtGetHeapFromQueue(void *mgmt, uint32_t heapIdx, size_t heapSize)
    {
        return pDevmmVirtGetHeapFromQueue(mgmt, heapIdx, heapSize);
    }

    static inline void HalDevmmVirtNormalHeapUpdateInfo(void *mgmt, void *heap, void *type, void *ops, uint64_t size)
    {
        return pDevmmVirtNormalHeapUpdateInfo(mgmt, heap, type, ops, size);
    }

    static inline void *HalDevmmVaToHeap(uint64_t ptr)
    {
        return pDevmmVaToHeap(ptr);
    }

    static inline int32_t GetDevmmFd(void)
    {
        return *pHalDevmmFd;
    }

    static inline Result HalGvaReserveMemory(void **address, size_t size, int32_t deviceId, uint64_t flags)
    {
        return pHalGvaReserveMemory(address, size, deviceId, flags);
    }

    static inline Result HalGvaUnreserveMemory()
    {
        return pHalGvaUnreserveMemory();
    }

    static inline Result HalGvaAlloc(void *address, size_t size, uint64_t flags)
    {
        return pHalGvaAlloc(address, size, flags);
    }

    static inline Result HalGvaFree(void *address, size_t size)
    {
        return pHalGvaFree(address, size);
    }

    static inline Result HalGvaOpen(void *address, const char *name, size_t size, uint64_t flags)
    {
        return pHalGvaOpen(address, name, size, flags);
    }

    static inline Result HalGvaClose(void *address, uint64_t flags)
    {
        return pHalGvaClose(address, flags);
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *halHandle;
    static const char *gAscendHalLibName;

    static halSvmModuleAllocedSizeIncFunc pSvmModuleAllocedSizeInc;
    static halDevmmVirtAllocMemFromBaseFunc pDevmmVirtAllocMemFromBase;
    static halDevmmIoctlEnableHeapFunc pDevmmIoctlEnableHeap;
    static halDevmmGetHeapListByTypeFunc pDevmmGetHeapListByType;
    static halDevmmVirtSetHeapIdleFunc pDevmmVirtSetHeapIdle;
    static halDevmmVirtDestroyHeapFunc pDevmmVirtDestroyHeap;
    static halDevmmVirtGetHeapMgmtFunc pDevmmVirtGetHeapMgmt;
    static halDevmmIoctlFreePagesFunc pDevmmIoctlFreePages;
    static halDevmmVaToHeapIdxFunc pDevmmVaToHeapIdx;
    static halDevmmVirtGetHeapFromQueueFunc pDevmmVirtGetHeapFromQueue;
    static halDevmmVirtNormalHeapUpdateInfoFunc pDevmmVirtNormalHeapUpdateInfo;
    static halDevmmVaToHeapFunc pDevmmVaToHeap;
    static int *pHalDevmmFd;

    static halGvaReserveMemoryFun pHalGvaReserveMemory;
    static halGvaUnreserveMemoryFun pHalGvaUnreserveMemory;
    static halGvaAllocFun pHalGvaAlloc;
    static halGvaFreeFun pHalGvaFree;
    static halGvaOpenFun pHalGvaOpen;
    static halGvaCloseFun pHalGvaClose;
};

}
}

#endif  // MF_HYBM_CORE_DL_HAL_API_H
