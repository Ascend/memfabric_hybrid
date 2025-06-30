/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MF_HYBRID_BM_H__
#define __MF_HYBRID_BM_H__

#include <string>
#include <type_traits>
#include "hybm_big_mem.h"
#include "hybm_common_include.h"
#include "hybm_mem_slice.h"

namespace ock {
namespace mf {
class MemEntity {
public:
    virtual int32_t Initialize(const hybm_options *options) noexcept = 0;
    virtual void UnInitialize() noexcept = 0;

    virtual int32_t ReserveMemorySpace(void **reservedMem) noexcept = 0;
    virtual int32_t UnReserveMemorySpace() noexcept = 0;

    virtual int32_t AllocLocalMemory(uint64_t size, uint32_t flags, hybm_mem_slice_t &slice) noexcept = 0;
    virtual void FreeLocalMemory(hybm_mem_slice_t slice, uint32_t flags) noexcept = 0;

    virtual int32_t ExportExchangeInfo(hybm_exchange_info &desc, uint32_t flags) noexcept = 0;
    virtual int32_t ExportExchangeInfo(hybm_mem_slice_t slice, hybm_exchange_info &desc, uint32_t flags) noexcept = 0;
    virtual int32_t ImportExchangeInfo(const hybm_exchange_info desc[], uint32_t count, uint32_t flags) noexcept = 0;

    virtual int32_t SetExtraContext(const void *context, uint32_t size) noexcept = 0;

    virtual int32_t Start(uint32_t flags) noexcept = 0;
    virtual void Stop() noexcept = 0;

    virtual int32_t Mmap() noexcept = 0;
    virtual int32_t Join(uint32_t rank) noexcept = 0;
    virtual int32_t Leave(uint32_t rank) noexcept = 0;

    virtual bool CheckAddressInEntity(const void *ptr, uint64_t length) const noexcept = 0;
    virtual int32_t CopyData(const void *src, void *dest, uint64_t length, hybm_data_copy_direction direction,
                             uint32_t flags) noexcept = 0;
    virtual int32_t CopyData2d(const void *src, uint64_t spitch, void *dest, uint64_t dpitch,  uint64_t width,
                               uint64_t height, hybm_data_copy_direction direction, uint32_t flags) noexcept = 0;

    virtual ~MemEntity() noexcept = default;
};

using MemEntityPtr = std::shared_ptr<MemEntity>;
}  // namespace mf
}  // namespace ock

#endif  // __MF_HYBRID_BM_H__
