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

#ifndef MF_HYBRID_COMPOSE_TRANSPORT_MANAGER_H
#define MF_HYBRID_COMPOSE_TRANSPORT_MANAGER_H

#include "hybm_transport_manager.h"

#include <mutex>

namespace ock {
namespace mf {
namespace transport {

struct ComposeMemoryRegion {
    uint64_t addr = 0;
    uint64_t size = 0;
    TransportType type = TT_BUTT;

    ComposeMemoryRegion(const uint64_t addr, const uint64_t size, TransportType type)
        : addr{addr}, size{size}, type{type}
    {}
};

class ComposeTransportManager : public TransportManager {
public:
    Result OpenDevice(const TransportOptions &options) override;

    Result CloseDevice() override;

    Result RegisterMemoryRegion(const TransportMemoryRegion &mr) override;

    Result UnregisterMemoryRegion(uint64_t addr) override;

    bool QueryHasRegistered(uint64_t addr, uint64_t size) override;

    Result QueryMemoryKey(uint64_t addr, TransportMemoryKey &key) override;

    Result ParseMemoryKey(const TransportMemoryKey &key, uint64_t &addr, uint64_t &size) override;

    Result Prepare(const HybmTransPrepareOptions &options) override;

    Result RemoveRanks(const std::vector<uint32_t> &removedRanks) override;

    Result Connect() override;

    Result AsyncConnect() override;

    Result UpdateRankOptions(const HybmTransPrepareOptions &options) override;

    Result WaitForConnected(int64_t timeoutNs) override;

    const std::string &GetNic() const override;

    Result ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result Synchronize(uint32_t rankId) override;

private:
    Result OpenHostTransport(const TransportOptions &options);

    Result OpenDeviceTransport(const TransportOptions &options);
    static TransportType GetTransportTypeFromFlag(uint32_t flags);
    std::shared_ptr<TransportManager> GetTransportFromType(TransportType type);
    std::shared_ptr<TransportManager> GetTransportFromAddress(uint64_t addr);
    static void GetHostPrepareOptions(const HybmTransPrepareOptions &param, HybmTransPrepareOptions &hostOptions);
    static void GetDevicePrepareOptions(const HybmTransPrepareOptions &param, HybmTransPrepareOptions &DeviceOptions);

private:
    std::shared_ptr<TransportManager> deviceTransportManager_{nullptr};
    std::shared_ptr<TransportManager> hostTransportManager_{nullptr};

    std::string nicInfo_;
    std::mutex mrsMutex_;
    std::unordered_map<uint64_t, ComposeMemoryRegion> mrs_;
};
} // namespace transport
} // namespace mf
} // namespace ock
#endif // MF_HYBRID_COMPOSE_TRANSPORT_MANAGER_H