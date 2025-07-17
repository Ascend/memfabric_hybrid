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

    Result BatchGet(const BatchGetRequest &req, BatchAllocResponse &resp) override;

    Result Remove(const RemoveRequest &req, Response &resp) override;

    Result BatchRemove(const BatchRemoveRequest &req, BatchRemoveResponse &resp) override;

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
        return metaMangerPtr_->ExistKey(req.key_, resp.ret_);
    }

    Result BatchExistKey(const BatchIsExistRequest &req, BatchIsExistResponse &resp) override
    {
        return metaMangerPtr_->BatchExistKey(req.keys_, resp.results_, resp.ret_);
    }

    Result Query(const QueryRequest &req, QueryResponse &resp) override
    {
        return metaMangerPtr_->Query(req.key_, resp.queryInfo_);
    }

    Result BatchQuery(const BatchQueryRequest &req, BatchQueryResponse &resp) override
    {
        for (const std::string& key : req.keys_) {
            MemObjQueryInfo queryInfo;
            metaMangerPtr_->Query(key, queryInfo);
            resp.batchQueryInfos_.push_back(queryInfo);
        }
        return MMC_OK;
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
