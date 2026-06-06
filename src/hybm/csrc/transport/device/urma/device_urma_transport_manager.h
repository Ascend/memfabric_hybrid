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

#ifndef MF_HYBRID_DEVICE_URMA_TRANSPORT_MANAGER_H
#define MF_HYBRID_DEVICE_URMA_TRANSPORT_MANAGER_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "dl_hcomm_api.h"
#include "hybm_types.h"
#include "hybm_transport_manager.h"
#include "load_kernel.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

constexpr uint32_t URMA_EXPORT_DESC_MAGIC = 0xA5FAB001U;
constexpr uint16_t URMA_EXPORT_DESC_VERSION = 1U;

using UrmaMemTag = uint64_t;
using HcommMemHandle = ock::mf::HcommMemHandle;
using HcommEndpointHandle = ock::mf::EndpointHandle;
using HcommChannelHandle = ock::mf::ChannelHandle;
using HcommThreadHandle = ock::mf::ThreadHandle;

enum UrmaProtocol {
    RESERVED = -1, /* 保留协议类型 */
    HCCS = 0,      /* HCCS协议 */
    ROCE = 1,      /* RDMA over Converged Ethernet */
    PCIE = 2,      /* PCIe协议 */
    SIO = 3,       /* SIO协议 */
    UBC_CTP = 4,   /* 华为统一总线UBC_CTP */
    UBC_TP = 5,    /* 华为统一总线UBC_TP */
    UB_MEM = 6,    /* UB_MEM协议 */
    UBOE = 7,      /* UBoE协议 */
};

enum UrmaMemoryType : uint16_t {
    HOST_DRAM = 0,
    DEVICE_HBM = 1,
    INVALID_BUTT = 2,
};

inline std::ostream &operator<<(std::ostream &os, UrmaMemoryType obj)
{
    switch (obj) {
        case HOST_DRAM:
            return os << "DRAM";
        case DEVICE_HBM:
            return os << "DEVICE";
        case INVALID_BUTT:
            return os << "BUTT";
        default:
            return os << "UNKNOWN(" << static_cast<uint16_t>(obj) << ")";
    }
}

struct UrmaEndpointDesc {
    uint32_t devPhyId{0};
    uint32_t superDevId{0};
    uint32_t serverIdx{0};
    uint32_t superPodIdx{0};
    UrmaProtocol protocol{UrmaProtocol::RESERVED};
    CommAddrType type{COMM_ADDR_TYPE_RESERVED};
    uint8_t raws[36]{}; // CommAddr.raws
};

struct UrmaCommMem {
    uint64_t addr{0};
    uint64_t size{0};
    UrmaMemoryType type{UrmaMemoryType::INVALID_BUTT};
};

struct UrmaLocalMr {
    UrmaCommMem mem{};
    HcommMemHandle hcommMem{nullptr};
};


struct UrmaExportDesc {
    uint32_t magic{URMA_EXPORT_DESC_MAGIC};
    uint16_t version{URMA_EXPORT_DESC_VERSION};
    uint16_t headerSize{0};
    UrmaMemoryType memoryType{UrmaMemoryType::INVALID_BUTT};
    UrmaMemTag memTag{0};
    uint64_t addr{0};
    uint64_t size{0};
    uint32_t hcommDescLen{0};
};

struct UrmaEndpointEntity;
using UrmaEndpointHandle = std::shared_ptr<UrmaEndpointEntity>;

class UrmaManagerTransport final {
public:
    UrmaManagerTransport() = default;

    UrmaEndpointHandle CreateEndpoint(const UrmaEndpointDesc &desc) const;

    Result HcommMemReg(const UrmaEndpointHandle &endpoint, UrmaMemTag memTag, const UrmaCommMem &mem,
                       HcommMemHandle *memHandle);

    Result HcommMemUnreg(const UrmaEndpointHandle &endpoint, HcommMemHandle memHandle);

    Result HcommMemExport(const UrmaEndpointHandle &endpoint, HcommMemHandle memHandle, const uint8_t **memDesc,
                          uint32_t *memDescLen);

    Result HcommMemImport(const UrmaEndpointHandle &endpoint, const uint8_t *memDesc, uint32_t descLen,
                          UrmaCommMem *commMem);

    Result HcommMemUnimport(const UrmaEndpointHandle &endpoint, const uint8_t *memDesc, uint32_t descLen);
};

class DeviceUrmaTransportManager final : public transport::TransportManager {
public:
    DeviceUrmaTransportManager() = default;

    ~DeviceUrmaTransportManager() override;

    Result OpenDevice(const TransportOptions &options) override;

    Result CloseDevice() override;

    Result RegisterMemoryRegion(const TransportMemoryRegion &mr) override;

    Result UnregisterMemoryRegion(uint64_t addr) override;

    bool QueryHasRegistered(uint64_t addr, uint64_t size) override;

    Result QueryMemoryKey(uint64_t addr, TransportMemoryKey &key) override;

    void UpdateMemoryKey(TransportMemoryKey &key, void *addr) override;

    Result Prepare(const HybmTransPrepareOptions &options) override;

    Result RemoveRanks(const std::vector<uint32_t> &removedRanks) override;

    Result Connect() override;

    Result AsyncConnect() override;

    Result WaitForConnected(int64_t timeoutNs) override;

    Result UpdateRankOptions(const HybmTransPrepareOptions &options) override;

    const std::string &GetNic() const override;

    const TransportPrivateData GetPrivateData() const override;

    Result ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result Synchronize(uint32_t rankId) override;

    Result WriteRemoteBatchAsync(uint32_t rankId, const CopyDescriptor &descriptor) override;

    Result ReadRemoteBatchAsync(uint32_t rankId, const CopyDescriptor &descriptor) override;

private:
    struct LocalRegistration {
        TransportMemoryRegion mr{};
        HcommMemHandle handle{nullptr};
        UrmaMemTag memTag{0};
        uint32_t refCount{0};
        std::unordered_map<uint32_t, HcommMemHandle> peerHandles{};
    };

    struct RemoteRegistration {
        uint64_t addr{0};
        uint64_t size{0};
        uint64_t memTag{0};
        std::vector<uint8_t> descBytes{};
        UrmaCommMem view{};
    };

    struct RemoteRankState {
        // 本 rank 为了和该 peer 通信而创建的本地 endpoint，按 peer 存放在 remoteRanks_[peerRank] 里
        UrmaEndpointHandle localEndpoint{nullptr};
        UrmaEndpointDesc localEndpointDesc{};
        UrmaEndpointDesc remoteEndpointDesc{};
        bool hasEndpointDesc{false};
        // 本 rank 侧用于与该 peer 通信的 channel 描述符
        std::vector<HcommChannelDesc> channelDescs{};
        // 本 rank 侧用于与该 peer 通信的 channel handle
        std::vector<HcommChannelHandle> channels{};
        // 本 rank 侧用于与该 peer 通信的 thread
        HcommThreadHandle thread{0};
        uint32_t pendingOps{0};
        std::vector<RemoteRegistration> imports{};
    };

    Result EnsureOpenLocked() const;
    struct RemoteMemKey {
        uint64_t addr;
        uint64_t size;
    };

    Result EnsurePeerThreadLocked(RemoteRankState &state);
    Result EnsureLocalChannelsForPeerLocked(RemoteRankState &state);

    Result ImportRemoteMemKeysLocked(uint32_t peerRank, RemoteRankState &state,
                                     const std::vector<TransportMemoryKey> &memKeys);

    Result RemoveRankLocked(uint32_t rankId);
    // Device kernel launch helpers (moved from removed HcomUrmaTransportAdapter)
    struct DeviceTransferBuffers {
        void *dstList{nullptr};
        void *srcList{nullptr};
        void *lenList{nullptr};
    };
    Result EnsureDeviceKernelLoadedLocked();
    aclrtFuncHandle GetDeviceKernelFunc(bool isRead) const;
    Result LaunchDeviceKernelBatch(HcommThreadHandle thread, bool isRead, HcommChannelHandle channel,
                                   const std::vector<uint64_t> &localAddrs,
                                   const std::vector<uint64_t> &remoteAddrs,
                                   const std::vector<uint64_t> &sizes);
    Result SynchronizeDeviceKernelStream();
    void ReleaseDeviceTransferBuffers(DeviceTransferBuffers &buffers);
    Result AddBatchPendingDeviceBuffers(DeviceTransferBuffers &buffers);
    void FreeBatchPendingDeviceBuffers();

    Result RemoteIo(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size, bool write);
    Result RemoteIoBatch(uint32_t rankId, const CopyDescriptor &descriptor, bool write);
    Result FindLocalRegistrationLocked(uint64_t addr, uint64_t size, LocalRegistration *registration) const;
    Result FindRemoteRegistrationLocked(uint32_t rankId, uint64_t addr, uint64_t size,
                                        RemoteRegistration *registration) const;

    mutable std::mutex mutex_{};
    bool opened_{false};
    uint32_t rankId_{0};
    uint32_t rankCount_{0};
    uint32_t userDeviceId_{0};
    uint32_t logicDeviceId_{0};
    uint32_t phyDeviceId_{0};
    uint32_t sdid_{0};
    uint32_t serverId_{0};
    uint32_t superPodId_{0};
    TransportOptions options_{};
    UrmaManagerTransport manager_;
    UrmaEndpointHandle localEndpoint_{nullptr};
    UrmaEndpointDesc localEndpointDesc_{};
    std::map<uint64_t, LocalRegistration> localRegistrations_{};
    // Device kernel launch state (moved from removed HcomUrmaTransportAdapter)
    std::mutex deviceKernelMutex_{};
    bool deviceKernelLoaded_{false};
    aclrtBinHandle deviceKernelHandle_{nullptr};
    DeviceFuncHandles deviceFuncHandles_{};
    // NOTE: pendingBatchBuffers_ is a GLOBAL list shared by all ranks/channels.
    //   SynchronizeDeviceKernelStream() drains ALL pending batch buffers on success.
    std::mutex pendingBatchMutex_{};
    std::vector<DeviceTransferBuffers> pendingBatchBuffers_{};
    std::unordered_map<uint32_t, RemoteRankState> remoteRanks_{};
};

} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock

#endif // MF_HYBRID_DEVICE_URMA_TRANSPORT_MANAGER_H
