/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_META_SERVICE_DEFAULT_H
#define MEM_FABRIC_MMC_META_SERVICE_DEFAULT_H

#include "mmc_meta_net_server.h"
#include "mmc_meta_common.h"
#include "mmc_meta_service.h"
#include "mmc_def.h"
#include "mmc_global_allocator.h"

namespace ock {
namespace mmc {
class MmcMetaServiceDefault : public MmcMetaService {
public:
    explicit MmcMetaServiceDefault(const std::string &name) : name_(name) {}

    Result Start(const mmc_meta_service_config_t &options) override;

    void Stop() override;

    Result BmRegister(uint32_t rank, uint16_t mediaType, uint64_t bm, uint64_t capacity);

    const std::string &Name() const override;

    const mmc_meta_service_config_t &Options() const override;

    MmcMetaMgrProxyPtr GetMetaMgrProxy() const override;

private:
    MetaNetServerPtr metaNetServer_;
    MmcMetaMgrProxyPtr metaMgrProxy_;

    std::mutex mutex_;
    bool started_ = false;
    bool bmStart_ = false;
    std::string name_;
    uint32_t worldSize_;
    uint32_t registerRank_ = 0;
    MmcMemPoolInitInfo poolInitInfo_;
    mmc_meta_service_config_t options_;
};
inline const std::string &MmcMetaServiceDefault::Name() const
{
    return name_;
}

inline const mmc_meta_service_config_t &MmcMetaServiceDefault::Options() const
{
    return options_;
}

using MmcMetaServiceDefaultPtr = MmcRef<MmcMetaServiceDefault>;
}
}

#endif  //MEM_FABRIC_MMC_META_SERVICE_DEFAULT_H
