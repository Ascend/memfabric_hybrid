/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_client.h"
#include "mmc_common_includes.h"
#include "mmc_client_default.h"

using namespace ock::mmc;
static MmcClientDefault *mmmcClientHandler = nullptr;

MMC_API int32_t mmcc_init(mmc_client_config_t *config)
{
    auto *pClientDefault = new (std::nothrow)MmcClientDefault("mmc_client");
    MMC_ASSERT_RETURN(pClientDefault != nullptr, MMC_NEW_OBJECT_FAILED);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(pClientDefault->Start(*config), pClientDefault->Name() << " init failed!");
    mmmcClientHandler = pClientDefault;
    return MMC_OK;
}

MMC_API void mmcc_uninit()
{
    MMC_ASSERT_RET_VOID(mmmcClientHandler != nullptr);
    mmmcClientHandler->Stop();
    mmmcClientHandler = nullptr;
    return;
}

MMC_API int32_t mmcc_put(const char *key, mmc_buffer *buf, uint32_t flags)
{
    MMC_ASSERT_RETURN(mmmcClientHandler != nullptr, MMC_CLIENT_NOT_INIT);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(mmmcClientHandler->Put(key, buf, flags),
                                    mmmcClientHandler->Name() << " put key " << key << " failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_get(const char *key, mmc_buffer *buf, uint32_t flags)
{
    MMC_ASSERT_RETURN(mmmcClientHandler != nullptr, MMC_CLIENT_NOT_INIT);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(mmmcClientHandler->Get(key, buf, flags),
                                    mmmcClientHandler->Name() << " gut key " << key << " failed!");
    return MMC_OK;
}

MMC_API mmc_location_t mmcc_get_location(const char *key, uint32_t flags)
{
    MMC_ASSERT_RETURN(mmmcClientHandler != nullptr, { 0 });
    return mmmcClientHandler->GetLocation(key, flags);
}

MMC_API int32_t mmcc_remove(const char *key, uint32_t flags)
{
    MMC_ASSERT_RETURN(mmmcClientHandler != nullptr, MMC_CLIENT_NOT_INIT);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(mmmcClientHandler->Remove(key, flags),
                                    mmmcClientHandler->Name() << " remove key " << key << " failed!");
    return MMC_OK;
}