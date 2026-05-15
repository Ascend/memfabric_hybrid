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
#ifndef ZBAL_STRUCT_DUMP_HELPER_H
#define ZBAL_STRUCT_DUMP_HELPER_H

#include "zbal_common_includes.h"

namespace zbal {
constexpr uint16_t COMM_META_SPACE_SIZE_MIN = 1024;     /* 1MB */
constexpr uint16_t COMM_META_SPACE_SIZE_DEFAULT = 1024; /* 1MB */
constexpr uint16_t COMM_META_SPACE_SIZE_MAX = 4096;     /* 4MB */
constexpr uint16_t COMM_GROUP_COUNT_CAP_MIN = 1;
constexpr uint16_t COMM_GROUP_COUNT_CAP_DEFAULT = 128;
constexpr uint16_t COMM_GROUP_COUNT_CAP_MAX = 256;
constexpr uint16_t COMM_GROUP_SYMBOL_SHIFT = 56;

static inline std::ostream &operator<<(std::ostream &os, const zbal_bootstrap_options_t &options)
{
    os << "zbal_bootstrap_options_t [flags: " << options.flags << ", bootstrap_type: " << options.btType
       << ", ipPort: " << ((options.ipPort == nullptr) ? "" : options.ipPort) << ", worldSize: " << options.worldSize
       << ", rankId: " << options.rankId << ", deviceId: " << options.deviceId
       << ", startConfigServer: " << options.startConfigServer << ", deviceMemorySize: " << options.deviceMemorySize
       << ", dataOperationType: " << options.dataOperationType << ", commMetaSpaceSize: " << options.commMetaSpaceSize
       << ", commGroupCap: " << options.commGroupCap << "]";

    return os;
}

static inline std::ostream &operator<<(std::ostream &os, const zbal_bootstrap_output_t &output)
{
    os << "zbal_bootstrap_output_t [deviceGva: " << output.deviceGva
       << ", createdDeviceMemorySpaceSize: " << output.createdDeviceMemorySpaceSize
       << ", allocatedDeviceMemorySize: " << output.allocatedDeviceMemorySize << ", myDeviceGva: " << output.myDeviceGva
       << ", myCommMetaDeviceGva: " << output.myCommMetaDeviceGva << ", metaSizeOfDevice: " << output.metaSizeOfDevice
       << ", mySMAGva: " << output.mySMAGva << ", myDeviceGva: " << output.myDeviceGva
       << ", smaSizeOfDevice: " << output.smaSizeOfDevice << "]";

    return os;
}

static inline std::ostream &operator<<(std::ostream &os, const zbal_allocator_options_t &options)
{
    os << "zbal_allocator_options_t [gva: " << options.gva << ", myGva: " << options.myGva << ", size: " << options.size
       << "]";

    return os;
}

static inline std::ostream &operator<<(std::ostream &os, const zbal_comm_options_t &options)
{
    os << "zbal_comm_options_t [name: " << (options.name != nullptr ? options.name : "")
       << ", backendType: " << options.backendType << ", flags: " << options.flags
       << ", isWorldGroup: " << options.isWorldGroup << ", groupSize: " << options.groupSize
       << ", groupRankId: " << options.groupRankId << "]";

    return os;
}

static inline std::ostream &operator<<(std::ostream &os, const zbal_comm_property_t &property)
{
    os << "zbal_comm_property_t [backendType: " << property.backendType << ", flags: " << property.flags
       << ", isWorldGroup: " << property.isWorldGroup << ", groupSize: " << property.groupSize
       << ", groupRankId: " << property.groupRankId << ", myGVA: " << property.myGVA
       << ", myMetaGVA: " << property.myMetaGVA << ", sizeOfMetaArea: " << property.sizeOfMetaArea
       << ", sizeOfMetaForAddressExchange: " << property.sizeOfMetaForAddressExchange
       << ", myMetaGVAForOpParam: " << property.myMetaGVAForOpParam
       << ", sizeOfMetaForOpParam: " << property.sizeOfMetaForOpParam << ", groupIndex: " << property.groupIndex << "]";

    return os;
}

static inline std::ostream &operator<<(std::ostream &os, const zbal_tensor_info_t &info)
{
    os << "zbal_tensor_info_t [data: " << info.data << ", dataType: " << info.dataType << ", dim: " << info.dim << "]";

    return os;
}
} // namespace zbal

#endif // ZBAL_STRUCT_DUMP_HELPER_H
