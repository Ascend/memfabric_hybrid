/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_META_SERVICE_H
#define MEM_FABRIC_MMC_META_SERVICE_H

#include "mmc_common_includes.h"
#include "mmc_def.h"
#include "mmc_meta_mgr_proxy.h"

namespace ock {
namespace mmc {
/**
 * Meta service is the global service of managing object meta,
 * i.e. key to bm management, include bm mount/unmount, space allocator, object lifecycle management
 *
 * This is the abstract class of Meta Service
 */
class MmcMetaService : public MmcReferable {
public:
    /**
     * @brief Start the meta service
     *
     * @param options      [in] option of meta service
     * @return 0 if successful
     */
    virtual Result Start(const mmc_meta_service_config_t &options) = 0;

    /**
     * @brief Stop the meta service
     */
    virtual void Stop() = 0;

    /**
     * @brief Get the name of meta service
     *
     * @return name
     */
    virtual const std::string &Name() const = 0;

    /**
     * @brief Get the meta service config of meta service
     *
     * @return config
     */
    virtual const mmc_meta_service_config_t &Options() const = 0;

    /**
     * @brief Get the meta manager proxy
     *
     * @return proxy manager
     */
    virtual const MmcMetaMgrProxyPtr& GetMetaMgrProxy() const = 0;
};
using MmcMetaServicePtr = MmcRef<MmcMetaService>;
}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_META_SERVICE_H