/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_META_PROXY_IMPL_H
#define MEM_FABRIC_MMC_META_PROXY_IMPL_H

#include <glob.h>
#include "mmc_meta_manager.h"
#include "mmc_meta_service.h"
#include "mmc_msg_client_meta.h"

namespace ock {
namespace mmc {

class MmcMetaMgrProxyDefault : public MmcMetaMgrProxy {
public:
    MmcMetaMgrProxyDefault(MmcMemPoolInitInfo mmcMemPoolInitInfo, uint64_t defaultTtl)
    {
        metaMangerPtr_ = MmcMakeRef<MmcMetaManager>(mmcMemPoolInitInfo, defaultTtl);
    }

    /**
     * @brief Alloc the global memeory space and create the meta object
     * @param key          [in] key of the meta object
     * @param metaInfo     [out] the meta object created
     */

    Result Alloc(const AllocRequest &req, AllocResponse &resp)
    {
        MmcMemObjMetaPtr objMeta;
        Result ret = metaMangerPtr_->Alloc(req.key_, req.options_, objMeta);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Meta Alloc Fail!");
            return MMC_ERROR;
        }
        resp.numBlobs_ = objMeta->NumBlobs();
        resp.prot_ = objMeta->Prot();
        resp.priority_ = objMeta->Priority();
        resp.lease_ = objMeta->Lease();

        std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
        for (size_t i = 0; i < blobs.size(); i++) {
            resp.blobs_.push_back(blobs[i]->GetDesc());
        }

        // TODO: send a copy of the meta data to local service

        return MMC_OK;
    }

    Result UpdateState(const UpdateRequest &req, Response &resp)
    {

        // const std::string &key, const MmcLocation &loc, const BlobActionResult &actRet
        MmcLocation loc{req.rank_, req.mediaType_};
        Result ret = metaMangerPtr_->UpdateState(req.key_, loc, req.actionResult_);
        resp.ret_ = ret;
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Meta Update State Fail!");
            return MMC_ERROR;
        } else {
            return MMC_OK;
        }
    }

    Result Get(const GetRequest &req, AllocResponse &resp)
    {

        MmcMemObjMetaPtr objMeta;
        metaMangerPtr_->Get(req.key_, objMeta);
        resp.numBlobs_ = objMeta->NumBlobs();
        resp.prot_ = objMeta->Prot();
        resp.priority_ = objMeta->Priority();
        resp.lease_ = objMeta->Lease();

        std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
        for (size_t i = 0; i < blobs.size(); i++) {
            resp.blobs_.push_back(blobs[i]->GetDesc());
        }
        return MMC_OK;
    }

    Result Remove(const RemoveRequest &req, Response &resp)
    {

        MmcMemObjMetaPtr objMeta;
        Result ret = metaMangerPtr_->Remove(req.key_);
        resp.ret_ = ret;
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Remove failed for key: " << req.key_ << " error: " << ret);
            return MMC_ERROR;
        } else {
            return MMC_OK;
        }
    }

private:
    MmcMetaManagerPtr metaMangerPtr_;
};
using MmcMetaMgrProxyDefaultPtr = MmcRef<MmcMetaMgrProxyDefault>;

}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_META_PROXY_IMPL_H
