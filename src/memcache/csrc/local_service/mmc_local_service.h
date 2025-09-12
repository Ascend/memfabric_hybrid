/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_LOCAL_SERVICE_H
#define MEM_FABRIC_MMC_LOCAL_SERVICE_H

#include "mmc_common_includes.h"
#include "mmc_def.h"

namespace ock {
namespace mmc {
class MmcLocalService : public MmcReferable {
public:
    virtual Result Start(const mmc_local_service_config_t &config) = 0;

    virtual void Stop() = 0;

    virtual const std::string &Name() const = 0;

    virtual const mmc_local_service_config_t &Options() const = 0;
};
} // namespace mmc
} // namespace ock

#endif // MEM_FABRIC_MMC_LOCAL_SERVICE_H
