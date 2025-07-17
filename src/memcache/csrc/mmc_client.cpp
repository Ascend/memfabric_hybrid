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
                                                                              << " get key " << key << " failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_query(const char *key, mmc_data_info *info, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(info != nullptr, "invalid param, info is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    MMC_LOG_ERROR_AND_RETURN_NOT_OK(gClientHandler->Query(key, *info, flags),
                                    gClientHandler->Name() << " query key " << key << " failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_batch_query(const char **keys, uint32_t keys_count, mmc_data_info *info, uint32_t flags)
{
    MMC_VALIDATE_RETURN(keys != nullptr, "invalid param, keys is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(keys_count != 0, "invalid param, keys_count is 0", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(info != nullptr, "invalid param, info is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    std::vector<std::string> keys_vector(keys, keys + keys_count);
    std::vector<mmc_data_info> info_vector;

    MMC_LOG_ERROR_AND_RETURN_NOT_OK(gClientHandler->BatchQuery(keys_vector, info_vector, flags),
                                    gClientHandler->Name() << " batch query failed!");
    if (info_vector.size() != keys_count) {
        MMC_LOG_ERROR("Batch query error!");
        return MMC_ERROR;
    }
    for (size_t i = 0; i < keys_count; ++i) {
        info[i] = info_vector[i];
    }

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

    bool result = false;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(gClientHandler->IsExist(key, result, flags), gClientHandler->Name() << " is_exist failed!");
    if (result == false) {
        return 1;  // not found
    }
    return MMC_OK;
}

MMC_API int32_t mmcc_batch_exist(const char **keys, const uint32_t keys_count, int32_t *exist_results, uint32_t flags)
{
    MMC_ASSERT_RETURN(gClientHandler != nullptr, MMC_CLIENT_NOT_INIT);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(keys_count != 0, "Got empty keys");

    std::vector<std::string> keys_vector(keys, keys + keys_count);
    std::vector<int32_t> exist_results_vector;

    bool result = false;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(gClientHandler->BatchIsExist(keys_vector, exist_results_vector, result, flags),
                                    gClientHandler->Name() << " batch_is_exist failed!");

    for (size_t i = 0; i < keys_count; ++i) {
        exist_results[i] = exist_results_vector[i];
    }
    if (result == false) {
        return 1;  // not found
    }
    return MMC_OK;
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

MMC_API int32_t mmcc_batch_get(const std::vector<std::string>& keys, std::vector<mmc_buffer>& bufs, uint32_t flags)
{
    MMC_VALIDATE_RETURN(!keys.empty(), "invalid param, keys is empty", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(bufs.size() == keys.size(), "invalid param, bufs size mismatch", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    Result result = gClientHandler->BatchGet(keys, bufs, flags);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(result != MMC_OK, gClientHandler->Name() << " batch_get failed!");
    return MMC_OK;
}
