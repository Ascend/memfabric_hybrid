/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef HYBM_GVA_H
#define HYBM_GVA_H

#include <cstdint>

namespace ock {
namespace mf {

int32_t HybmGetInitedLogicDeviceId();
int32_t hybm_init_hbm_gva(uint16_t deviceId, uint64_t flags, uint64_t& baseAddress);

} // namespace mf
} // namespace ock

#endif
