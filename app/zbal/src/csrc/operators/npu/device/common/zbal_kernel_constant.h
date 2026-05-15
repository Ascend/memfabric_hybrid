/*
Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
ZBAL is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
     http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
*/
#ifndef ZBAL_KERNEL_COMM_ARGS_H
#define ZBAL_KERNEL_COMM_ARGS_H

#include "zbal_defines.h"

namespace Moe {

constexpr int PING_PONG_SIZE = 2;
constexpr int UB_ALIGN = 32;
constexpr uint32_t STATE_OFFSET = 32U;
constexpr uint32_t ADDR_OFFSET = 32U;

enum MetaType : int { STATE = 0, ADDR = 1, FLAG = 2 };

// meta:  |-- sizeForCommGroupInfo --|-- sizeForParam --|-- sizeForExchangeAddress --|
//                                                      |- state -|- addr -|- flag -|
// 1 MB meta space is reserved, and reverse offset 50 KB is used to write the cleared synchronization flag.
constexpr uint64_t KB_SIZE = 1024UL;
constexpr uint64_t META_FLAG_R_OFFSET = 50 * KB_SIZE;

// cycle num is converted into a fixed base unit of time, set at 50
constexpr uint64_t CYCLE_TO_TIME = zbal::ZBAL_CYCLE_UNIT;

constexpr uint64_t TIMEOUT_DETECTION_THRESHOLD = 5000000000UL;

} // namespace Moe

#endif // ZBAL_KERNEL_UTILS_H