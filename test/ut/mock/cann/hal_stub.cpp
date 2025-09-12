/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <cstdint>
#include <cstddef>

constexpr int32_t RETURN_OK = 0;
constexpr int32_t RETURN_ERROR = -1;
constexpr uint64_t baseAddr = 0x10000000000;

extern "C" {
int32_t halGvaReserveMemory(void **address, size_t size, int32_t deviceId, uint64_t flags)
{
    *address = (void *) baseAddr;
    return RETURN_OK;
}

int32_t halGvaUnreserveMemory()
{
    return RETURN_OK;
}

int32_t halGvaAlloc(void *address, size_t size, uint64_t flags)
{
    return RETURN_OK;
}

int32_t halGvaFree(void *address, size_t size)
{
    return RETURN_OK;
}

int32_t halGvaOpen(void *address, const char *name, size_t size, uint64_t flags)
{
    return RETURN_OK;
}

int32_t halGvaClose(void *address, uint64_t flags)
{
    return RETURN_OK;
}

int *g_devmm_mem_dev;
void svm_module_alloced_size_inc(void *a, uint32_t b, uint32_t c, uint64_t d)
{
    return;
}

uint64_t devmm_virt_alloc_mem_from_base(void *a, size_t b, uint32_t c, uint64_t d)
{
    return RETURN_OK;
}

int32_t devmm_ioctl_enable_heap(uint32_t a, uint32_t b, uint32_t c, uint64_t d, uint32_t e)
{
    return RETURN_OK;
}

int32_t devmm_get_heap_list_by_type(void *a, void *b, void *c)
{
    return RETURN_OK;
}

int32_t devmm_virt_set_heap_idle(void *a, void *b)
{
    return RETURN_OK;
}

int32_t devmm_virt_destroy_heap(void *a, void *b, bool c)
{
    return RETURN_OK;
}

void *devmm_virt_get_heap_mgmt()
{
    return nullptr;
}

int32_t devmm_ioctl_free_pages(uint64_t a)
{
    return RETURN_OK;
}

uint32_t devmm_va_to_heap_idx(const void *a, uint64_t b)
{
    return RETURN_OK;
}

void *devmm_virt_get_heap_from_queue(void *a, uint32_t b, size_t c)
{
    return nullptr;
}

void devmm_virt_normal_heap_update_info(void *a, void *b, void *c, void *d, uint64_t e)
{
    return;
}

void *devmm_va_to_heap(uint64_t a)
{
    return nullptr;
}
}