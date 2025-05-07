/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "smem_bm_entry_manager.h"
#include "smem_net_common.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {
SmemBmEntryManager &SmemBmEntryManager::Instance()
{
    static SmemBmEntryManager instance;
    return instance;
}

Result SmemBmEntryManager::Initialize(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId,
                                       uint16_t deviceId, smem_bm_config_t *config)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    if (inited_) {
        SM_LOG_WARN("smem bm manager has already initialized");
        return SM_OK;
    }

    SM_PARAM_VALIDATE(config == nullptr, "invalid param, config is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(configStoreIpPort == nullptr, "invalid param, ipPort is NULL", SM_INVALID_PARAM);

    UrlExtraction option;
    std::string url(configStoreIpPort);
    SM_ASSERT_RETURN(option.ExtractIpPortFromUrl(url) == SM_OK, SM_INVALID_PARAM);

    if (rankId == 0 && config->startConfigStore) {
        storeServer_ = ock::smem::StoreFactory::CreateStore(option.ip, option.port, true, 0);
        SM_ASSERT_RETURN(storeServer_ != nullptr, SM_ERROR);
    }
    storeClient_ = ock::smem::StoreFactory::CreateStore(option.ip, option.port, false,
                                                        static_cast<int32_t>(rankId), static_cast<int32_t>(config->initTimeout));
    SM_ASSERT_RETURN(storeClient_ != nullptr, SM_ERROR);

    config_ = *config;
    deviceId_ = deviceId;
    inited_ = true;
    return SM_OK;
}

}  // namespace smem
}  // namespace ock
