/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_COMMON_INCLUDE_H
#define MEM_FABRIC_HYBRID_HYBM_COMMON_INCLUDE_H

#include <map>
#include <mutex>

#include "hybm_big_mem.h"
#include "hybm_define.h"
#include "hybm_functions.h"
#include "hybm_logger.h"
#include "hybm_types.h"

int32_t HybmGetInitDeviceId();

int32_t HybmGetInitedLogicDeviceId();

bool HybmHasInited();

bool HybmGvmHasInited();

#endif // MEM_FABRIC_HYBRID_HYBM_COMMON_INCLUDE_H
