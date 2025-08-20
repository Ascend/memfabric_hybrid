/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_HOST_HCOM_TRANSPORT_MANAGER_H
#define MF_HYBRID_HOST_HCOM_TRANSPORT_MANAGER_H

#include <mutex>
#include "hybm_transport_manager.h"
#include "hcom_service_c_define.h"

namespace ock {
namespace mf {
namespace transport {
namespace host {

struct HcomMemoryRegion {
    uint64_t addr;
    uint64_t size;
    TransportMemoryKey lKey;
    Service_MemoryRegion mr;
};

class HcomTransportManager : public TransportManager {
public:
    static std::shared_ptr<HcomTransportManager> GetInstance()
    {
        static auto instance = std::make_shared<HcomTransportManager>();
        return instance;
    }

    Result OpenDevice(const TransportOptions &options) override;

    Result CloseDevice() override;

    Result RegisterMemoryRegion(const TransportMemoryRegion &mr) override;

    Result UnregisterMemoryRegion(uint64_t addr) override;

    Result QueryMemoryKey(uint64_t addr, TransportMemoryKey &key) override;

    Result Prepare(const HybmTransPrepareOptions &parma) override;

    Result Connect() override;

    Result AsyncConnect() override;

    Result WaitForConnected(int64_t timeoutNs) override;

    Result UpdateRankOptions(const HybmTransPrepareOptions &param) override;

    const std::string &GetNic() const override;

    Result ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

private:
    Result CheckTransportOptions(const TransportOptions &options);

    static Result TransportRpcHcomNewEndPoint(Hcom_Channel newCh, uint64_t usrCtx, const char *payLoad);

    static Result TransportRpcHcomEndPointBroken(Hcom_Channel ch, uint64_t usrCtx, const char *payLoad);

    static Result TransportRpcHcomRequestReceived(Service_Context ctx, uint64_t usrCtx);

    static Result TransportRpcHcomRequestPosted(Service_Context ctx, uint64_t usrCtx);

    static Result TransportRpcHcomOneSideDone(Service_Context ctx, uint64_t usrCtx);

    Result ConnectHcomChannel(uint32_t rankId, const std::string &url);

    void DisConnectHcomChannel(uint32_t rankId, Hcom_Channel ch);

    Result GetMemoryRegionByAddr(const uint32_t &rankId, const uint64_t &addr, HcomMemoryRegion &mr);

    Result UpdateRankMrInfos(const std::unordered_map<uint32_t, TransportRankPrepareInfo> &opt);

    Result UpdateRankConnectInfos(const std::unordered_map<uint32_t, TransportRankPrepareInfo> &options);

private:
    std::string localNic_{};
    std::string protocol{};
    std::string localIp_{};
    int32_t localPort_{-1};
    Hcom_Service rpcService_{0};
    uint32_t rankId_{UINT32_MAX};
    uint32_t rankCount_{0};
    std::vector<std::mutex> mrMutex_;
    std::vector<std::vector<HcomMemoryRegion>> mrs_;
    std::vector<std::mutex> channelMutex_;
    std::vector<std::string> nics_;
    std::vector<Hcom_Channel> channels_;
};
}
}
}
}

#endif  // MF_HYBRID_HOST_HCOM_TRANSPORT_MANAGER_H
