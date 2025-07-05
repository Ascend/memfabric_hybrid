/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_LOCAL_SERVICE_DEFAULT_H
#define MEM_FABRIC_MMC_LOCAL_SERVICE_DEFAULT_H

#include "mmc_meta_net_client.h"
#include "mmc_local_common.h"
#include "mmc_local_service.h"
#include "mmc_def.h"
#include "smem.h"
#include "smem_bm.h"

namespace ock {
namespace mmc {
class MmcLocalServiceDefault : public MmcLocalService {
public:
    explicit MmcLocalServiceDefault(const std::string &name) : name_(name) {}

    ~MmcLocalServiceDefault() override;

    Result Start(const mmc_local_service_config_t &config) override;

    void Stop() override;

    Result InitBm(const mmc_local_service_config_t &config);

    const std::string &Name() const override;

    const mmc_local_service_config_t &Options() const override;

    template <typename REQ, typename RESP>
    Result SyncCallMeta(const REQ &req, RESP &resp, int16_t &userResult, int32_t timeoutInSecond)
    {
        return metaNetClient_->SyncCall(req, resp, userResult, timeoutInSecond);
    }

    inline MetaNetClientPtr GetMetaClient() const;
private:

    MetaNetClientPtr metaNetClient_;
    int32_t pid_ = 0;
    std::mutex mutex_;
    bool started_ = false;
    std::string name_;
    mmc_local_service_config_t options_;
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
