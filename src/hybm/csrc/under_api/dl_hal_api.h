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

#ifndef MF_HYBM_CORE_DL_HAL_API_H
#define MF_HYBM_CORE_DL_HAL_API_H

#include <mutex>

#include "dl_hal_api_def.h"
#include "hybm_types.h"

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

using halSqTaskSendFunc = int (*)(uint32_t, halTaskSendInfo *);
using halCqReportRecvFunc = int (*)(uint32_t, halReportRecvInfo *);
using halSqCqAllocateFunc = int (*)(uint32_t, halSqCqInputInfo *, halSqCqOutputInfo *);
using halSqCqFreeFunc = int (*)(uint32_t, halSqCqFreeInfo *);
using halResourceIdAllocFunc = int (*)(uint32_t, struct halResourceIdInputInfo *, struct halResourceIdOutputInfo *);
using halResourceIdFreeFunc = int (*)(uint32_t, struct halResourceIdInputInfo *);
using halGetSsidFunc = int (*)(uint32_t, uint32_t *);
using halResourceConfigFunc = int (*)(uint32_t, struct halResourceIdInputInfo *, struct halResourceConfigInfo *);
using halSqCqQueryFunc = int (*)(uint32_t devId, struct halSqCqQueryInfo *info);
using halHostRegisterFunc = int (*)(void *, uint64_t, uint32_t, uint32_t, void **);
using halHostUnregisterExFunc = int (*)(void *, uint32_t, uint32_t);
using drvNotifyIdAddrOffsetFunc = int (*)(uint32_t, struct drvNotifyInfo *);

using halMemAddressReserveFunc = int (*)(void **, size_t, size_t, void *, uint64_t);
using halMemAddressFreeFunc = int (*)(void *);
using halMemCreateFunc = int (*)(drv_mem_handle_t **, size_t, const struct drv_mem_prop *, uint64_t);
using halMemReleaseFunc = int (*)(drv_mem_handle_t *);
using halMemMapFunc = int (*)(void *, size_t, size_t, drv_mem_handle_t *, uint64_t);
using halMemUnmapFunc = int (*)(void *);
using halMemExportFunc = int (*)(drv_mem_handle_t *, drv_mem_handle_type, uint64_t, struct MemShareHandle *);
using halMemImportFunc = int (*)(drv_mem_handle_type, struct MemShareHandle *, uint32_t, drv_mem_handle_t **);
using halMemShareHandleSetAttributeFunc = int (*)(uint64_t, enum ShareHandleAttrType, struct ShareHandleAttr);
using halMemTransShareableHandleFunc = int (*)(drv_mem_handle_type, struct MemShareHandle *, uint32_t *, uint64_t *);
using halMemGetAllocationGranularityFunc = int (*)(const struct drv_mem_prop *, drv_mem_granularity_options, size_t *);

class DlHalApi {
public:
    static Result LoadLibrary(uint32_t gvaVersion);
    static void CleanupLibrary();
    static void CleanupHalApi();

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
        if (pAssignNodeData == nullptr) {
            return;
        }
        return pAssignNodeData(va, size, total, flag, RbtreeNode);
    }

    static inline int32_t HalInsertIdleSizeTree(void *RbtreeNode, void *rbtree_queue)
    {
        if (pInsertIdleSizeTree == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pInsertIdleSizeTree(RbtreeNode, rbtree_queue);
    }

    static inline int32_t HalInsertIdleVaTree(void *RbtreeNode, void *rbtree_queue)
    {
        if (pInsertIdleVaTree == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pInsertIdleVaTree(RbtreeNode, rbtree_queue);
    }

    static inline void *HalAllocRbtreeNode(void *rbtree_queue)
    {
        if (pAllocRbtreeNode == nullptr) {
            return nullptr;
        }
        return pAllocRbtreeNode(rbtree_queue);
    }

    static inline int32_t HalEraseIdleVaTree(void *RbtreeNode, void *rbtree_queue)
    {
        if (pEraseIdleVaTree == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pEraseIdleVaTree(RbtreeNode, rbtree_queue);
    }

    static inline int32_t HalEraseIdleSizeTree(void *RbtreeNode, void *rbtree_queue)
    {
        if (pEraseIdleSizeTree == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pEraseIdleSizeTree(RbtreeNode, rbtree_queue);
    }

    static inline void *HalGetAllocedNodeInRange(uint64_t va, void *rbtree_queue)
    {
        if (pGetAllocedNodeInRange == nullptr) {
            return nullptr;
        }
        return pGetAllocedNodeInRange(va, rbtree_queue);
    }

    static inline void *HalGetIdleVaNodeInRange(uint64_t va, void *rbtree_queue)
    {
        if (pGetIdleVaNodeInRange == nullptr) {
            return nullptr;
        }
        return pGetIdleVaNodeInRange(va, rbtree_queue);
    }

    static inline int32_t HalInsertAllocedTree(void *RbtreeNode, void *rbtree_queue)
    {
        if (pInsertAllocedTree == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pInsertAllocedTree(RbtreeNode, rbtree_queue);
    }

    static inline void HalFreeRbtreeNode(void *RbtreeNode, void *rbtree_queue)
    {
        if (pFreeRbtreeNode == nullptr) {
            return;
        }
        return pFreeRbtreeNode(RbtreeNode, rbtree_queue);
    }

    static inline int HalSqTaskSend(uint32_t devId, struct halTaskSendInfo *info)
    {
        if (pHalSqTaskSend == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalSqTaskSend(devId, info);
    }

    static inline int HalCqReportRecv(uint32_t devId, struct halReportRecvInfo *info)
    {
        if (pHalCqReportRecv == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalCqReportRecv(devId, info);
    }

    static inline int HalSqCqAllocate(uint32_t devId, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out)
    {
        if (pHalSqCqAllocate == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalSqCqAllocate(devId, in, out);
    }

    static inline int HalSqCqFree(uint32_t devId, struct halSqCqFreeInfo *info)
    {
        if (pHalSqCqFree == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalSqCqFree(devId, info);
    }

    static inline int HalResourceIdAlloc(uint32_t devId, struct halResourceIdInputInfo *in,
                                         struct halResourceIdOutputInfo *out)
    {
        if (pHalResourceIdAlloc == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalResourceIdAlloc(devId, in, out);
    }

    static inline int HalResourceIdFree(uint32_t devId, struct halResourceIdInputInfo *in)
    {
        if (pHalResourceIdFree == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalResourceIdFree(devId, in);
    }

    static inline int HalGetSsid(uint32_t devId, uint32_t *ssid)
    {
        if (pHalGetSsid == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalGetSsid(devId, ssid);
    }

    static inline int HalResourceConfig(uint32_t devId, struct halResourceIdInputInfo *in,
                                        struct halResourceConfigInfo *para)
    {
        if (pHalResourceConfig == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalResourceConfig(devId, in, para);
    }

    static inline int HalSqCqQuery(uint32_t devId, struct halSqCqQueryInfo *info)
    {
        if (pHalSqCqQuery == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalSqCqQuery(devId, info);
    }

    static inline int HalHostRegister(void *srcPtr, uint64_t size, uint32_t flag, uint32_t devid, void **dstPtr)
    {
        if (pHalHostRegister == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalHostRegister(srcPtr, size, flag, devid, dstPtr);
    }

    static inline int HalHostUnregisterEx(void *srcPtr, uint32_t devid, uint32_t flag)
    {
        if (pHalHostUnregisterEx == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalHostUnregisterEx(srcPtr, devid, flag);
    }

    static inline int DrvNotifyIdAddrOffset(uint32_t deviceId, struct drvNotifyInfo *drvInfo)
    {
        if (pDrvNotifyIdAddrOffset == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pDrvNotifyIdAddrOffset(deviceId, drvInfo);
    }

static inline int HalMemAddressReserve(void **ptr, size_t size, size_t alignment, void *addr, uint64_t flag)
    {
        if (pHalMemAddressReserve == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalMemAddressReserve(ptr, size, alignment, addr, flag);
    }

    static inline int HalMemAddressFree(void *ptr)
    {
        if (pHalMemAddressFree == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalMemAddressFree(ptr);
    }

    static inline int HalMemCreate(drv_mem_handle_t **handle, size_t size, struct drv_mem_prop *prop, uint64_t flag)
    {
        if (pHalMemCreate == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalMemCreate(handle, size, prop, flag);
    }

    static inline int HalMemRelease(drv_mem_handle_t *handle)
    {
        if (pHalMemRelease == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalMemRelease(handle);
    }

    static inline int HalMemMap(void *ptr, size_t size, size_t offset, drv_mem_handle_t *handle, uint64_t flag)
    {
        if (pHalMemMap == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalMemMap(ptr, size, offset, handle, flag);
    }

    static inline int HalMemUnmap(void *ptr)
    {
        if (pHalMemUnmap == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalMemUnmap(ptr);
    }

    static inline int HalMemExport(drv_mem_handle_t *handle, drv_mem_handle_type type, uint64_t flags,
                                   struct MemShareHandle *sHandle)
    {
        if (pHalMemExport == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalMemExport(handle, type, flags, sHandle);
    }

    static inline int HalMemImport(drv_mem_handle_type type, struct MemShareHandle *sHandle, uint32_t devid,
                                   drv_mem_handle_t **handle)
    {
        if (pHalMemImport == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalMemImport(type, sHandle, devid, handle);
    }

    static inline int HalMemShareHandleSetAttribute(uint64_t handle, enum ShareHandleAttrType type,
                                                    struct ShareHandleAttr attr)
    {
        if (pHalMemShareHandleSetAttribute == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalMemShareHandleSetAttribute(handle, type, attr);
    }

    static inline int HalMemTransShareableHandle(drv_mem_handle_type type, struct MemShareHandle *handle,
                                                 uint32_t *serverId, uint64_t *shareableHandle)
    {
        if (pHalMemTransShareableHandle == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalMemTransShareableHandle(type, handle, serverId, shareableHandle);
    }

    static inline int HalMemGetAllocationGranularity(const struct drv_mem_prop *prop,
                                           drv_mem_granularity_options option, size_t *granularity)
    {
        if (pHalMemGetAllocationGranularity == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pHalMemGetAllocationGranularity(prop, option, granularity);
    }

private:
    static Result LoadHybmVmmLibrary(uint32_t gvaVersion);
    static Result LoadHybmV1V2Library(uint32_t gvaVersion);

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

    static halSqTaskSendFunc pHalSqTaskSend;
    static halCqReportRecvFunc pHalCqReportRecv;
    static halSqCqAllocateFunc pHalSqCqAllocate;
    static halSqCqFreeFunc pHalSqCqFree;
    static halResourceIdAllocFunc pHalResourceIdAlloc;
    static halResourceIdFreeFunc pHalResourceIdFree;
    static halGetSsidFunc pHalGetSsid;
    static halResourceConfigFunc pHalResourceConfig;
    static halSqCqQueryFunc pHalSqCqQuery;
    static halHostRegisterFunc pHalHostRegister;
    static halHostUnregisterExFunc pHalHostUnregisterEx;
    static drvNotifyIdAddrOffsetFunc pDrvNotifyIdAddrOffset;

    static halMemAddressReserveFunc pHalMemAddressReserve;
    static halMemAddressFreeFunc pHalMemAddressFree;
    static halMemCreateFunc pHalMemCreate;
    static halMemReleaseFunc pHalMemRelease;
    static halMemMapFunc pHalMemMap;
    static halMemUnmapFunc pHalMemUnmap;
    static halMemExportFunc pHalMemExport;
    static halMemImportFunc pHalMemImport;
    static halMemShareHandleSetAttributeFunc pHalMemShareHandleSetAttribute;
    static halMemTransShareableHandleFunc pHalMemTransShareableHandle;
    static halMemGetAllocationGranularityFunc pHalMemGetAllocationGranularity;
};

} // namespace mf
} // namespace ock

#endif // MF_HYBM_CORE_DL_HAL_API_H
