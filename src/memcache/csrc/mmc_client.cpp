/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_client.h"
#include "mmc_common_includes.h"
#include "mmc_client_default.h"

using namespace ock::mmc;
static MmcClientDefault *gClientHandler = nullptr;

MMC_API int32_t mmcc_init(mmc_client_config_t *config)
{
    MMC_VALIDATE_RETURN(config != nullptr, "invalid param, config is null", MMC_INVALID_PARAM);

    auto pClientDefault = MmcMakeRef<MmcClientDefault>("mmc_client");
    if (pClientDefault == nullptr) {
        MMC_LOG_AND_SET_LAST_ERROR("new object failed, probably out of memory");
        return MMC_NEW_OBJECT_FAILED;
    }

    MMC_LOG_ERROR_AND_RETURN_NOT_OK(pClientDefault->Start(*config), pClientDefault->Name() << " init client failed");
    gClientHandler = pClientDefault.Get();
    /* increase ref count to avoid auto delete */
    gClientHandler->IncreaseRef();
    return MMC_OK;
}

MMC_API void mmcc_uninit()
{
    MMC_VALIDATE_RETURN_VOID(gClientHandler != nullptr, "client is not initialize");

    gClientHandler->Stop();
    /* decrease ref count to delete automatically */
    gClientHandler->DecreaseRef();
    gClientHandler = nullptr;
}

MMC_API int32_t mmcc_put(const char *key, mmc_buffer *buf, mmc_put_options options, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(buf != nullptr, "invalid param, buf is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    MMC_LOG_ERROR_AND_RETURN_NOT_OK(gClientHandler->Put(key, buf, options, flags),
                                    gClientHandler->Name() << " put key " << key << " failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_get(const char *key, mmc_buffer *buf, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(buf != nullptr, "invalid param, buf is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    MMC_LOG_ERROR_AND_RETURN_NOT_OK(gClientHandler->Get(key, buf, flags), gClientHandler->Name()
                                                                              << " gut key " << key << " failed!");
    return MMC_OK;
}

MMC_API mmc_location_t mmcc_get_location(const char *key, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", {});
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", {});

    return gClientHandler->GetLocation(key, flags);
}

MMC_API int32_t mmcc_remove(const char *key, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    MMC_LOG_ERROR_AND_RETURN_NOT_OK(gClientHandler->Remove(key, flags), gClientHandler->Name()
                                                                            << " remove key " << key << " failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_exist(const char *key, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    Result result = gClientHandler->IsExist(key, flags);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(result != MMC_OK && result != MMC_UNMATCHED_KEY, gClientHandler->Name()
                                                                                         << " is_exist failed!");
    return result == MMC_OK ? 0 : 1;
}

MMC_API int32_t mmcc_batch_exist(const std::vector<std::string> &keys, std::vector<Result> &exist_results,
                                 uint32_t flags)
{
    MMC_ASSERT_RETURN(gClientHandler != nullptr, MMC_CLIENT_NOT_INIT);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(keys.size() == 0, "Got empty keys");
    Result result = gClientHandler->BatchIsExist(keys, exist_results, flags);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(result != MMC_OK && result != MMC_UNMATCHED_KEY, gClientHandler->Name()
                                                                                         << " batch_is_exist failed!");
    return (result == MMC_OK) ? 0 : 1;
}

MMC_API int32_t mmcc_batch_remove(const std::vector<std::string>& keys, std::vector<Result>& remove_results, uint32_t flags)
{
    MMC_ASSERT_RETURN(gClientHandler != nullptr, MMC_CLIENT_NOT_INIT);
    if (keys.empty()) {
        MMC_LOG_ERROR("Got empty keys");
        return MMC_ERROR;
    }
    Result result = gClientHandler->BatchRemove(keys, remove_results, flags);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(result != MMC_OK,
                                    gClientHandler->Name() << " batch_remove failed!");
    return MMC_OK;
}
