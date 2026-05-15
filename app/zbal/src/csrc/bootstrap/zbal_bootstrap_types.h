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
#ifndef ZBAL_BOOTSTRAP_DEFINE_H
#define ZBAL_BOOTSTRAP_DEFINE_H

#include "zbal_common_includes.h"
#include "zbal_struct_helper.h"

namespace zbal {
namespace bootstrap {
/* types for bootstrap */
class Bootstrap;
using BootstrapPtr = ZRef<Bootstrap>;

/* types for mem bootstrap */
class MemBootstrap;
using MemBootstrapPtr = ZRef<MemBootstrap>;
class MemFabricBoostrap;
using MemFabricBoostrapPtr = ZRef<MemFabricBoostrap>;

enum MemBoostrapType : uint16_t {
    MBT_MEMFABRIC = 0,

    MBT_BUTT
};

struct MemBootstrapOptions {
    MemBoostrapType boostrapType = MBT_MEMFABRIC; /* memory init type */
    uint16_t deviceId = 0;                        /* device id */
    uint32_t rankCount = 0;                       /* total rank count */
    uint32_t rankId = 0;                          /* my rank id */
    uint64_t totalMemSize = 0;                    /* total memory size */
    uint32_t flags = 0;                           /* optional flags */
    uint32_t dataOperationType = 0;               /* data operation type MTE etc */
    std::string ipPort;                           /* SHM exchange ip port */

    friend std::ostream &operator<<(std::ostream &os, const MemBootstrapOptions &options)
    {
        os << "MemBootstrapOptions [boostrapType: " << options.boostrapType << ", deviceId: " << options.deviceId
           << ", rankCount: " << options.rankCount << ", rankId: " << options.rankId
           << ", totalMemSize: " << options.totalMemSize << ", flags: " << options.flags
           << ", dataOperationType: " << options.dataOperationType << ", ipPort: " << options.ipPort << "]";

        return os;
    }
};

struct MemBootstrapOutput {
    void *gvaDevice = nullptr;
    void *myGvaDevice = nullptr;
    uint64_t memorySizeDevice = 0;
    uint64_t memorySpaceSizeDevice = 0;

    friend std::ostream &operator<<(std::ostream &os, const MemBootstrapOutput &output)
    {
        os << "MemBootstrapOutput [deviceGva: " << output.gvaDevice << ", myDeviceGva: " << output.myGvaDevice
           << ", memorySizeDevice: " << output.memorySizeDevice
           << ", memorySpaceSizeDevice: " << output.memorySpaceSizeDevice << "]";

        return os;
    }
};

} // namespace bootstrap
} // namespace zbal

#endif // ZBAL_BOOTSTRAP_DEFINE_H
