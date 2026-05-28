/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/

#ifndef MF_HYBRID_AIV_SDMA_TRANSPORT_MANAGER_H
#define MF_HYBRID_AIV_SDMA_TRANSPORT_MANAGER_H

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
#include "dl_op_api.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

constexpr int MAX_AIV_PER_NPU = 48;
constexpr int STARS_NOTIFY_ADDR_OFFSET(14 * 1024);

struct HostStreamInfo {
    uint64_t stream;
    uint64_t ctx;
    int32_t streamId;
    uint32_t sqId;
    uint32_t cqId;
    uint32_t logicCqId;
    uint64_t cqeAddr;
    int32_t devId;
    uint8_t reserved[20];
};

struct SdmaOpResInfo {
    uint64_t size;
    uint64_t streamsAddr;
    uint64_t workspaceAddr;
    uint8_t reserved[40];
};

class SdmaTransportManager : public TransportManager {
public:
    SdmaTransportManager() = default;
    ~SdmaTransportManager() override = default;

    Result OpenDevice(const TransportOptions &options) override;
    Result CloseDevice() override;

    bool QueryHasRegistered(uint64_t addr, uint64_t size) override
    {
        return true;
    }

    Result ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override
    {
        return BM_NOT_SUPPORTED;
    }

    Result RemoveRanks(const std::vector<uint32_t> &removedRanks) override
    {
        return BM_NOT_SUPPORTED;
    }

    Result WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override
    {
        return BM_NOT_SUPPORTED;
    }

    Result ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override
    {
        return BM_NOT_SUPPORTED;
    }

    Result WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override
    {
        return BM_NOT_SUPPORTED;
    }

    Result Synchronize(uint32_t rankId) override
    {
        return BM_NOT_SUPPORTED;
    }

    void UpdateMemoryKey(TransportMemoryKey &key, void *addr) override {}

    const TransportPrivateData GetPrivateData() const override
    {
        return TransportPrivateData();
    }

    Result RegisterMemoryRegion(const TransportMemoryRegion &) override
    {
        return BM_NOT_SUPPORTED;
    }

    Result UnregisterMemoryRegion(uint64_t) override
    {
        return BM_NOT_SUPPORTED;
    }

    Result QueryMemoryKey(uint64_t, TransportMemoryKey &) override
    {
        return BM_NOT_SUPPORTED;
    }

    Result Prepare(const HybmTransPrepareOptions &) override
    {
        return BM_NOT_SUPPORTED;
    }

    Result Connect() override
    {
        return BM_NOT_SUPPORTED;
    }

    Result AsyncConnect() override
    {
        return BM_NOT_SUPPORTED;
    }

    Result WaitForConnected(int64_t) override
    {
        return BM_NOT_SUPPORTED;
    }

    Result UpdateRankOptions(const HybmTransPrepareOptions &) override
    {
        return BM_NOT_SUPPORTED;
    }

    Result WriteRemoteBatchAsync(uint32_t rankId, const CopyDescriptor &descriptor)
    {
        return BM_NOT_SUPPORTED;
    }

    Result ReadRemoteBatchAsync(uint32_t rankId, const CopyDescriptor &descriptor)
    {
        return BM_NOT_SUPPORTED;
    }

    uint64_t GetSdmaWorkSpaceAddr() const override;

    const std::string &GetNic() const override
    {
        static const std::string emptyNic;
        return emptyNic;
    }

private:
    Result CreateNotifyIds();
    Result CreateStarsStreams(int32_t channel_num);
    Result MallocSdmaWorkspace(size_t workspace_size);
    Result CopyHostOpResToDevice();
    Result CreateAclTensor(const std::vector<uint64_t> &host_data, const std::vector<int64_t> &shape,
                           void **device_addr, aclDataType data_type, aclTensor **tensor);
    Result LaunchSdmaAicpuKernel(uint64_t streams_addr, uint64_t sdma_workspace_addr);

private:
    bool inited_{false};
    std::mutex mutex_;
    uint32_t rankId_{0};
    uint32_t rankCount_{1};
    uint32_t deviceId_{0};
    uint64_t sdmaWorkspaceAddr_;
    void *notifyArr_[MAX_AIV_PER_NPU];

    static SdmaOpResInfo opResInfo_;
    static void *opResInfoDevicePtr_;
    static std::vector<HostStreamInfo> streams_;
};
} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock

#endif // MF_HYBRID_AIV_SDMA_TRANSPORT_MANAGER_H
