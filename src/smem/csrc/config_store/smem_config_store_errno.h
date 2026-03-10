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
#ifndef MEM_FABRIC_HYBRID_SMEM_STORE_DEFINE_H
#define MEM_FABRIC_HYBRID_SMEM_STORE_DEFINE_H

#include <cstdint>
#include "smem_types.h"

namespace ock {
namespace smem {

enum StoreErrorCode : int16_t {
    SUCCESS = SM_OK,
    ERROR = SM_ERROR,
    INVALID_MESSAGE = -400,
    INVALID_KEY = -401,
    NOT_EXIST = -404,
    RESTORE = -405,
    TIMEOUT = -601,
    IO_ERROR = -602
};

} // namespace smem
} // namespace ock

#endif // MEM_FABRIC_HYBRID_SMEM_STORE_DEFINE_H