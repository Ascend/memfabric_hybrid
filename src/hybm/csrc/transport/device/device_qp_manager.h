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

#ifndef MF_HYBRID_DEVICE_QP_MANAGER_H
#define MF_HYBRID_DEVICE_QP_MANAGER_H

#include <netinet/in.h>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include "hybm_def.h"
#include "device_rdma_common.h"
#include "mf_rwlock.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

sockaddr_in Ip2Net(in_addr ip);

struct UserQpInfo {
    void *qpHandle{nullptr};
    std::atomic<uint32_t> ref{0};
    UserQpInfo() : qpHandle{nullptr}, ref{0} {}
    UserQpInfo& operator=(UserQpInfo&&) = delete;
    UserQpInfo(UserQpInfo&& ot) noexcept : qpHandle(ot.qpHandle), ref(ot.ref.load())
    {
        ot.qpHandle = nullptr;
        ot.ref = 0;
    }
};

struct ConnectionChannel {
    sockaddr_in remoteNet;
    void *socketHandle;
    void *socketFd{nullptr};
    void *qpHandle{nullptr};
    bool qpConnectCalled{false};
    int qpStatus{-1};

    ConnectionChannel() : remoteNet{}, socketHandle{nullptr} {}
    explicit ConnectionChannel(sockaddr_in net) : ConnectionChannel{std::move(net), nullptr} {}
    ConnectionChannel(sockaddr_in net, void *sock) : remoteNet{std::move(net)}, socketHandle{sock} {}
};

class DeviceQpManager {
public:
    DeviceQpManager(uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet,
                    hybm_role_type role) noexcept;
    virtual ~DeviceQpManager() = default;

    virtual int SetRemoteRankInfo(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks) noexcept = 0;
    virtual int RemoveRanks(const std::unordered_set<uint32_t> &ranks) noexcept;
    virtual int Startup(void *rdma) noexcept = 0;
    virtual void Shutdown() noexcept = 0;
    virtual int WaitingConnectionReady() noexcept;
    virtual const void *GetQpInfoAddress() const noexcept;
    virtual UserQpInfo *GetQpHandleWithRankId(uint32_t rankId) noexcept = 0;
    virtual void PutQpHandle(UserQpInfo *qp) const noexcept = 0;
    virtual bool CheckQpReady(const std::vector<uint32_t> &rankIds) const noexcept;

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
}  // namespace device
}  // namespace transport
}  // namespace mf
}  // namespace ock

#endif  // MF_HYBRID_DEVICE_QP_MANAGER_H