/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBM_CORE_DL_HAL_API_H
#define MF_HYBM_CORE_DL_HAL_API_H

#include "hybm_common_include.h"

namespace ock {
namespace mf {
using halSvmModuleAllocedSizeIncFunc = void (*)(void *, uint32_t, uint32_t, uint64_t);
using halVirtAllocMemFromBaseFunc = uint64_t (*)(void *, size_t, uint32_t, uint64_t);
using halIoctlEnableHeapFunc = int32_t (*)(uint32_t, uint32_t, uint32_t, uint64_t, uint32_t);
using halGetHeapListByTypeFunc = int32_t (*)(void *, void *, void *);
using halVirtSetHeapIdleFunc = int32_t (*)(void *, void *);
using halVirtDestroyHeapV1Func = int32_t (*)(void *, void *);
using halVirtDestroyHeapV2Func = int32_t (*)(void *, void *, bool);
using halVirtGetHeapMgmtFunc = void *(*)(void);
using halIoctlFreePagesFunc = int32_t (*)(uint64_t);
using halVaToHeapIdxFunc = uint32_t (*)(const void *, uint64_t);
using halVirtGetHeapFromQueueFunc = void *(*)(void *, uint32_t, size_t);
using halVirtNormalHeapUpdateInfoFunc = void (*)(void *, void *, void *, void *, uint64_t);
using halVaToHeapFunc = void *(*)(uint64_t);

using halAssignNodeDataFunc = void (*)(uint64_t, uint64_t, uint64_t, uint32_t, void *RbtreeNode);
using halInsertIdleSizeTreeFunc = int32_t (*)(void *RbtreeNode, void *rbtree_queue);
using halInsertIdleVaTreeFunc = int32_t (*)(void *RbtreeNode, void *rbtree_queue);
using halAllocRbtreeNodeFunc = void *(*)(void *rbtree_queue);
using halEraseIdleVaTreeFunc = int32_t (*)(void *RbtreeNode, void *rbtree_queue);
using halEraseIdleSizeTreeFunc = int32_t (*)(void *RbtreeNode, void *rbtree_queue);
using halGetAllocedNodeInRangeFunc = void *(*)(uint64_t va, void *rbtree_queue);
using halGetIdleVaNodeInRangeFunc = void *(*)(uint64_t va, void *rbtree_queue);
using halInsertAllocedTreeFunc = int32_t (*)(void *RbtreeNode, void *rbtree_queue);
using halFreeRbtreeNodeFunc = void (*)(void *RbNode, void *rbtree_queue);

class DlHalApi {
public:
    static Result LoadLibrary();
    static void CleanupLibrary();

    static inline void HalSvmModuleAllocedSizeInc(void *type, uint32_t devid, uint32_t moduleId, uint64_t size)
    {
        if (pSvmModuleAllocedSizeInc == nullptr) {
            return;
        }
        return pSvmModuleAllocedSizeInc(type, devid, moduleId, size);
    }

    static inline uint64_t HalVirtAllocMemFromBase(void *mgmt, size_t size, uint32_t advise, uint64_t allocPtr)
    {
        if (pVirtAllocMemFromBase == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pVirtAllocMemFromBase(mgmt, size, advise, allocPtr);
    }

    static inline Result HalIoctlEnableHeap(uint32_t heapIdx, uint32_t heapType, uint32_t subType,
                                                 uint64_t heapSize, uint32_t heapListType)
    {
        if (pIoctlEnableHeap == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pIoctlEnableHeap(heapIdx, heapType, subType, heapSize, heapListType);
    }

    static inline Result HalGetHeapListByType(void *mgmt, void *heapType, void *heapList)
    {
        if (pGetHeapListByType == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pGetHeapListByType(mgmt, heapType, heapList);
    }

    static inline Result HalVirtSetHeapIdle(void *mgmt, void *heap)
    {
        if (pVirtSetHeapIdle == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pVirtSetHeapIdle(mgmt, heap);
    }

    static inline Result HalVirtDestroyHeapV1(void *mgmt, void *heap)
    {
        if (pVirtDestroyHeapV1 == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pVirtDestroyHeapV1(mgmt, heap);
    }

    static inline Result HalVirtDestroyHeapV2(void *mgmt, void *heap, bool needDec)
    {
        if (pVirtDestroyHeapV2 == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pVirtDestroyHeapV2(mgmt, heap, needDec);
    }

    static inline void *HalVirtGetHeapMgmt(void)
    {
        if (pVirtGetHeapMgmt == nullptr) {
            return nullptr;
        }
        return pVirtGetHeapMgmt();
    }

    static inline Result HalIoctlFreePages(uint64_t ptr)
    {
        if (pIoctlFreePages == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pIoctlFreePages(ptr);
    }

    static inline uint32_t HalVaToHeapIdx(void *mgmt, uint64_t va)
    {
        if (pVaToHeapIdx == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pVaToHeapIdx(mgmt, va);
    }

    static inline void *HalVirtGetHeapFromQueue(void *mgmt, uint32_t heapIdx, size_t heapSize)
    {
        if (pVirtGetHeapFromQueue == nullptr) {
            return nullptr;
        }
        return pVirtGetHeapFromQueue(mgmt, heapIdx, heapSize);
    }

    static inline void HalVirtNormalHeapUpdateInfo(void *mgmt, void *heap, void *type, void *ops, uint64_t size)
    {
        if (pVirtNormalHeapUpdateInfo == nullptr) {
            return;
        }
        return pVirtNormalHeapUpdateInfo(mgmt, heap, type, ops, size);
    }

    static inline void *HalVaToHeap(uint64_t ptr)
    {
        if (pVaToHeap == nullptr) {
            return nullptr;
        }
        return pVaToHeap(ptr);
    }

    static inline int32_t GetFd(void)
    {
        return *pHalFd;
    }

    static inline void HalAssignNodeData(uint64_t va, uint64_t size, uint64_t total, uint32_t flag, void *RbtreeNode)
    {
        return pAssignNodeData(va, size, total, flag, RbtreeNode);
    }

    static inline int32_t HalInsertIdleSizeTree(void *RbtreeNode, void *rbtree_queue)
    {
        return pInsertIdleSizeTree(RbtreeNode, rbtree_queue);
    }

    static inline int32_t HalInsertIdleVaTree(void *RbtreeNode, void *rbtree_queue)
    {
        return pInsertIdleVaTree(RbtreeNode, rbtree_queue);
    }

    static inline void *HalAllocRbtreeNode(void *rbtree_queue)
    {
        return pAllocRbtreeNode(rbtree_queue);
    }

    static inline int32_t HalEraseIdleVaTree(void *RbtreeNode, void *rbtree_queue)
    {
        return pEraseIdleVaTree(RbtreeNode, rbtree_queue);
    }

    static inline int32_t HalEraseIdleSizeTree(void *RbtreeNode, void *rbtree_queue)
    {
        return pEraseIdleSizeTree(RbtreeNode, rbtree_queue);
    }

    static inline void *HalGetAllocedNodeInRange(uint64_t va, void *rbtree_queue)
    {
        return pGetAllocedNodeInRange(va, rbtree_queue);
    }

    static inline void *HalGetIdleVaNodeInRange(uint64_t va, void *rbtree_queue)
    {
        return pGetIdleVaNodeInRange(va, rbtree_queue);
    }

    static inline int32_t HalInsertAllocedTree(void *RbtreeNode, void *rbtree_queue)
    {
        return pInsertAllocedTree(RbtreeNode, rbtree_queue);
    }

    static inline void HalFreeRbtreeNode(void *RbtreeNode, void *rbtree_queue)
    {
        return pFreeRbtreeNode(RbtreeNode, rbtree_queue);
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *halHandle;
    static const char *gAscendHalLibName;

    static halSvmModuleAllocedSizeIncFunc pSvmModuleAllocedSizeInc;
    static halVirtAllocMemFromBaseFunc pVirtAllocMemFromBase;
    static halIoctlEnableHeapFunc pIoctlEnableHeap;
    static halGetHeapListByTypeFunc pGetHeapListByType;
    static halVirtSetHeapIdleFunc pVirtSetHeapIdle;
    static halVirtDestroyHeapV1Func pVirtDestroyHeapV1;
    static halVirtDestroyHeapV2Func pVirtDestroyHeapV2;
    static halVirtGetHeapMgmtFunc pVirtGetHeapMgmt;
    static halIoctlFreePagesFunc pIoctlFreePages;
    static halVaToHeapIdxFunc pVaToHeapIdx;
    static halVirtGetHeapFromQueueFunc pVirtGetHeapFromQueue;
    static halVirtNormalHeapUpdateInfoFunc pVirtNormalHeapUpdateInfo;
    static halVaToHeapFunc pVaToHeap;
    static int *pHalFd;

    static halAssignNodeDataFunc pAssignNodeData;
    static halInsertIdleSizeTreeFunc pInsertIdleSizeTree;
    static halInsertIdleVaTreeFunc pInsertIdleVaTree;
    static halAllocRbtreeNodeFunc pAllocRbtreeNode;
    static halEraseIdleVaTreeFunc pEraseIdleVaTree;
    static halEraseIdleSizeTreeFunc pEraseIdleSizeTree;
    static halGetAllocedNodeInRangeFunc pGetAllocedNodeInRange;
    static halGetIdleVaNodeInRangeFunc pGetIdleVaNodeInRange;
    static halInsertAllocedTreeFunc pInsertAllocedTree;
    static halFreeRbtreeNodeFunc pFreeRbtreeNode;
};

}
}

#endif  // MF_HYBM_CORE_DL_HAL_API_H
