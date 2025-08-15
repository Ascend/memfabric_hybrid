/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_LOCAL_SERVICE_DEFAULT_H
#define MEM_FABRIC_MMC_LOCAL_SERVICE_DEFAULT_H

#include "mmc_meta_net_client.h"
#include "mmc_local_service.h"
#include "mmc_bm_proxy.h"
#include "mmc_blob_common.h"
#include "mmc_def.h"

namespace ock {
namespace mmc {
class MmcLocalServiceDefault : public MmcLocalService {
public:
    explicit MmcLocalServiceDefault(const std::string &name) : name_(name), options_() {}

    ~MmcLocalServiceDefault() override;

    Result Start(const mmc_local_service_config_t &config) override;

    void Stop() override;

    Result RegisterBm();

    Result BuildMeta();

    Result InitBm();

    Result DestroyBm();

    Result Replicate(const std::string &key, const MmcMemBlobDesc &blobDesc);

    Result ReplicateRemove(const std::string &key);

    Result CopyBlob(const MmcMemBlobDesc &src, const MmcMemBlobDesc &dst);

    const std::string &Name() const override;

    const mmc_local_service_config_t &Options() const override;

    template <typename REQ, typename RESP>
    Result SyncCallMeta(const REQ &req, RESP &resp, int32_t timeoutInSecond)
    {
        return metaNetClient_->SyncCall(req, resp, timeoutInSecond);
    }

    inline MetaNetClientPtr GetMetaClient() const;

private:
    MetaNetClientPtr metaNetClient_;
    MmcBmProxyPtr bmProxyPtr_;
    int32_t pid_ = 0;
    std::mutex mutex_;
    std::mutex blobMutex_;
    bool started_ = false;
    std::string name_;
    mmc_local_service_config_t options_;
    std::map<std::string, MmcMemBlobDesc> blobMap_;
    const int32_t blobRebuildSendMaxCount = 10240;
};

inline const std::string &MmcLocalServiceDefault::Name() const
{
    return name_;
}

inline const mmc_local_service_config_t &MmcLocalServiceDefault::Options() const
{
    return options_;
}

inline MetaNetClientPtr MmcLocalServiceDefault::GetMetaClient() const
{
    return (getpid() == pid_) ? metaNetClient_ : nullptr;
}
}
}

#endif  //MEM_FABRIC_MMC_LOCAL_SERVICE_DEFAULT_H
