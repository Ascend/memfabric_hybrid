/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#ifndef SMEM_MMC_META_NET_SERVER_H
#define SMEM_MMC_META_NET_SERVER_H
#include "mmc_meta_common.h"
#include "mmc_net_engine.h"
#include "mmc_meta_mgr_proxy.h"

namespace ock {
namespace mmc {
class MetaNetServer : public MmcReferable {
public:
    explicit MetaNetServer(const MmcMetaServicePtr &metaService, const std::string inputName = "");

    ~MetaNetServer() override;

    Result Start();

    void Stop();

    template <typename REQ, typename RESP>
    Result SyncCall(uint16_t randId, const REQ &req, RESP &resp, int32_t timeoutInSecond)
    {
        return engine_->Call(randId, req.msgId, req, resp, timeoutInSecond);
    }

private:
    Result HandleNewLink(const NetLinkPtr &link);

    Result HandleLinkBroken(const NetLinkPtr &link);
    /* message handle function */
    Result HandleAlloc(const NetContextPtr &context);

    Result HandleBmRegister(const NetContextPtr &context);

    Result HandleBmUnregister(const NetContextPtr &context);

    Result HandlePing(const NetContextPtr &context);

    Result HandleUpdate(const NetContextPtr &context);

    Result HandleGet(const NetContextPtr &context);

    Result HandleBatchGet(const NetContextPtr &context);

    Result HandleRemove(const NetContextPtr &context);

    Result HandleIsExist(const NetContextPtr &context);

    Result HandleBatchIsExist(const NetContextPtr &context);

    Result HandleBatchRemove(const NetContextPtr &context);

    Result HandleQuery(const NetContextPtr &context);

    Result HandleBatchQuery(const NetContextPtr &context);

private:
    NetEnginePtr engine_;
    MmcMetaServicePtr metaService_;

    /* not hot used variables */
    std::mutex mutex_;
    bool started_ = false;
    std::string name_;
};
using MetaNetServerPtr = MmcRef<MetaNetServer>;
}
}
#endif  // SMEM_MMC_META_NET_SERVER_H
