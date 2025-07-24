/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
*/
#ifndef MF_HYBRID_HYBM_HCCP_TRANS_MANAGER_H
#define MF_HYBRID_HYBM_HCCP_TRANS_MANAGER_H

#include <memory>

#include "hybm_trans_manager.h"

namespace ock {
namespace mf {

class HccpTransManager : public HybmTransManager {
public:
    static std::shared_ptr<HccpTransManager> GetInstance()
    {
        static auto instance = std::make_shared<HccpTransManager>();
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
};
}
}
#endif // MF_HYBRID_HYBM_HCCP_TRANS_MANAGER_H
