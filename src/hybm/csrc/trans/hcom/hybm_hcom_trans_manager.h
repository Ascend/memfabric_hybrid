/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
*/
#ifndef MF_HYBRID_HYBM_HCOM_TRANS_MANAGER_H
#define MF_HYBRID_HYBM_HCOM_TRANS_MANAGER_H

#include <memory>

#include "hybm_trans_manager.h"
#include "hcom_service_c_define.h"

namespace ock {
namespace mf {

class HcomTransManager : public HybmTransManager {
public:
    static std::shared_ptr<HcomTransManager> GetInstance()
    {
        static auto instance = std::make_shared<HcomTransManager>();
        return instance;
    }

    Result OpenDevice(HybmTransOptions &options) override;
    Result CloseDevice() override;
    Result RegisterMemoryRegion(const HybmTransMemReg &input, MrInfo &output) override;
    Result UnregisterMemoryRegion(const void *addr) override;
    Result Prepare(const HybmTransPrepareOptions &options) override;
    Result Connect(const std::unordered_map<uint32_t, std::string> &nics, int mode) override;
    Result AsyncConnect(const std::unordered_map<uint32_t, std::string> &nics, int mode) override;
    Result WaitForConnected(int64_t timeoutNs) override;
    std::string GetNic() override;
    Result QueryKey(void *addr, HybmTransKey &key) override;
    Result RdmaOneSideTrans(const uint32_t &rankId, const uint64_t &lAddr, const uint64_t &rAddr,
                            const uint64_t &size, const bool &isGet) override;

private:
    Result CheckHybmTransOptions(HybmTransOptions &options);
    static Result TransRpcHcomNewEndPoint(Hcom_Channel newCh, uint64_t usrCtx, const char *payLoad);
    static Result TransRpcHcomEndPointBroken(Hcom_Channel ch, uint64_t usrCtx, const char *payLoad);
    static Result TransRpcHcomRequestReceived(Service_Context ctx, uint64_t usrCtx);
    static Result TransRpcHcomRequestPosted(Service_Context ctx, uint64_t usrCtx);
    static Result TransRpcHcomOneSideDone(Service_Context ctx, uint64_t usrCtx);

    Result ConnectHcomService(uint32_t rankId, const std::string &url);
    void RemoveChannel(Hcom_Channel ch);
    Result GetOneSideKeyByAddr(const uint64_t &addr, OneSideKey &key);

private:
    std::string localNic_;
    std::string localIp_;
    int32_t localPort_;
    Hcom_Service rpcService_{};
    std::unordered_map<uint64_t, MrInfo> mrInfoMap_;
    std::unordered_map<uint32_t, std::string> nicMap_;
    std::unordered_map<uint32_t, Hcom_Channel> connectMap_;
};
}
}
#endif //MF_HYBRID_HYBM_HCOM_TRANS_MANAGER_H
