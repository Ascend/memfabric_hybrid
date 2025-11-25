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
#ifndef HYBM_HYBM_CMD_H
#define HYBM_HYBM_CMD_H

#include <string>

namespace ock {
namespace mf {
namespace drv {

void HybmInitialize(int deviceId, int fd) noexcept;

int HybmMapShareMemory(const char *name, void *expectAddr, uint64_t size, uint64_t flags) noexcept;

int HybmUnmapShareMemory(void *expectAddr, uint64_t flags) noexcept;

int HybmIoctlAllocAnddAdvice(uint64_t ptr, size_t size, uint32_t devid, uint32_t advise) noexcept;

}
}
}

#endif  // HYBM_HYBM_CMD_H
