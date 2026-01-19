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
#include <map>
#include <vector>
#include <fstream>
#include <climits>
#include "hybm_common_include.h"
#include "hybm_gva_version.h"
#include "hybm_logger.h"
#include "devmm_define.h"
#include "hybm_cmd.h"
#include "dl_hal_api.h"
#include "devmm_svm_gva.h"

namespace ock {
namespace mf {
namespace drv {

struct GvaHeap {
    uint32_t inited = false;
    int32_t deviceId = -1;

    uint64_t start = 0;
    uint64_t end = 0;

    pthread_mutex_t treeLock;
    std::map<uint64_t, uint64_t> tree;
    std::map<uint64_t, uint64_t> reserved;
};
GvaHeap g_gvaHeapMgr;

static void SetModuleId2Advise(uint32_t modelId, uint32_t *advise)
{
    *advise = *advise | ((modelId & DV_ADVISE_MODULE_ID_MASK) << DV_ADVISE_MODULE_ID_BIT);
}

static void FillSvmHeapType(uint32_t advise, struct DevVirtHeapType *heapType)
{
    heapType->heap_list_type = SVM_LIST;
    heapType->heap_sub_type = SUB_SVM_TYPE;
    heapType->heap_mem_type = DEVMM_DDR_MEM;
    if ((advise & DV_ADVISE_HUGEPAGE) != 0) {
        heapType->heap_type = DEVMM_HEAP_HUGE_PAGE;
    } else {
        heapType->heap_type = DEVMM_HEAP_CHUNK_PAGE;
    }
}

static int32_t InitGvaHeapMgmt(uint64_t st, uint64_t ed, int32_t deviceId)
{
    if (g_gvaHeapMgr.inited) {
        if (ed != g_gvaHeapMgr.start) {
            BM_LOG_ERROR("init gva mgr error. input_ed:0x" << std::hex << ed << " pre_st:0x" << g_gvaHeapMgr.start);
            return -1;
        }
        if (deviceId != g_gvaHeapMgr.deviceId) {
            BM_LOG_ERROR("init gva mgr error. input_device:" << deviceId << " pre_device:" << g_gvaHeapMgr.deviceId);
            return -1;
        }
        g_gvaHeapMgr.start = st;
        g_gvaHeapMgr.reserved[st] = ed;
        return 0;
    }

    g_gvaHeapMgr.tree.clear();
    g_gvaHeapMgr.reserved.clear();
    g_gvaHeapMgr.start = st;
    g_gvaHeapMgr.end = ed;
    g_gvaHeapMgr.deviceId = deviceId;
    g_gvaHeapMgr.reserved[st] = ed;
    (void)pthread_mutex_init(&g_gvaHeapMgr.treeLock, nullptr);
    g_gvaHeapMgr.inited = true;

    return 0;
}

static void UnInitGvaHeapMgr()
{
    g_gvaHeapMgr.inited = false;
}

static bool GvaHeapCheckInRange(uint64_t key, uint64_t len)
{
    if (g_gvaHeapMgr.tree.empty()) {
        return false;
    }

    auto it = g_gvaHeapMgr.tree.lower_bound(key);
    if (it != g_gvaHeapMgr.tree.end()) {
        uint64_t l = it->first;
        uint64_t r = it->second;
        if (key <= l && l < key + len) {
            BM_LOG_ERROR("check in range. (key=0x" << std::hex << key << " len=0x" << len << " L=0x" << l << " R=0x"
                                                   << r << ")");
            return true;
        }
    }

    if (it == g_gvaHeapMgr.tree.begin()) {
        return false;
    }

    it--;
    uint64_t l = it->first;
    uint64_t r = it->second;
    if (l <= key && key < r) {
        BM_LOG_ERROR("check in range. (key=0x" << std::hex << key << " len=0x" << len << " L=0x" << l << " R=0x" << r
                                               << ")");
        return true;
    }
    return false;
}

static bool TryUpdateGvaHeap(uint64_t va, size_t len)
{
    if (!g_gvaHeapMgr.inited) {
        BM_LOG_ERROR("update gva heap failed, gva heap not init.");
        return false;
    }

    if (va < g_gvaHeapMgr.start || va + len > g_gvaHeapMgr.end) {
        BM_LOG_ERROR("update gva heap failed, out of range. (key=0x" << std::hex << va << " len=0x" << len << " st=0x"
                                                                     << g_gvaHeapMgr.start << " ed=0x"
                                                                     << g_gvaHeapMgr.end << ")");
        return false;
    }

    (void)pthread_mutex_lock(&g_gvaHeapMgr.treeLock);
    if (GvaHeapCheckInRange(va, len)) {
        (void)pthread_mutex_unlock(&g_gvaHeapMgr.treeLock);
        BM_LOG_ERROR("update gva heap failed, has some alloced memory in range.");
        return false;
    }

    g_gvaHeapMgr.tree[va] = va + len;
    (void)pthread_mutex_unlock(&g_gvaHeapMgr.treeLock);
    return true;
}

static int32_t RemoveInGvaHeap(uint64_t va)
{
    if (!g_gvaHeapMgr.inited) {
        BM_LOG_WARN("remove record in gva heap failed, gva heap not init.");
        return -1;
    }

    (void)pthread_mutex_lock(&g_gvaHeapMgr.treeLock);
    const auto count = g_gvaHeapMgr.tree.erase(va);
    (void)pthread_mutex_unlock(&g_gvaHeapMgr.treeLock);

    if (count == 0) {
        return -1;
    }
    return 0;
}

static void GvaHeapRemoveReserved(uint64_t va)
{
    if (!g_gvaHeapMgr.inited) {
        BM_LOG_WARN("remove reserved in gva heap failed, gva heap not init.");
        return;
    }

    std::vector<uint64_t> vaList;
    (void)pthread_mutex_lock(&g_gvaHeapMgr.treeLock);
    if (g_gvaHeapMgr.reserved.find(va) == g_gvaHeapMgr.reserved.end()) {
        BM_LOG_WARN("not reserved this va!");
        (void)pthread_mutex_unlock(&g_gvaHeapMgr.treeLock);
        return;
    }
    uint64_t ed = g_gvaHeapMgr.reserved[va];

    auto it = g_gvaHeapMgr.tree.lower_bound(va);
    while (it != g_gvaHeapMgr.tree.end() && it->second <= ed) {
        vaList.push_back(it->first);
        it++;
    }

    for (auto ptr : vaList) {
        g_gvaHeapMgr.tree.erase(ptr);
        DlHalApi::HalIoctlFreePages(ptr);
    }
    g_gvaHeapMgr.reserved.erase(va);
    (void)pthread_mutex_unlock(&g_gvaHeapMgr.treeLock);
}

static struct DevVirtComHeap *VirtAllocHeapForBaseMem(void *mgmt, struct DevVirtHeapType *heapType, uint64_t allocPtr,
                                                      size_t allocSize)
{
    struct DevVirtComHeap *heapSet = nullptr;
    uint32_t heapIdx;

    heapIdx = DlHalApi::HalVaToHeapIdx(mgmt, allocPtr);
    heapSet = (struct DevVirtComHeap *)DlHalApi::HalVirtGetHeapFromQueue(mgmt, heapIdx, allocSize);
    if (heapSet == nullptr) {
        BM_LOG_ERROR("Base alloc heap failed. (size=0x" << std::hex << allocSize << ")");
        return nullptr;
    }
    DlHalApi::HalVirtNormalHeapUpdateInfo(mgmt, heapSet, heapType, nullptr, allocSize);
    return heapSet;
}

static inline void VirtListAddInner(struct DVirtListHead *new_, struct DVirtListHead *prev, struct DVirtListHead *next)
{
    next->prev = new_;
    new_->next = next;
    new_->prev = prev;
    prev->next = new_;
}

static inline void VirtListAdd(struct DVirtListHead *new_, struct DVirtListHead *head)
{
    VirtListAddInner(new_, head, head->next);
}

static inline void VirtListDelInner(struct DVirtListHead *prev, struct DVirtListHead *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void VirtListDel(struct DVirtListHead *entry)
{
    VirtListDelInner(entry->prev, entry->next);
    // init
    entry->next = entry;
    entry->prev = entry;
}

static inline uint32_t HeapSubTypeToMemVal(uint32_t type)
{
    static uint32_t memVal[SUB_MAX_TYPE] = {
        [SUB_SVM_TYPE] = MEM_SVM_VAL,          [SUB_DEVICE_TYPE] = MEM_DEV_VAL,    [SUB_HOST_TYPE] = MEM_HOST_VAL,
        [SUB_DVPP_TYPE] = MEM_DEV_VAL,         [SUB_READ_ONLY_TYPE] = MEM_DEV_VAL, [SUB_RESERVE_TYPE] = MEM_RESERVE_VAL,
        [SUB_DEV_READ_ONLY_TYPE] = MEM_DEV_VAL};
    if (type >= SUB_MAX_TYPE) {
        BM_LOG_ERROR("type is out of range, type=" << type);
        return MEM_HOST_VAL;
    }
    return memVal[type];
}

static void PrimaryHeapModuleMemStatsInc(struct DevVirtComHeap *heap, uint32_t moduleId, uint64_t size)
{
    uint32_t memVal = HeapSubTypeToMemVal(heap->heap_sub_type);
    uint32_t pageType = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? DEVMM_HUGE_PAGE_TYPE : DEVMM_NORMAL_PAGE_TYPE;
    uint32_t phyMemtype = heap->heap_mem_type;
    uint32_t devid = (heap->heap_list_type - DEVICE_AGENT0_LIST);
    struct MemStatsType type;

    type.mem_val = memVal;
    type.page_type = pageType;
    type.phy_memtype = phyMemtype;
    if (heap->heap_sub_type != SUB_RESERVE_TYPE) {
        if (HybmGetGvaVersion() == HYBM_GVA_V3) {
            DlHalApi::HalSvmModuleAllocedSizeInc((void *)&type, devid, moduleId, size);
        }
        heap->module_id = moduleId;
    }
}

static void UpdateTreeNode(struct DevRbtreeNode *node, struct DevVirtComHeap *heap, uint64_t va, uint64_t size,
                           uint32_t flag)
{
    DlHalApi::HalAssignNodeData(va, size, size, flag, node);
    (void)DlHalApi::HalInsertIdleSizeTree(node, &heap->rbtree_queue);
    (void)DlHalApi::HalInsertIdleVaTree(node, &heap->rbtree_queue);
}

static void SeparateNodeByVa(struct DevVirtComHeap *heap, struct DevRbtreeNode *node, uint64_t va)
{
    struct DevRbtreeNode *new_node = nullptr;
    uint64_t new_node_size;
    uint64_t node_size;

    new_node = (struct DevRbtreeNode *)DlHalApi::HalAllocRbtreeNode(&heap->rbtree_queue);
    if (new_node == nullptr) {
        BM_LOG_ERROR("Out of memory, cannot malloc new_node.");
        return;
    }

    /* Use va as the dividing line to separate node. */
    (void)DlHalApi::HalEraseIdleSizeTree(node, &heap->rbtree_queue);
    (void)DlHalApi::HalEraseIdleVaTree(node, &heap->rbtree_queue);

    new_node_size = va - node->data.va;
    UpdateTreeNode(new_node, heap, node->data.va, new_node_size, node->data.flag);

    node_size = node->data.size - new_node_size;
    UpdateTreeNode(node, heap, va, node_size, node->data.flag);
}

static struct DevRbtreeNode *GetNodeFromIdleVaTree(struct DevVirtComHeap *heap, size_t allocSize, uint64_t va)
{
    struct DevRbtreeNode *node = nullptr;

    node = (struct DevRbtreeNode *)DlHalApi::HalGetIdleVaNodeInRange(va, &heap->rbtree_queue);
    if (node == nullptr) {
        node = (struct DevRbtreeNode *)DlHalApi::HalGetAllocedNodeInRange(va, &heap->rbtree_queue);
        if (node == nullptr) {
            BM_LOG_ERROR("Cannot find va in allocated tree. size=0x" << std::hex << allocSize);
        } else {
            BM_LOG_ERROR("Va is allocated. size=0x" << allocSize);
        }
        return nullptr;
    }

    if ((va + allocSize) > (node->data.va + node->data.size)) {
        BM_LOG_ERROR("alloc size is too large. size=0x" << std::hex << allocSize);
        return nullptr;
    }

    if (node->data.va != va) {
        SeparateNodeByVa(heap, node, va);
    }
    return node;
}

static void UpdatePeakCacheMem(struct DevVirtComHeap *heap, uint32_t memType)
{
    if (heap->peak_alloc_cache_mem[memType] < heap->cur_alloc_cache_mem[memType]) {
        heap->peak_alloc_cache_time[memType] = time(nullptr);
        heap->peak_alloc_cache_mem[memType] = heap->cur_alloc_cache_mem[memType];
    }
}

static inline void NodeFlagSetValue(uint32_t *flag, uint32_t shift, uint32_t wide, uint32_t value)
{
    uint32_t msk = ((1U << wide) - 1);
    uint32_t val = (msk & value);

    (*flag) &= (~(msk << shift));
    (*flag) |= (val << shift);
}

static int32_t AllocFromNode(struct DevVirtComHeap *heap, struct DevRbtreeNode *node, uint32_t advise, uint32_t memtype)
{
    struct DevRbtreeNode *treeNode = nullptr;
    uint64_t mapSize = node->data.size; // mapSize = allocSize in base_heap
    uint64_t va = node->data.va;

    (void)DlHalApi::HalEraseIdleSizeTree(node, &heap->rbtree_queue);
    (void)DlHalApi::HalEraseIdleVaTree(node, &heap->rbtree_queue);
    treeNode = node;

    va = heap->ops->heap_alloc(heap, treeNode->data.va, mapSize, advise);
    if (va < DEVMM_SVM_MEM_START) {
        BM_LOG_ERROR("Can not alloc address.");
        (void)DlHalApi::HalInsertIdleSizeTree(node, &heap->rbtree_queue);
        (void)DlHalApi::HalInsertIdleVaTree(node, &heap->rbtree_queue);
        return -1;
    }
    heap->sys_mem_alloced += node->data.total;
    heap->sys_mem_alloced_num++;

    NodeFlagSetValue(&treeNode->data.flag, DEVMM_NODE_MEMTYPE_SHIFT, DEVMM_NODE_MEMTYPE_WID, 0);
    treeNode->data.flag |= DEVMM_NODE_MAPPED_FLG;
    heap->cur_alloc_cache_mem[memtype] +=
        (treeNode->data.size <= heap->need_cache_thres[memtype]) ? treeNode->data.size : 0;
    (void)DlHalApi::HalInsertAllocedTree(treeNode, &heap->rbtree_queue);
    return 0;
}

static uint64_t VirtAllocGvaMemInnerCommon(struct DevVirtComHeap *heap, uint64_t bytesize, uint32_t advise,
                                           uint64_t allocPtr)
{
    uint64_t va = ALIGN_DOWN(allocPtr, DEVMM_HEAP_SIZE);
    struct DevRbtreeNode *node = nullptr;
    uint32_t memtype = DEVMM_MEM_NORMAL;
    uint64_t allocSize;
    int32_t ret;

    if ((heap == nullptr) || (bytesize > heap->heap_size) || (bytesize == 0)) {
        BM_LOG_ERROR("Heap is nullptr or alloc memory too large bytesize:"
                     << bytesize << " heap_size:" << (heap != nullptr ? std::to_string(heap->heap_size) : "nullptr"));
        return 1;
    }

    allocSize = ALIGN_UP(bytesize, heap->chunk_size);
    (void)pthread_rwlock_rdlock(&heap->heap_rw_lock);
    (void)pthread_mutex_lock(&heap->tree_lock);
    node = GetNodeFromIdleVaTree(heap, allocSize, va);
    if (node == nullptr) {
        (void)pthread_mutex_unlock(&heap->tree_lock);
        (void)pthread_rwlock_unlock(&heap->heap_rw_lock);
        return 1;
    }

    va = node->data.va;
    ret = AllocFromNode(heap, node, advise, memtype);
    if (ret != 0) {
        BM_LOG_ERROR("Can not alloc memory. bytesize=" << bytesize << " allocSize=" << allocSize);
        (void)pthread_mutex_unlock(&heap->tree_lock);
        (void)pthread_rwlock_unlock(&heap->heap_rw_lock);
        return 1;
    }
    UpdatePeakCacheMem(heap, memtype);

    (void)pthread_mutex_unlock(&heap->tree_lock);
    (void)pthread_rwlock_unlock(&heap->heap_rw_lock);

    return va;
}

static uint64_t VirtAllocGvaMemInner(DevVirtHeapMgmt *mgmt, uint64_t bytesize, uint32_t advise, uint64_t allocPtr)
{
    if (mgmt == nullptr) {
        BM_LOG_ERROR("Invalid mgmt pointer for V1 alloc");
        return 1;
    }
    return VirtAllocGvaMemInnerCommon(&mgmt->heap_queue.base_heap, bytesize, advise, allocPtr);
}

static uint64_t VirtAllocGvaMemInnerV2(DevVirtHeapMgmtV2 *mgmt, uint64_t bytesize, uint32_t advise, uint64_t allocPtr)
{
    if (mgmt == nullptr) {
        BM_LOG_ERROR("Invalid mgmt pointer for V2 alloc");
        return 1;
    }
    return VirtAllocGvaMemInnerCommon(&mgmt->heap_queue.base_heap, bytesize, advise, allocPtr);
}

static int32_t VirtDestroyHeap(void *mgmt, void *heap)
{
    if (HybmGetGvaVersion() == HYBM_GVA_V1) {
        return DlHalApi::HalVirtDestroyHeapV1(mgmt, heap);
    } else if (HybmGetGvaVersion() == HYBM_GVA_V2 or HybmGetGvaVersion() == HYBM_GVA_V3) {
        return DlHalApi::HalVirtDestroyHeapV2(mgmt, heap, true);
    } else {
        return 0;
    }
}

static uint64_t VirtAllocGvaMem(void *mgmt, uint64_t allocPtr, size_t allocSize, struct DevVirtHeapType *heap_type,
                                uint32_t advise)
{
    uint32_t module_id = ((advise >> DV_ADVISE_MODULE_ID_BIT) & DV_ADVISE_MODULE_ID_MASK);
    struct DevHeapList *heap_list = nullptr;
    struct DevVirtComHeap *heap = nullptr;
    uint64_t retPtr;
    int32_t ret;

    if (HybmGetGvaVersion() == HYBM_GVA_V1) {
        retPtr = VirtAllocGvaMemInner((DevVirtHeapMgmt *)mgmt, allocSize, 0, allocPtr);
    } else if (HybmGetGvaVersion() == HYBM_GVA_V2) {
        retPtr = VirtAllocGvaMemInnerV2((DevVirtHeapMgmtV2 *)mgmt, allocSize, 0, allocPtr);
    } else {
        retPtr = DlHalApi::HalVirtAllocMemFromBase(mgmt, allocSize, 0, allocPtr);
    }
    if (retPtr != allocPtr) {
        BM_LOG_ERROR("gva alloc mem failed. (size=0x"
                     << std::hex << allocSize
                     << (retPtr >= DEVMM_SVM_MEM_START ? ", maybe ascend driver need to update)" : ")"));
        return 0;
    }

    /* alloc large mem para is addr of heap_type */
    heap = VirtAllocHeapForBaseMem(mgmt, heap_type, retPtr, allocSize);
    if (heap == nullptr) {
        BM_LOG_ERROR("gva alloc heap failed. (size=0x" << std::hex << allocSize << ")");
        return 0;
    }

    ret = DlHalApi::HalIoctlEnableHeap(heap->heap_idx, heap_type->heap_type, heap_type->heap_sub_type, heap->heap_size,
                                       heap_type->heap_list_type);
    if (ret != 0) {
        BM_LOG_ERROR("gva update heap failed. (size=0x" << std::hex << allocSize << ")");
        (void)DlHalApi::HalVirtSetHeapIdle(mgmt, heap);
        return 0;
    }

    if (DlHalApi::HalGetHeapListByType(mgmt, heap_type, &heap_list) != 0) {
        (void)VirtDestroyHeap(mgmt, heap);
        return 0;
    }

    PrimaryHeapModuleMemStatsInc(heap, module_id, allocSize);
    (void)pthread_rwlock_wrlock(&heap_list->list_lock);
    VirtListAdd(&heap->list, &heap_list->heap_list);
    heap_list->heap_cnt++;
    (void)pthread_rwlock_unlock(&heap_list->list_lock);
    BM_LOG_INFO("gva alloc heap. (size=0x" << std::hex << allocSize << ")");
    return retPtr;
}

static int32_t FreeManagedNomal(uint64_t va)
{
    void *mgmt = nullptr;
    struct DevVirtComHeap *heap = nullptr;
    heap = (DevVirtComHeap *)DlHalApi::HalVaToHeap(va);
    if ((heap == nullptr) || heap->heap_type != DEVMM_HEAP_HUGE_PAGE || heap->heap_sub_type != SUB_SVM_TYPE ||
        heap->heap_list_type != SVM_LIST) {
        BM_LOG_ERROR("FreeManagedNomal get heap info error.");
        return BM_ERROR;
    }

    mgmt = DlHalApi::HalVirtGetHeapMgmt();
    if (mgmt == nullptr) {
        BM_LOG_ERROR("FreeManagedNomal get heap mgmt is nullptr.");
        return BM_ERROR;
    }

    struct DevHeapList *heap_list = nullptr;
    struct DevVirtHeapType heap_type;
    heap_type.heap_type = heap->heap_type;
    heap_type.heap_list_type = heap->heap_list_type;
    heap_type.heap_sub_type = heap->heap_sub_type;
    heap_type.heap_mem_type = heap->heap_mem_type;

    if (DlHalApi::HalGetHeapListByType(mgmt, &heap_type, &heap_list) != 0) {
        BM_LOG_ERROR("get heap list error.");
        return BM_ERROR;
    }
    (void)pthread_rwlock_wrlock(&heap_list->list_lock);
    VirtListDel(&heap->list);
    heap_list->heap_cnt--;
    (void)pthread_rwlock_unlock(&heap_list->list_lock);

    if (VirtDestroyHeap(mgmt, heap) != 0) {
        BM_LOG_ERROR("Destory ptr error.");
        return BM_ERROR;
    }
    return BM_OK;
}

int32_t HalGvaReserveMemory(uint64_t *address, size_t size, int32_t deviceId, uint64_t flags)
{
#ifdef UT_ENABLED
    *address = SVM_END_ADDR - GB;
    return BM_OK;
#endif
    uint32_t advise = 0;
    struct DevVirtHeapType heap_type;
    size_t allocSize = ALIGN_UP(size, DEVMM_HEAP_SIZE);
    if (allocSize == 0 || allocSize > (DEVMM_SVM_MEM_SIZE >> 1) || address == nullptr) { // init size <= 4T
        BM_LOG_ERROR("gva init failed, (size must > 0 && <= 4T) or address is null. (flag=" << flags << " size=0x"
                                                                                            << std::hex << size << ")");
        return -1;
    }

    advise |= DV_ADVISE_HUGEPAGE;
    advise |= (flags & GVA_GIANT_FLAG) ? (DV_ADVISE_GIANTPAGE | DV_ADVISE_DDR) : (DV_ADVISE_HBM);
    SetModuleId2Advise(HCCL_HAL_MODULE_ID, &advise);
    FillSvmHeapType(advise, &heap_type);

    void *mgmt = nullptr;
    mgmt = DlHalApi::HalVirtGetHeapMgmt();
    if (mgmt == nullptr) {
        BM_LOG_ERROR("HalGvaInitMemory get heap mgmt is nullptr.");
        return -1;
    }

    uint64_t va = (DEVMM_SVM_MEM_START + DEVMM_SVM_MEM_SIZE - DEVMM_HEAP_SIZE) - allocSize;
    if (g_gvaHeapMgr.inited) {
        if (allocSize > g_gvaHeapMgr.start) {
            BM_LOG_ERROR("invalid allocSize or g_gvaHeapMgr.start");
            return -1;
        }
        va = g_gvaHeapMgr.start - allocSize;
    }

    uint64_t retVa = VirtAllocGvaMem(mgmt, va, allocSize, &heap_type, advise);
    if (retVa != va) {
        BM_LOG_ERROR("HalGvaInitMemory alloc mem failed. (flag=" << flags << " size=0x" << std::hex << size << ")");
        return -1;
    }

    int32_t ret = InitGvaHeapMgmt(va, va + allocSize, deviceId);
    if (ret != 0) {
        BM_LOG_ERROR("HalGvaInitMemory init gva heap failed.");
        FreeManagedNomal(va);
        return -1;
    }

    *address = va;
    return BM_OK;
}

int32_t HalGvaUnreserveMemory(uint64_t address)
{
#ifdef UT_ENABLED
    return BM_OK;
#endif
    GvaHeapRemoveReserved(address);
    return BM_OK;
}

int32_t HalGvaAlloc(uint64_t address, size_t size, uint64_t flags)
{
#ifdef UT_ENABLED
    return BM_OK;
#endif
    uint64_t va = address;
    if ((va % DEVMM_MAP_ALIGN_SIZE != 0) || (size % DEVMM_MAP_ALIGN_SIZE != 0)) {
        BM_LOG_ERROR("open gva va check failed, size must the align of 2M. (size=0x" << std::hex << size << ")");
        return -1;
    }

    if (!TryUpdateGvaHeap(va, size)) {
        return -1;
    }

    uint32_t advise = DV_ADVISE_HUGEPAGE | DV_ADVISE_POPULATE | DV_ADVISE_LOCK_DEV;
    advise |= (flags & GVA_GIANT_FLAG) ? (DV_ADVISE_GIANTPAGE | DV_ADVISE_DDR) : (DV_ADVISE_HBM);
    SetModuleId2Advise(APP_MODULE_ID, &advise);
    int32_t ret = HybmIoctlAllocAnddAdvice(va, size, g_gvaHeapMgr.deviceId, advise);
    if (ret != 0) {
        BM_LOG_ERROR("Alloc gva local mem error. (ret=" << ret << " size=0x" << std::hex << size << "advise=0x"
                                                        << advise << ")");
        (void)RemoveInGvaHeap(va);
        return -1;
    }

    return 0;
}

int32_t HalGvaFree(uint64_t address, size_t size)
{
#ifdef UT_ENABLED
    return BM_OK;
#endif
    if (RemoveInGvaHeap(address) == 0) {
        return DlHalApi::HalIoctlFreePages(address);
    } else {
        return -1;
    }
}

static int32_t OpenGvaMalloc(uint64_t va, size_t len, uint64_t flags)
{
    if ((va % DEVMM_MAP_ALIGN_SIZE != 0) || (len % DEVMM_MAP_ALIGN_SIZE != 0)) {
        BM_LOG_ERROR("open gva va check failed, size must the align of 2M. (size=0x" << std::hex << len << ")");
        return -1;
    }

    if (!TryUpdateGvaHeap(va, len)) {
        return -1;
    }

    uint32_t advise = DV_ADVISE_HUGEPAGE;
    advise |= (flags & GVA_GIANT_FLAG) ? (DV_ADVISE_GIANTPAGE | DV_ADVISE_DDR) : (DV_ADVISE_HBM);
    SetModuleId2Advise(HCCL_HAL_MODULE_ID, &advise);
    int32_t ret = HybmIoctlAllocAnddAdvice(va, len, g_gvaHeapMgr.deviceId, advise);
    if (ret != 0) {
        BM_LOG_ERROR("Alloc gva open mem error. (ret=" << ret << " size=0x" << std::hex << len << "advise=0x" << advise
                                                       << ")");
        (void)RemoveInGvaHeap(va);
        return -1;
    }

    return 0;
}

int32_t HalGvaOpen(uint64_t address, const char *name, size_t size, uint64_t flags)
{
#ifdef UT_ENABLED
    return BM_OK;
#endif
    if (OpenGvaMalloc(address, size, flags) != 0) {
        BM_LOG_ERROR("HalGvaOpen malloc gva error. (size=0x" << std::hex << size << ")");
        return -1;
    }

    auto ret = HybmMapShareMemory(name, reinterpret_cast<void *>(address), size, flags);
    if (ret != 0) {
        HalGvaFree(address, 0);
    }
    return ret;
}

int32_t HalGvaClose(uint64_t address, uint64_t flags)
{
#ifdef UT_ENABLED
    return BM_OK;
#endif
    auto ret = HybmUnmapShareMemory(reinterpret_cast<void *>(address), flags);
    if (ret != 0) {
        BM_LOG_ERROR("Close error. ret:" << ret);
        return ret;
    }

    return HalGvaFree(address, 0);
}

} // namespace drv
} // namespace mf
} // namespace ock