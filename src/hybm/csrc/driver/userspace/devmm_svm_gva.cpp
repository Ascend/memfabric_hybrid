/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <map>
#include <fstream>
#include <climits>
#include "hybm_logger.h"
#include "devmm_define.h"
#include "hybm_cmd.h"
#include "dl_hal_api.h"
#include "devmm_svm_gva.h"

namespace ock {
namespace mf {
namespace drv {

const std::string LEAST_DRIVER_VER = "V100R001C21B035";

struct DevmmGvaHeap {
    uint32_t inited = false;
    int32_t deviceId = -1;

    uint64_t start = 0;
    uint64_t end = 0;

    pthread_mutex_t treeLock;
    std::map<uint64_t, uint64_t> tree;
};
DevmmGvaHeap *g_gvaHeapMgr = nullptr;

static void SetModuleId2Advise(uint32_t modelId, uint32_t *advise)
{
    if (advise == nullptr) {
        return;
    }
    *advise = *advise | ((modelId & DV_ADVISE_MODULE_ID_MASK) << DV_ADVISE_MODULE_ID_BIT);
}

static void FillSvmHeapType(uint32_t advise, struct devmm_virt_heap_type *heapType)
{
    if (heapType == nullptr) {
        return;
    }
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
    if (g_gvaHeapMgr != nullptr) {
        if (ed != g_gvaHeapMgr->start) {
            BM_LOG_ERROR("init gva manager error. input_ed:0x" << std::hex << ed << " pre_st:0x"
                                                               << g_gvaHeapMgr->start);
            return -1;
        }
        if (deviceId != g_gvaHeapMgr->deviceId) {
            BM_LOG_ERROR("init gva manager error. input_device:" << deviceId << " pre_device:"
                                                                 << g_gvaHeapMgr->deviceId);
            return -1;
        }
        g_gvaHeapMgr->start = st;
        return 0;
    }

    g_gvaHeapMgr = new (std::nothrow) DevmmGvaHeap;
    if (g_gvaHeapMgr == nullptr) {
        BM_LOG_ERROR("malloc gva_heap error.");
        return -1;
    }
    if (pthread_mutex_init(&g_gvaHeapMgr->treeLock, nullptr) != 0) {
        BM_LOG_ERROR("failed to init mutex");
        delete g_gvaHeapMgr;
        g_gvaHeapMgr = nullptr;
        return -1;
    }

    g_gvaHeapMgr->tree.clear();
    g_gvaHeapMgr->start = st;
    g_gvaHeapMgr->end = ed;
    g_gvaHeapMgr->deviceId = deviceId;
    g_gvaHeapMgr->inited = true;

    return 0;
}

static bool DevmmGvaHeapCheckInRange(uint64_t key, uint64_t len)
{
    if (g_gvaHeapMgr->tree.empty()) {
        return false;
    }

    auto it = g_gvaHeapMgr->tree.lower_bound(key);
    if (it != g_gvaHeapMgr->tree.end()) {
        uint64_t l = it->first;
        uint64_t r = it->second;
        if (key <= l && l < key + len) {
            BM_LOG_ERROR("check in range. (va=0x" << std::hex << key <<
                " len=0x" << len << " L=0x" << l << " R=0x" << r << ")");
            return true;
        }
    }

    if (it == g_gvaHeapMgr->tree.begin()) {
        return false;
    }

    it--;
    uint64_t l = it->first;
    uint64_t r = it->second;
    if (l <= key && key < r) {
        BM_LOG_ERROR("check in range. (va=0x" << std::hex << key <<
            " len=0x" << len << " L=0x" << l << " R=0x" << r << ")");
        return true;
    }
    return false;
}

static bool DevmmTryUpdateGvaHeap(uint64_t va, size_t len)
{
    if (g_gvaHeapMgr == nullptr || !g_gvaHeapMgr->inited) {
        BM_LOG_ERROR("update gva heap failed, gva heap not init.");
        return false;
    }

    if (va < g_gvaHeapMgr->start || va + len > g_gvaHeapMgr->end) {
        BM_LOG_ERROR("update gva heap failed, out of range. (va=0x" << std::hex << va <<
            " len=0x" << len << " st=0x" << g_gvaHeapMgr->start << " ed=0x" << g_gvaHeapMgr->end << ")");
        return false;
    }

    (void)pthread_mutex_lock(&g_gvaHeapMgr->treeLock);
    if (DevmmGvaHeapCheckInRange(va, len)) {
        (void)pthread_mutex_unlock(&g_gvaHeapMgr->treeLock);
        BM_LOG_ERROR("update gva heap failed, has some alloced memory in range.");
        return false;
    }

    g_gvaHeapMgr->tree[va] = va + len;
    (void)pthread_mutex_unlock(&g_gvaHeapMgr->treeLock);
    return true;
}

static int32_t DevmmRemoveInGvaHeap(uint64_t va)
{
    if (g_gvaHeapMgr == nullptr || !g_gvaHeapMgr->inited) {
        BM_LOG_ERROR("remove record in gva heap failed, gva heap not init.");
        return -1;
    }

    (void)pthread_mutex_lock(&g_gvaHeapMgr->treeLock);
    g_gvaHeapMgr->tree.erase(va);
    (void)pthread_mutex_unlock(&g_gvaHeapMgr->treeLock);
    return 0;
}

static struct devmm_virt_com_heap *DevmmVirtAllocHeapForBaseMem(void *mgmt,
    struct devmm_virt_heap_type *heapType, unsigned long allocPtr, size_t allocSize)
{
    struct devmm_virt_com_heap *heapSet = nullptr;
    uint32_t heapIdx;

    heapIdx = DlHalApi::HalDevmmVaToHeapIdx(mgmt, allocPtr);
    heapSet = (struct devmm_virt_com_heap *)DlHalApi::HalDevmmVirtGetHeapFromQueue(mgmt, heapIdx, allocSize);
    if (heapSet == nullptr) {
        BM_LOG_ERROR("Base alloc heap failed. (size=0x" << std::hex << allocSize << ")");
        return nullptr;
    }
    DlHalApi::HalDevmmVirtNormalHeapUpdateInfo(mgmt, heapSet, heapType, nullptr, allocSize);
    return heapSet;
}

static inline void DevmmVirtListAddInner(struct devmm_virt_list_head *new_, struct devmm_virt_list_head *prev,
    struct devmm_virt_list_head *next)
{
    next->prev = new_;
    new_->next = next;
    new_->prev = prev;
    prev->next = new_;
}

static inline void DevmmVirtListAdd(struct devmm_virt_list_head *new_, struct devmm_virt_list_head *head)
{
    if (head == nullptr) {
        BM_LOG_ERROR("head is nullptr");
        return;
    }
    DevmmVirtListAddInner(new_, head, head->next);
}

static inline void DevmmVirtListDelInner(struct devmm_virt_list_head *prev, struct devmm_virt_list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void DevmmVirtListDel(struct devmm_virt_list_head *entry)
{
    if (entry == nullptr) {
        BM_LOG_ERROR("entry is nullptr");
        return;
    }
    DevmmVirtListDelInner(entry->prev, entry->next);
    // init
    entry->next = entry;
    entry->prev = entry;
}

static inline uint32_t DevmmHeapSubTypeToMemVal(uint32_t type)
{
    static uint32_t memVal[SUB_MAX_TYPE] = {
        [SUB_SVM_TYPE] = MEM_SVM_VAL,
        [SUB_DEVICE_TYPE] = MEM_DEV_VAL,
        [SUB_HOST_TYPE] = MEM_HOST_VAL,
        [SUB_DVPP_TYPE] = MEM_DEV_VAL,
        [SUB_READ_ONLY_TYPE] = MEM_DEV_VAL,
        [SUB_RESERVE_TYPE] = MEM_RESERVE_VAL,
        [SUB_DEV_READ_ONLY_TYPE]= MEM_DEV_VAL
    };

    return memVal[type];
}

static void DevmmPrimaryHeapModuleMemStatsInc(struct devmm_virt_com_heap *heap,
    uint32_t moduleId, uint64_t size)
{
    if (heap == nullptr) {
        return;
    }
    uint32_t memVal = DevmmHeapSubTypeToMemVal(heap->heap_sub_type);
    uint32_t pageType = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? DEVMM_HUGE_PAGE_TYPE : DEVMM_NORMAL_PAGE_TYPE;
    uint32_t phyMemtype = heap->heap_mem_type;
    uint32_t devid = (heap->heap_list_type - DEVICE_AGENT0_LIST);
    struct svm_mem_stats_type type;

    type.mem_val = memVal;
    type.page_type = pageType;
    type.phy_memtype = phyMemtype;
    if (heap->heap_sub_type != SUB_RESERVE_TYPE) {
        DlHalApi::HalSvmModuleAllocedSizeInc(&type, devid, moduleId, size);
        heap->module_id = moduleId;
    }
}

static uint64_t DevmmVirtAllocGvaMem(void *mgmt, uint64_t allocPtr,
    size_t allocSize, struct devmm_virt_heap_type *heap_type, uint32_t advise)
{
    uint32_t module_id = ((advise >> DV_ADVISE_MODULE_ID_BIT) & DV_ADVISE_MODULE_ID_MASK);
    struct devmm_heap_list *heap_list = nullptr;
    struct devmm_virt_com_heap *heap = nullptr;
    uint64_t retPtr = allocPtr;
    int32_t ret;

    retPtr = DlHalApi::HalDevmmVirtAllocMemFromBase(mgmt, allocSize, 0, allocPtr);
    if (retPtr != allocPtr) {
        BM_LOG_ERROR("gva alloc mem failed. (size=0x" << std::hex << allocSize <<
            (retPtr >= DEVMM_SVM_MEM_START ? ", maybe ascend driver need to update)" : ")"));
        return 0;
    }

    /* alloc large mem para is addr of heap_type */
    heap = DevmmVirtAllocHeapForBaseMem(mgmt, heap_type, retPtr, allocSize);
    if (heap == nullptr) {
        BM_LOG_ERROR("gva alloc heap failed. (size=0x" << std::hex << allocSize << ")");
        (void)DlHalApi::HalDevmmIoctlFreePages(retPtr);
        return 0;
    }

    ret = DlHalApi::HalDevmmIoctlEnableHeap(heap->heap_idx, heap_type->heap_type,
        heap_type->heap_sub_type, heap->heap_size, heap_type->heap_list_type);
    if (ret != 0) {
        BM_LOG_ERROR("gva update heap failed. (size=0x" << std::hex << allocSize << ")");
        (void)DlHalApi::HalDevmmVirtSetHeapIdle(mgmt, heap);
        (void)DlHalApi::HalDevmmIoctlFreePages(retPtr);
        return 0;
    }

    if (DlHalApi::HalDevmmGetHeapListByType(mgmt, heap_type, &heap_list) != 0 || heap_list == nullptr) {
        (void)DlHalApi::HalDevmmVirtDestroyHeap(mgmt, heap, false);
        (void)DlHalApi::HalDevmmIoctlFreePages(retPtr);
        return 0;
    }

    DevmmPrimaryHeapModuleMemStatsInc(heap, module_id, allocSize);
    (void)pthread_rwlock_wrlock(&heap_list->list_lock);
    DevmmVirtListAdd(&heap->list, &heap_list->heap_list);
    heap_list->heap_cnt++;
    (void)pthread_rwlock_unlock(&heap_list->list_lock);
    BM_LOG_INFO("gva alloc heap. (size=0x" << std::hex << allocSize << ")");
    return retPtr;
}

static int32_t DevmmFreeManagedNomal(uint64_t va)
{
    void *mgmt = nullptr;
    struct devmm_virt_com_heap *heap = nullptr;
    heap = (devmm_virt_com_heap *)DlHalApi::HalDevmmVaToHeap(va);
    if ((heap == nullptr) || heap->heap_type != DEVMM_HEAP_HUGE_PAGE || heap->heap_sub_type != SUB_SVM_TYPE ||
        heap->heap_list_type != SVM_LIST) {
        BM_LOG_ERROR("DevmmFreeManagedNomal get heap info error. ");
        return BM_ERROR;
    }

    mgmt = DlHalApi::HalDevmmVirtGetHeapMgmt();
    if (mgmt == nullptr) {
        BM_LOG_ERROR("DevmmFreeManagedNomal get heap mgmt is nullptr.");
        return BM_ERROR;
    }

    struct devmm_heap_list *heap_list = nullptr;
    struct devmm_virt_heap_type heap_type;
    heap_type.heap_type = heap->heap_type;
    heap_type.heap_list_type = heap->heap_list_type;
    heap_type.heap_sub_type = heap->heap_sub_type;
    heap_type.heap_mem_type = heap->heap_mem_type;

    if (DlHalApi::HalDevmmGetHeapListByType(mgmt, &heap_type, &heap_list) != 0) {
        BM_LOG_ERROR("get heap list error. ");
        return BM_ERROR;
    }
    (void)pthread_rwlock_wrlock(&heap_list->list_lock);
    DevmmVirtListDel(&heap->list);
    heap_list->heap_cnt--;
    (void)pthread_rwlock_unlock(&heap_list->list_lock);

    if (DlHalApi::HalDevmmVirtDestroyHeap(mgmt, heap, true) != 0) {
        BM_LOG_ERROR("Destroy ptr error. ");
        return BM_ERROR;
    }
    return BM_OK;
}

int32_t HalGvaReserveMemory(uint64_t *address, size_t size, int32_t deviceId, uint64_t flags)
{
    if (address == nullptr) {
        BM_LOG_ERROR("address is nullptr");
        return -1;
    }
    if (size == 0 || size > (DEVMM_SVM_MEM_SIZE >> 1)) { // init size <= 4T
        BM_LOG_ERROR("gva init failed, (size must > 0 && <= 4T). "
                     "(flag=" << flags << " size=0x" << std::hex << size << ")");
        return -1;
    }
    uint32_t advise = 0;
    struct devmm_virt_heap_type heap_type{};
    size_t allocSize = ALIGN_UP(size, DEVMM_HEAP_SIZE);

    advise |= DV_ADVISE_HUGEPAGE;
    SetModuleId2Advise(HCCL_HAL_MODULE_ID, &advise);
    FillSvmHeapType(advise, &heap_type);

    void *mgmt = nullptr;
    mgmt = DlHalApi::HalDevmmVirtGetHeapMgmt();
    if (mgmt == nullptr) {
        BM_LOG_ERROR("HalGvaInitMemory get heap mgmt is nullptr.");
        return -1;
    }

    uint64_t va = (DEVMM_SVM_MEM_START + DEVMM_SVM_MEM_SIZE) - allocSize;
    if (g_gvaHeapMgr != nullptr && g_gvaHeapMgr->inited) {
        va = g_gvaHeapMgr->start - allocSize;
    }

    uint64_t retVa = DevmmVirtAllocGvaMem(mgmt, va, allocSize, &heap_type, advise);
    if (retVa != va) {
        BM_LOG_ERROR("HalGvaInitMemory alloc mem failed. (flag=" << flags <<
            " size=0x" << std::hex << size << " ret=0x" << retVa << ")");
        return -1;
    }

    int32_t ret = InitGvaHeapMgmt(va, va + allocSize, deviceId);
    if (ret != 0) {
        BM_LOG_ERROR("HalGvaInitMemory init gva heap failed. ");
        DevmmFreeManagedNomal(va);
        return -1;
    }

    *address = va;
    return BM_OK;
}

int32_t HalGvaUnreserveMemory(void)
{
    if (g_gvaHeapMgr == nullptr) {
        return BM_OK;
    }

    (void)pthread_mutex_lock(&g_gvaHeapMgr->treeLock);
    for (auto &pair : g_gvaHeapMgr->tree) {
        (void)DlHalApi::HalDevmmIoctlFreePages(pair.first);
    }
    g_gvaHeapMgr->tree.clear();
    (void)pthread_mutex_unlock(&g_gvaHeapMgr->treeLock);

    DevmmFreeManagedNomal(g_gvaHeapMgr->start);
    delete g_gvaHeapMgr;
    g_gvaHeapMgr = nullptr;
    return BM_OK;
}

int32_t HalGvaAlloc(uint64_t address, size_t size, uint64_t flags)
{
    uint64_t va = address;
    if ((va % DEVMM_MAP_ALIGN_SIZE != 0) || (size % DEVMM_MAP_ALIGN_SIZE != 0)) {
        BM_LOG_ERROR("open gva va check failed, size must the align of 2M. (size=0x" <<
            std::hex << size << ")");
        return -1;
    }

    if (!DevmmTryUpdateGvaHeap(va, size)) {
        return -1;
    }

    uint32_t advise = 0;
    advise |= DV_ADVISE_HUGEPAGE | DV_ADVISE_HBM;
    advise |= DV_ADVISE_POPULATE | DV_ADVISE_LOCK_DEV;
    SetModuleId2Advise(APP_MODULE_ID, &advise);
    BM_ASSERT_RETURN(g_gvaHeapMgr != nullptr, BM_INVALID_PARAM);
    int32_t ret = HybmIoctlAllocAnddAdvice(va, size, g_gvaHeapMgr->deviceId, advise);
    if (ret != 0) {
        BM_LOG_ERROR("Alloc gva local mem error. (ret=" << ret << " size=0x" << std::hex <<
            size << "advise=0x" << advise << ")");
        (void)DevmmRemoveInGvaHeap(va);
        return -1;
    }

    return 0;
}

int32_t HalGvaFree(uint64_t address, size_t size)
{
    if (DevmmRemoveInGvaHeap(address) == 0) {
        return DlHalApi::HalDevmmIoctlFreePages(address);
    } else {
        return -1;
    }
}

static int32_t DevmmOpenGvaMalloc(uint64_t va, size_t len)
{
    if ((va % DEVMM_MAP_ALIGN_SIZE != 0) || (len % DEVMM_MAP_ALIGN_SIZE != 0)) {
        BM_LOG_ERROR("open gva va check failed, size must the align of 2M. (size=0x" <<
            std::hex << len << ")");
        return -1;
    }

    if (!DevmmTryUpdateGvaHeap(va, len)) {
        return -1;
    }

    uint32_t advise = 0;
    advise |= DV_ADVISE_HUGEPAGE;
    SetModuleId2Advise(HCCL_HAL_MODULE_ID, &advise);
    int32_t ret = HybmIoctlAllocAnddAdvice(va, len, g_gvaHeapMgr->deviceId, advise);
    if (ret != 0) {
        BM_LOG_ERROR("Alloc gva open mem error. (ret=" << ret << " size=0x" << std::hex <<
            len << "advise=0x" << advise << ")");
        (void)DevmmRemoveInGvaHeap(va);
        return -1;
    }

    return 0;
}

int32_t HalGvaOpen(uint64_t address, const char *name, size_t size, uint64_t flags)
{
    if (DevmmOpenGvaMalloc(address, size) != 0) {
        BM_LOG_ERROR("HalGvaOpen malloc gva error. (size=0x" << std::hex << size << ")");
        return -1;
    }

    auto ret = HybmMapShareMemory(name, reinterpret_cast<void *>(address), size, flags);
    if (ret != 0) {
        BM_LOG_ERROR("HalGvaOpen map share memory error, ret = " << ret);
        if (HalGvaFree(address, 0) != 0) {
            BM_LOG_ERROR("HalGvaOpen free gva error. ");
        }
    }
    return ret;
}

int32_t HalGvaClose(uint64_t address, uint64_t flags)
{
    auto ret = HybmUnmapShareMemory(reinterpret_cast<void *>(address), flags);
    if (ret != 0) {
        BM_LOG_ERROR("Close error. ret=" << ret);
        return ret;
    }
    
    return HalGvaFree(address, 0);
}

}
}
}