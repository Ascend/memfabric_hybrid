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

#ifndef MEM_FABRIC_HYBRID_DEVMM_SVM_GVA_H
#define MEM_FABRIC_HYBRID_DEVMM_SVM_GVA_H

#include <cstddef>
#include <cstdint>

namespace ock {
namespace mf {
namespace drv {

const uint64_t GVA_GIANT_FLAG = (1ULL << 0);

int32_t HalGvaReserveMemory(uint64_t *address, size_t size, int32_t deviceId, uint64_t flags);

int32_t HalGvaUnreserveMemory(uint64_t address);

int32_t HalGvaAlloc(uint64_t address, size_t size, uint64_t flags);

int32_t HalGvaFree(uint64_t address, size_t size);

int32_t HalGvaOpen(uint64_t address, const char *name, size_t size, uint64_t flags);

int32_t HalGvaClose(uint64_t address, uint64_t flags);

}
}
}

#endif // MEM_FABRIC_HYBRID_DEVMM_SVM_GVA_H
