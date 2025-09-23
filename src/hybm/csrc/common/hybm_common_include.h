/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_COMMON_INCLUDE_H
#define MEM_FABRIC_HYBRID_HYBM_COMMON_INCLUDE_H

#include <map>
#include <mutex>

#include "hybm_big_mem.h"
#include "hybm_define.h"
#include "hybm_functions.h"
#include "mf_file_util.h"
#include "hybm_logger.h"
#include "hybm_types.h"

int32_t HybmGetInitDeviceId();

bool HybmHasInited();

ock::mf::HybmGvaVersion HybmGetGvaVersion();

#endif // MEM_FABRIC_HYBRID_HYBM_COMMON_INCLUDE_H
