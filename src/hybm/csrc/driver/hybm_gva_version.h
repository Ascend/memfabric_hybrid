/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
*/

#ifndef HYBM_GVA_VERSION_H
#define HYBM_GVA_VERSION_H

#include <cstdint>

#include "hybm_define.h"

namespace ock {
namespace mf {

HybmGvaVersion HybmGetGvaVersion();
int32_t HalGvaPrecheck();

} // namespace mf
} // namespace ock

#endif
