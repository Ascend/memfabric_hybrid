/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <cstdint>
#include <cstddef>

constexpr int32_t RETURN_OK = 0;
constexpr int32_t RETURN_ERROR = -1;
constexpr uint64_t baseAddr = 0x10000000000;

extern "C" {
int *g_devmm_mem_dev;

uint64_t devmm_virt_alloc_mem_from_base(void *mgmt, size_t alloc_size, uint32_t advise, uint64_t alloc_ptr)
{
    return 0;
}

int32_t devmm_ioctl_enable_heap(uint32_t heap_idx, uint32_t heap_type, uint32_t heap_sub_type,
                                uint64_t heap_size, uint32_t heap_list_type)
{
    return 0;
}

int32_t devmm_get_heap_list_by_type(void *p_heap_mgmt, void *heap_type, void **heap_list)
{
    return 0;
}

int32_t devmm_virt_set_heap_idle(void *mgmt, void *heap)
{
    return 0;
}

void *devmm_virt_get_heap_mgmt(void)
{
    return NULL;
}

int32_t devmm_ioctl_free_pages(uint64_t ptr)
{
    return 0;
}

uint32_t devmm_va_to_heap_idx(const void *mgmt, uint64_t va)
{
    return 0;
}

void *devmm_va_to_heap(uint64_t va)
{
    return NULL;
}

void *devmm_virt_get_heap_from_queue(void *mgmt, uint32_t heap_idx, size_t heap_size)
{
    return NULL;
}

void devmm_virt_normal_heap_update_info(void *mgmt, void *heap, void *heap_type, void *ops, uint64_t alloc_size)
{
    return;
}

void svm_module_alloced_size_inc(void *type, uint32_t devid, uint32_t module_id, uint64_t size)
{
    return;
}

int32_t devmm_virt_destroy_heap(void *mgmt, void *heap, bool need_mem_stats_dec)
{
    return 0;
}

int halSqTaskSend(uint32_t devId, void *info)
{
    return 0;
}

int halCqReportRecv(uint32_t devId, void *info)
{
    return 0;
}

int halSqCqAllocate(uint32_t devId, void *in, void *out)
{
    return 0;
}

int halSqCqFree(uint32_t devId, void *info)
{
    return 0;
}

int halResourceIdAlloc(uint32_t devId, void *in, void *out)
{
    return 0;
}

int halResourceIdFree(uint32_t devId, void *in)
{
    return 0;
}

int drvMemSmmuQuery(uint32_t device, uint32_t *SSID)
{
    return 0;
}

int halResourceConfig(uint32_t devId, void *in, void *para)
{
    return 0;
}

int halSqCqQuery(uint32_t devId, void *info)
{
    return 0;
}

int halHostRegister(void *srcPtr, uint64_t size, uint32_t flag, uint32_t devid, void **dstPtr)
{
    return 0;
}

int halHostUnregister(void *srcPtr, uint32_t devid)
{
    return 0;
}
}