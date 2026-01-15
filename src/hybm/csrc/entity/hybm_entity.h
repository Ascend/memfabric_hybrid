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
#ifndef __MF_HYBRID_BM_H__
#define __MF_HYBRID_BM_H__

#include <string>
#include <vector>
#include <type_traits>
#include "hybm_big_mem.h"
#include "hybm_common_include.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_mem_slice.h"
#include "hybm_entity_tag_info.h"

namespace ock {
namespace mf {
class MemEntity {
public:
    virtual int32_t Initialize(const hybm_options *options) noexcept = 0;
    virtual void UnInitialize() noexcept = 0;

    virtual int32_t ReserveMemorySpace() noexcept = 0;
    virtual int32_t UnReserveMemorySpace() noexcept = 0;
    virtual void *GetReservedMemoryPtr(hybm_mem_type memType) noexcept = 0;

    virtual int32_t AllocLocalMemory(uint64_t size, hybm_mem_type mType, uint32_t flags,
                                     hybm_mem_slice_t &slice) noexcept = 0;
    virtual int32_t RegisterLocalMemory(const void *ptr, uint64_t size, uint32_t flags,
                                        hybm_mem_slice_t &slice) noexcept = 0;
    virtual int32_t FreeLocalMemory(hybm_mem_slice_t slice, uint32_t flags) noexcept = 0;

    virtual int32_t ExportExchangeInfo(ExchangeInfoWriter &desc, uint32_t flags) noexcept = 0;
    virtual int32_t ExportExchangeInfo(hybm_mem_slice_t slice, ExchangeInfoWriter &desc, uint32_t flags) noexcept = 0;
    virtual int32_t ImportExchangeInfo(const ExchangeInfoReader desc[], uint32_t count, void *addresses[],
                                       uint32_t flags) noexcept = 0;
    virtual int32_t ImportEntityExchangeInfo(const ExchangeInfoReader desc[], uint32_t count,
                                             uint32_t flags) noexcept = 0;
    virtual int32_t GetExportSliceInfoSize(size_t &size) noexcept = 0;
    virtual int32_t RemoveImported(const std::vector<uint32_t> &ranks) noexcept = 0;

    virtual int32_t SetExtraContext(const void *context, uint32_t size) noexcept = 0;

    virtual void Unmap() noexcept = 0;
    virtual int32_t Mmap() noexcept = 0;
    virtual bool SdmaReaches(uint32_t remoteRank) const noexcept = 0;
    virtual hybm_data_op_type CanReachDataOperators(uint32_t remoteRank) const noexcept = 0;

    virtual bool CheckAddressInEntity(const void *ptr, uint64_t length) const noexcept = 0;
    virtual int32_t CopyData(hybm_copy_params &params, hybm_data_copy_direction direction, void *stream,
                             uint32_t flags) noexcept = 0;
    virtual int32_t BatchCopyData(hybm_batch_copy_params &params, hybm_data_copy_direction direction, void *stream,
                                  uint32_t flags) noexcept = 0;
    virtual int32_t Wait() noexcept = 0;

    virtual ~MemEntity() noexcept = default;

private:
    HybmEntityTagInfo tagInfo_;
};

using MemEntityPtr = std::shared_ptr<MemEntity>;
} // namespace mf
} // namespace ock

#endif // __MF_HYBRID_BM_H__
