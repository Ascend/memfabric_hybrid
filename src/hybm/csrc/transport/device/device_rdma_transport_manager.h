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

#ifndef MF_HYBRID_DEVICE_RDMA_TRANSPORT_MANAGER_H
#define MF_HYBRID_DEVICE_RDMA_TRANSPORT_MANAGER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <map>
#include <mutex>
#include <memory>
#include <unordered_map>
#include "hybm_define.h"
#include "hybm_stream_manager.h"
#include "hybm_stream_notify.h"
#include "hybm_transport_manager.h"
#include "device_chip_info.h"
#include "device_rdma_common.h"
#include "device_qp_manager.h"

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
    bool QueryHasRegistered(uint64_t addr, uint64_t size) override;
    Result QueryMemoryKey(uint64_t addr, TransportMemoryKey &key) override;
    Result ParseMemoryKey(const TransportMemoryKey &key, uint64_t &addr, uint64_t &size) override;
    Result Prepare(const HybmTransPrepareOptions &options) override;
    Result RemoveRanks(const std::vector<uint32_t> &removedRanks) override;
    Result Connect() override;
    Result AsyncConnect() override;
    Result WaitForConnected(int64_t timeoutNs) override;
    Result UpdateRankOptions(const HybmTransPrepareOptions &options) override;
    const std::string &GetNic() const override;
    const void *GetQpInfo() const override;
    Result ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;
    Result WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;
    Result ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;
    Result WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;
    Result Synchronize(uint32_t rankId) override;

private:
    static bool PrepareOpenDevice(uint32_t userId, uint32_t device, uint32_t rankCount, in_addr &deviceIp,
                                  void *&rdmaHandle);
    static bool OpenTsd(uint32_t deviceId, uint32_t rankCount);
    static bool RaInit(uint32_t deviceId);
    static bool RetireDeviceIp(uint32_t deviceId, in_addr &deviceIp);
    static bool RaRdevInit(uint32_t deviceId, in_addr deviceIp, void *&rdmaHandle);
    void ClearAllRegisterMRs();
    int CheckPrepareOptions(const HybmTransPrepareOptions &options);
    int RemoteIO(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size, bool write, bool sync);
    int CorrectHostRegWr(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size, send_wr_v2 &wr);
    int ConvertHccpMrInfo(const TransportMemoryRegion &mr, HccpMrInfo &info);
    void OptionsToRankMRs(const HybmTransPrepareOptions &options);
    Result WaitQpReady();
    int GetRegAddress(const MemoryRegionMap &map, uint64_t inputAddr, uint64_t size, bool isLocal, uint64_t &outputAddr,
                      uint32_t &mrKey) const;

private: // RDMA HOST STARS
    void ConstructSqeNoSinkModeForRdmaDbSendTask(const send_wr_rsp &rspInfo, rtStarsSqe_t &command, HybmStreamPtr st);
    uint64_t GetRoceDbAddrForRdmaDbSendTask();
    int32_t InitStreamNotifyBuf();
    int32_t Synchronize(void *qpHandle, uint32_t rankId);

private:
    static thread_local HybmStreamNotifyPtr notify_;
    RdmaNotifyInfo notifyInfo_;
    bool started_{false};
    uint32_t rankId_{0};
    uint32_t rankCount_{1};
    uint32_t deviceId_{0};
    hybm_role_type role_{HYBM_ROLE_PEER};
    in_addr deviceIp_{0};
    uint16_t devicePort_{0};
    void *rdmaHandle_{nullptr};
    std::string nicInfo_;
    MemoryRegionMap registerMRS_;
    std::vector<MemoryRegionMap> ranksMRs_;
    std::shared_ptr<DeviceQpManager> qpManager_;
    std::vector<std::pair<uint64_t, uint32_t>> notifyRemoteInfo_;
    std::shared_ptr<DeviceChipInfo> deviceChipInfo_;
    std::atomic<uint64_t> wrIdx_{0};

    ReadWriteLock lock_;
};
} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock

#endif // MF_HYBRID_DEVICE_RDMA_TRANSPORT_MANAGER_H
