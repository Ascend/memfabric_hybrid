/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
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
    const sockaddr_in deviceAddress_;
    const hybm_role_type rankRole_;
    void *serverSocketHandle_{nullptr};
};
}
}
}
}

#endif  // MF_HYBRID_DEVICE_QP_MANAGER_H
