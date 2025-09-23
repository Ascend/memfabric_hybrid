/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MF_HYBRID_DEVICE_QP_MANAGER_H
#define MF_HYBRID_DEVICE_QP_MANAGER_H

#include <netinet/in.h>
#include <cstdint>
#include <unordered_map>
#include "hybm_def.h"
#include "device_rdma_common.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

class DeviceQpManager {
public:
    DeviceQpManager(uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet,
                    hybm_role_type role) noexcept;
    virtual ~DeviceQpManager() = default;

    virtual int SetRemoteRankInfo(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks) noexcept = 0;
    virtual int SetLocalMemories(const MemoryRegionMap &mrs) noexcept = 0;
    virtual int Startup(void *rdma) noexcept = 0;
    virtual void Shutdown() noexcept = 0;
    virtual int WaitingConnectionReady() noexcept;
    virtual const void *GetQpInfoAddress() const noexcept;
    virtual void *GetQpHandleWithRankId(uint32_t rankId) const noexcept = 0;

protected:
    void *CreateLocalSocket() noexcept;
    int CreateServerSocket() noexcept;
    void DestroyServerSocket() noexcept;

protected:
    const uint32_t deviceId_;
    const uint32_t rankId_;
    const uint32_t rankCount_;
    const hybm_role_type rankRole_;
    sockaddr_in deviceAddress_;
    void *serverSocketHandle_{nullptr};
};
}
}
}
}

#endif  // MF_HYBRID_DEVICE_QP_MANAGER_H
