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
struct MemSlice {
    MemSlice(uint16_t index, MemType mType, MemPageTblType tbType, uint64_t va, uint64_t size)
        : magic_(Func::MakeObjectMagic(uint64_t(this))), index_(index), memType_(mType), memPageTblType_(tbType),
          vAddress_(va), size_(size)
    {}

    hybm_mem_slice_t ConvertToId() const noexcept;
    bool ValidateId(hybm_mem_slice_t slice) const;
    static uint64_t GetIndexFrom(hybm_mem_slice_t slice) noexcept;

    const uint64_t magic_ : 40;         /* to verify hybm_mem_slice_t ptr */
    const uint64_t index_ : 16;         /* id of mem slice  */
    const uint64_t memType_ : 4;        /* device or host memory */
    const uint64_t memPageTblType_ : 2; /* use CANN SVM page table or HyBM page table */
    const uint64_t vAddress_;           /* address of memory */
    const uint64_t size_;
};
} // namespace mf
} // namespace ock

#endif // MEM_FABRIC_HYBRID_HYBM_MEM_SLICE_H