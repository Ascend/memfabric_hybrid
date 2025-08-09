/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_DEFAULT_TRANSPORT_MANAGER_H
#define MF_HYBRID_HYBM_DEFAULT_TRANSPORT_MANAGER_H

#include "hybm_transport_manager.h"

namespace ock {
namespace mf {
namespace transport {

class DefaultTransportManager : public TransportManager {
public:
    static std::shared_ptr<DefaultTransportManager> GetInstance()
    {
        static auto instance = std::make_shared<DefaultTransportManager>();
        return instance;
    }

    Result OpenDevice(const TransportOptions &options) override
    {
        return BM_OK;
    }

    Result CloseDevice() override
    {
        return BM_OK;
    }

    Result RegisterMemoryRegion(const TransportMemoryRegion &mr) override
    {
        return BM_OK;
    }

    Result UnregisterMemoryRegion(uint64_t addr) override
    {
        return BM_OK;
    }

    Result QueryMemoryKey(uint64_t addr, TransportMemoryKey &key) override
    {
        return BM_OK;
    }

    Result Prepare(const HybmTransPrepareOptions &parma) override
    {
        return BM_OK;
    }

    Result Connect() override
    {
        return BM_OK;
    }

    Result AsyncConnect() override
    {
        return BM_OK;
    }

    Result WaitForConnected(int64_t timeoutNs) override
    {
        return BM_OK;
    }

    Result UpdateRankOptions(const HybmTransPrepareOptions &param) override
    {
        return BM_OK;
    }

    const std::string &GetNic() const override
    {
        static std::string emptyNic;
        return emptyNic;
    }

    Result ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override
    {
        return BM_OK;
    }

    Result WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override
    {
        return BM_OK;
    }
};
}
}
}

#endif // MF_HYBRID_HYBM_DEFAULT_TRANSPORT_MANAGER_H
