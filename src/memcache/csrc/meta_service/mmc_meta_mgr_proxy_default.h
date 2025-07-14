/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_META_PROXY_IMPL_H
#define MEM_FABRIC_MMC_META_PROXY_IMPL_H

#include "mmc_meta_manager.h"
#include "mmc_meta_service.h"
#include "mmc_msg_client_meta.h"
#include "mmc_meta_net_server.h"
#include <glob.h>

namespace ock {
namespace mmc {

class MmcMetaMgrProxyDefault : public MmcMetaMgrProxy {
public:
    MmcMetaMgrProxyDefault(uint64_t defaultTtl, MetaNetServerPtr &netServerPtr)
                           : netServerPtr_(netServerPtr)
    {
        metaMangerPtr_ = MmcMakeRef<MmcMetaManager>(defaultTtl);
    }

    ~MmcMetaMgrProxyDefault() override = default;

    Result Alloc(const AllocRequest &req, AllocResponse &resp) override;

    Result UpdateState(const UpdateRequest &req, Response &resp) override;

    Result Get(const GetRequest &req, AllocResponse &resp) override;

    Result Remove(const RemoveRequest &req, Response &resp) override;

    Result BatchRemove(const BatchRemoveRequest &req, BatchRemoveResponse &resp);

    Result Mount(const MmcLocation &loc, const MmcLocalMemlInitInfo &localMemInitInfo) override
    {
        return metaMangerPtr_->Mount(loc, localMemInitInfo);
    }

    Result Unmount(const MmcLocation &loc) override
    {
        return metaMangerPtr_->Unmount(loc);
    }

    Result ExistKey(const IsExistRequest &req, Response &resp) override
    {
        resp.ret_ = metaMangerPtr_->ExistKey(req.key_);
        return resp.ret_;
    }

    Result BatchExistKey(const BatchIsExistRequest &req, BatchIsExistResponse &resp) override
    {
        std::vector<Result> results;
        resp.ret_ = metaMangerPtr_->BatchExistKey(req.keys_, results);
        resp.results_ = results;
        return resp.ret_;
    }
private:
    MmcMetaManagerPtr metaMangerPtr_;
    MetaNetServerPtr netServerPtr_;
    const int32_t timeOut_ = 60;
};
using MmcMetaMgrProxyDefaultPtr = MmcRef<MmcMetaMgrProxyDefault>;

}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_META_PROXY_IMPL_H
