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
#ifndef MEM_FABRIC_HYBRID_HYBM_MEM_SLICE_H
#define MEM_FABRIC_HYBRID_HYBM_MEM_SLICE_H

#include "hybm_common_include.h"
#include "hybm_mem_common.h"

namespace ock {
namespace mf {

// Memory allocation method enumeration
enum class MemAllocMethod : uint8_t {
    MMAP = 0,         // Allocated via mmap
    HAL_MEM_ALLOC = 1 // Allocated via HalMemAlloc
};

struct MemSlice {
    MemSlice(uint32_t index, hybm_mem_type mType, MemPageTblType tbType, uint64_t gva, uint64_t lva, uint64_t size,
             MemAllocMethod allocMethod = MemAllocMethod::MMAP)
        : magic_(Func::MakeObjectMagic(uint64_t(this))), index_(index), memType_(mType), memPageTblType_(tbType),
          gva_(gva), vAddress_(lva), size_(size), allocMethod_(allocMethod)
    {}

    hybm_mem_slice_t ConvertToId() const noexcept;
    bool ValidateId(hybm_mem_slice_t slice) const;
    static uint64_t GetIndexFrom(hybm_mem_slice_t slice) noexcept;

    const uint64_t magic_ : 24;         /* to verify hybm_mem_slice_t ptr */
    const uint64_t index_ : 32;         /* id of mem slice  */
    const uint64_t memType_ : 4;        /* device or host memory */
    const uint64_t memPageTblType_ : 2; /* use CANN SVM page table or HyBM page table */
    const uint64_t gva_;                /* global virtual address of memory */
    const uint64_t vAddress_;           /* local address of memory: hostVa or deviceVa */
    const uint64_t size_;
    const MemAllocMethod allocMethod_;  /* memory allocation method: mmap or halMemAlloc */
};
} // namespace mf
} // namespace ock

#endif // MEM_FABRIC_HYBRID_HYBM_MEM_SLICE_H