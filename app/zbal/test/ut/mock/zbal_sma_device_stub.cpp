/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "zbal_sma_device.h"

namespace zbal {
namespace sma {
namespace device {

void DeviceSMACachingAllocator::free_block(DeviceBlock *block,
                                           const std::shared_ptr<c10::GatheredContext> &context,
                                           uint8_t allocator_type)
{
}

void DeviceSMACachingAllocator::insert_events(DeviceBlock *block)
{
}

} // namespace device
} // namespace sma
} // namespace zbal
