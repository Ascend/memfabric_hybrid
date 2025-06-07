/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_COMMON_INCLUDE_H
#define MEM_FABRIC_HYBRID_HYBM_COMMON_INCLUDE_H

#include <map>
#include <mutex>
#include <vector>

#include "hybm_big_mem.h"
#include "hybm_define.h"
#include "hybm_functions.h"
#include "hybm_logger.h"
#include "hybm_types.h"

int32_t HybmGetInitDeviceId();

bool HybmHasInited();

#endif // MEM_FABRIC_HYBRID_HYBM_COMMON_INCLUDE_H
