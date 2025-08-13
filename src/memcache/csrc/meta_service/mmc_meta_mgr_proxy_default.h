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
    explicit MmcMetaMgrProxyDefault(MetaNetServerPtr &netServerPtr) : netServerPtr_(netServerPtr) {}

    ~MmcMetaMgrProxyDefault() override = default;

    Result Start(uint64_t defaultTtl, uint16_t evictThresholdHigh, uint16_t evictThresholdLow) override
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (started_) {
            MMC_LOG_INFO("MmcMetaMgrProxyDefault already started");
            return MMC_OK;
        }
        metaMangerPtr_ = MmcMakeRef<MmcMetaManager>(defaultTtl, evictThresholdHigh, evictThresholdLow);
        if (metaMangerPtr_ == nullptr) {
            MMC_LOG_ERROR("new object failed, probably out of memory");
            return MMC_NEW_OBJECT_FAILED;
        }
        MMC_RETURN_ERROR(metaMangerPtr_->Start(), "MmcMetaManager start failed");
        started_ = true;
        return MMC_OK;
    }

    void Stop() override
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!started_) {
            MMC_LOG_WARN("MmcMetaMgrProxyDefault has not been started");
            return;
        }
        metaMangerPtr_->Stop();
        started_ = false;
    }

    Result Alloc(const AllocRequest &req, AllocResponse &resp) override;

    Result BatchAlloc(const BatchAllocRequest &req, BatchAllocResponse &resp) override;

    Result UpdateState(const UpdateRequest &req, Response &resp) override;

    Result BatchUpdateState(const BatchUpdateRequest &req, BatchUpdateResponse &resp) override;

    Result Get(const GetRequest &req, AllocResponse &resp) override;

    Result BatchGet(const BatchGetRequest &req, BatchAllocResponse &resp) override;

    Result Remove(const RemoveRequest &req, Response &resp) override
    {
        return resp.ret_ = metaMangerPtr_->Remove(req.key_);
    }

    Result BatchRemove(const BatchRemoveRequest& req, BatchRemoveResponse& resp) override
    {
        resp.results_.reserve(req.keys_.size());
        for (const std::string& key : req.keys_) {
            resp.results_.emplace_back(metaMangerPtr_->Remove(key));
        }
        return MMC_OK;
    }

    Result Mount(const MmcLocation &loc, const MmcLocalMemlInitInfo &localMemInitInfo,
        std::map<std::string, MmcMemBlobDesc> &blobMap) override
    {
        return metaMangerPtr_->Mount(loc, localMemInitInfo, blobMap);
    }

    Result Unmount(const MmcLocation &loc) override
    {
        return metaMangerPtr_->Unmount(loc);
    }

    Result ExistKey(const IsExistRequest &req, IsExistResponse &resp) override
    {
        return resp.ret_ = metaMangerPtr_->ExistKey(req.key_);
    }

    Result BatchExistKey(const BatchIsExistRequest &req, BatchIsExistResponse &resp) override;

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
    std::mutex mutex_;
    bool started_ = false;
    MmcMetaManagerPtr metaMangerPtr_;
    MetaNetServerPtr netServerPtr_;
    const int32_t timeOut_ = 60;
};
using MmcMetaMgrProxyDefaultPtr = MmcRef<MmcMetaMgrProxyDefault>;

}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_META_PROXY_IMPL_H
