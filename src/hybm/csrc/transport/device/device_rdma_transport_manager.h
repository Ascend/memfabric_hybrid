/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_DEVICE_RDMA_TRANSPORT_MANAGER_H
#define MF_HYBRID_DEVICE_RDMA_TRANSPORT_MANAGER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <map>
#include <mutex>
#include <memory>

#include "hybm_define.h"
#include "hybm_transport_manager.h"
#include "device_rdma_common.h"
#include "device_rdma_connection_manager.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
class RdmaTransportManager : public TransportManager {
public:
    ~RdmaTransportManager() override;
    Result OpenDevice(const TransportOptions &options) override;
    Result CloseDevice() override;
    Result RegisterMemoryRegion(const TransportMemoryRegion &mr) override;
    Result UnregisterMemoryRegion(uint64_t addr) override;
    Result QueryMemoryKey(uint64_t addr, TransportMemoryKey &key) override;
    Result Prepare(const HybmTransPrepareOptions &options) override;
    Result Connect() override;
    Result AsyncConnect() override;
    Result WaitForConnected(int64_t timeoutNs) override;
    Result UpdateRankOptions(const HybmTransPrepareOptions &options) override;
    const std::string &GetNic() const override;
    const void *GetQpInfo() const override;
    Result ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;
    Result WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

private:
    static bool PrepareOpenDevice(uint32_t device, uint32_t rankCount, in_addr &deviceIp, void *&rdmaHandle);
    static bool OpenTsd(uint32_t deviceId, uint32_t rankCount);
    static bool RaInit(uint32_t deviceId);
    static bool RetireDeviceIp(uint32_t deviceId, in_addr &deviceIp);
    static bool RaRdevInit(uint32_t deviceId, in_addr deviceIp, void *&rdmaHandle);
    void ClearAllRegisterMRs();

private:
    uint32_t rankId_{0};
    uint32_t rankCount_{1};
    uint32_t deviceId_{0};
    in_addr deviceIp_{0};
    uint16_t devicePort_{0};
    void *rdmaHandle_{nullptr};
    static void *storedRdmaHandle_;
    static bool tsdOpened_;
    static bool raInitialized_;
    static bool deviceIpRetired_;
    std::string nicInfo_;
    std::mutex mrsMutex_;
    std::map<uint64_t, RegMemResult, std::greater<uint64_t>> registerMRS_;
    std::shared_ptr<RdmaConnectionManager> connectionManager_;
};
}
}
}
}

#endif  // MF_HYBRID_DEVICE_RDMA_TRANSPORT_MANAGER_H
