/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_client.h"
#include "mmc_common_includes.h"
#include "mmc_client_default.h"

using namespace ock::mmc;
static MmcClientDefault *gClientHandler = nullptr;

namespace {
constexpr uint32_t MAX_BATCH_COUNT = 16384;
}

MMC_API int32_t mmcc_init(mmc_client_config_t *config)
{
    MMC_VALIDATE_RETURN(config != nullptr, "invalid param, config is null", MMC_INVALID_PARAM);

    auto pClientDefault = MmcMakeRef<MmcClientDefault>("mmc_client");
    if (pClientDefault == nullptr) {
        MMC_LOG_AND_SET_LAST_ERROR("new object failed, probably out of memory");
        return MMC_NEW_OBJECT_FAILED;
    }

    MMC_RETURN_ERROR(pClientDefault->Start(*config), pClientDefault->Name() << " init client failed");
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
    MMC_VALIDATE_RETURN(strlen(key) != 0, "invalid param, key's len equals 0", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) <= 256, "invalid param, key's len more than 256", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(buf != nullptr, "invalid param, buf is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN((void *)buf->addr != nullptr, "invalid param, buf addr is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    MMC_RETURN_ERROR(gClientHandler->Put(key, buf, options, flags),
                     gClientHandler->Name() << " put key " << key << " failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_get(const char *key, mmc_buffer *buf, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) != 0, "invalid param, key's len equals 0", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) <= 256, "invalid param, key's len more than 256", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(buf != nullptr, "invalid param, buf is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    MMC_RETURN_ERROR(gClientHandler->Get(key, buf, flags), gClientHandler->Name()
                                                                              << " get key " << key << " failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_query(const char *key, mmc_data_info *info, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) != 0, "invalid param, key's len equals 0", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) <= 256, "invalid param, key's len more than 256", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(info != nullptr, "invalid param, info is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    MMC_RETURN_ERROR(gClientHandler->Query(key, *info, flags),
                     gClientHandler->Name() << " query key " << key << " failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_batch_query(const char **keys, uint32_t keys_count, mmc_data_info *info, uint32_t flags)
{
    MMC_VALIDATE_RETURN(keys != nullptr, "invalid param, keys is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(keys_count != 0 && keys_count <= MAX_BATCH_COUNT, "invalid param, keys_count: "
                        << keys_count, MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(info != nullptr, "invalid param, info is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    std::vector<std::string> keys_vector;
    std::vector<mmc_data_info> info_vector;
    std::vector<size_t> invalids;
    keys_vector.reserve(keys_count);
    info_vector.reserve(keys_count);
    invalids.reserve(keys_count);

    // remove invalid keys
    for (size_t i = 0; i < keys_count; ++i) {
        if (keys[i] == nullptr || strlen(keys[i]) == 0 || strlen(keys[i]) > 256) {
            MMC_LOG_WARN("Get invalid key: \"" << keys[i] << "\" on idx [" << i << "]");
            invalids.emplace_back(i);
            continue;
        }
        keys_vector.emplace_back(keys[i]);
    }

    MMC_RETURN_ERROR(gClientHandler->BatchQuery(keys_vector, info_vector, flags),
                     gClientHandler->Name() << " batch query failed!");
    MMC_VALIDATE_RETURN(keys_count == info_vector.size() + invalids.size(),
                        "invalid results' size (" << info_vector.size() << "), should be keys_count ("
                        << keys_count << ") - invalid_keys' size (" << invalids.size() << ")", MMC_ERROR);

    invalids.emplace_back(-1);
    for (size_t i = 0, j = 0; i + j < keys_count; ++i) {
        if (i + j == invalids[j]) {
            info[i + j] = mmc_data_info{0, 0, 0, false};
        } else {
            info[i + j] = info_vector[i];
        }
    }
    return MMC_OK;
}

MMC_API mmc_location_t mmcc_get_location(const char *key, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", {});
    MMC_VALIDATE_RETURN(strlen(key) != 0, "invalid param, key's len equals 0", {});
    MMC_VALIDATE_RETURN(strlen(key) <= 256, "invalid param, key's len more than 256", {});
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", {});

    return gClientHandler->GetLocation(key, flags);
}

MMC_API int32_t mmcc_remove(const char *key, uint32_t flags)
{
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) != 0, "invalid param, key's len equals 0", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) <= 256, "invalid param, key's len more than 256", MMC_INVALID_PARAM);
    MMC_RETURN_ERROR(gClientHandler->Remove(key, flags), gClientHandler->Name() << " remove key " << key << " failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_batch_remove(const char **keys, const uint32_t keys_count, int32_t *remove_results, uint32_t flags)
{
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(keys != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(keys_count != 0 && keys_count <= MAX_BATCH_COUNT, "invalid param, keys_count: "
                        << keys_count, MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(remove_results != nullptr, "invalid param, remove_results is null", MMC_INVALID_PARAM);

    std::vector<std::string> keys_vector;
    std::vector<int32_t> remove_results_vector;
    std::vector<size_t> invalids;
    keys_vector.reserve(keys_count);
    remove_results_vector.reserve(keys_count);
    invalids.reserve(keys_count);

    // remove invalid keys
    for (size_t i = 0; i < keys_count; ++i) {
        if (keys[i] == nullptr || strlen(keys[i]) == 0 || strlen(keys[i]) > 256) {
            MMC_LOG_WARN("Get invalid key: \"" << keys[i] << "\" on idx [" << i << "]");
            invalids.emplace_back(i);
            continue;
        }
        keys_vector.emplace_back(keys[i]);
    }

    MMC_RETURN_ERROR(gClientHandler->BatchRemove(keys_vector, remove_results_vector, flags),
                     gClientHandler->Name() << " batch_remove failed!");
    MMC_VALIDATE_RETURN(keys_count == remove_results_vector.size() + invalids.size(),
                        "invalid results' size (" << remove_results_vector.size() << "), should be keys_count ("
                        << keys_count << ") - invalid_keys' size (" << invalids.size() << ")", MMC_ERROR);

    invalids.emplace_back(-1);
    for (size_t i = 0, j = 0; i + j < keys_count; ++i) {
        if (i + j == invalids[j]) {
            remove_results[i + j] = MMC_INVALID_PARAM;
        } else {
            remove_results[i + j] = remove_results_vector[i];
        }
    }
    return MMC_OK;
}

MMC_API int32_t mmcc_exist(const char *key, uint32_t flags)
{
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) != 0, "invalid param, key's len equals 0", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) <= 256, "invalid param, key's len more than 256", MMC_INVALID_PARAM);
    Result result = gClientHandler->IsExist(key, flags);
    if (result == MMC_UNMATCHED_KEY) {
        // not exist, but does not need write error log
        return result;
    }
    MMC_RETURN_ERROR(result, gClientHandler->Name() << " is_exist failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_batch_exist(const char **keys, const uint32_t keys_count, int32_t *exist_results, uint32_t flags)
{
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(keys != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(keys_count != 0 && keys_count <= MAX_BATCH_COUNT, "invalid param, keys_count: "
                        << keys_count, MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(exist_results != nullptr, "invalid param, exist_results is null", MMC_INVALID_PARAM);

    std::vector<std::string> keys_vector;
    std::vector<int32_t> exist_results_vector;
    std::vector<size_t> invalids;
    keys_vector.reserve(keys_count);
    exist_results_vector.reserve(keys_count);
    invalids.reserve(keys_count);

    // remove invalid keys
    for (size_t i = 0; i < keys_count; ++i) {
        if (keys[i] == nullptr || strlen(keys[i]) == 0 || strlen(keys[i]) > 256) {
            MMC_LOG_WARN("Get invalid key: \"" << keys[i] << "\" on idx [" << i << "]");
            invalids.emplace_back(i);
            continue;
        }
        keys_vector.emplace_back(keys[i]);
    }

    MMC_RETURN_ERROR(gClientHandler->BatchIsExist(keys_vector, exist_results_vector, flags),
                     gClientHandler->Name() << " batch_is_exist failed!");
    MMC_VALIDATE_RETURN(keys_count == exist_results_vector.size() + invalids.size(),
                        "invalid results' size (" << exist_results_vector.size() << "), should be keys_count ("
                        << keys_count << ") - invalid_keys' size (" << invalids.size() << ")", MMC_ERROR);

    invalids.emplace_back(-1);
    for (size_t i = 0, j = 0; i + j < keys_count; ++i) {
        if (i + j == invalids[j]) {
            exist_results[i + j] = MMC_INVALID_PARAM;
        } else {
            exist_results[i + j] = exist_results_vector[i];
        }
    }
    return MMC_OK;
}

MMC_API int32_t mmcc_batch_get(const char **keys, uint32_t keys_count, mmc_buffer *bufs, uint32_t flags)
{
    MMC_VALIDATE_RETURN(keys != nullptr, "invalid param, keys is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(keys_count != 0 && keys_count <= MAX_BATCH_COUNT, "invalid param, keys_count: "
                        << keys_count, MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(bufs != nullptr, "invalid param, bufs is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    std::vector<std::string> keys_vector;
    std::vector<mmc_buffer> bufs_vector;
    std::vector<size_t> invalids;
    keys_vector.reserve(keys_count);
    bufs_vector.reserve(keys_count);
    invalids.reserve(keys_count);

    // remove invalid keys
    for (size_t i = 0; i < keys_count; ++i) {
        if (keys[i] == nullptr || strlen(keys[i]) == 0 || strlen(keys[i]) > 256) {
            MMC_LOG_WARN("Remove invalid key: " << keys[i]);
            continue;
        }
        if (bufs == nullptr) {
            MMC_LOG_WARN("Remove invalid buf with key: " << keys[i]);
            continue;
        }
        keys_vector.emplace_back(keys[i]);
        bufs_vector.emplace_back(bufs[i]);
    }

    MMC_RETURN_ERROR(gClientHandler->BatchGet(keys_vector, bufs_vector, flags),
                     gClientHandler->Name() << " batch_get failed!");

    MMC_VALIDATE_RETURN(keys_count == bufs_vector.size() + invalids.size(),
                        "invalid results' size (" << bufs_vector.size() << "), should be keys_count ("
                        << keys_count << ") - invalid_keys' size (" << invalids.size() << ")", MMC_ERROR);

    invalids.emplace_back(-1);
    for (size_t i = 0, j = 0; i + j < keys_count; ++i) {
        if (i + j == invalids[j]) {
            bufs[i + j] = mmc_buffer{.addr = 0, .type = 0, .dimType = 0, .oneDim = {0, 0}};
        } else {
            bufs[i + j] = bufs_vector[i];
        }
    }

    return MMC_OK;
}

MMC_API int32_t mmcc_batch_put(const char **keys, uint32_t keys_count, const mmc_buffer *bufs,
                               const mmc_put_options& options, uint32_t flags)
{
    MMC_VALIDATE_RETURN(keys != nullptr, "invalid param, keys is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(keys_count != 0 && keys_count <= MAX_BATCH_COUNT, "invalid param, keys_count: "
                        << keys_count, MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(bufs != nullptr, "invalid param, bufs is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(gClientHandler != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    std::vector<std::string> keys_vector;
    std::vector<mmc_buffer> bufs_vector;
    keys_vector.reserve(keys_count);
    bufs_vector.reserve(keys_count);

    // remove invalid keys, bufs
    for (size_t i = 0; i < keys_count; ++i) {
        if (keys[i] == nullptr || strlen(keys[i]) == 0 || strlen(keys[i]) > 256) {
            MMC_LOG_WARN("Remove invalid key: " << keys[i]);
            continue;
        }
        if (bufs == nullptr || bufs[i].addr == 0 || bufs[i].type >= 2 || bufs[i].dimType >= 2) {
            MMC_LOG_WARN("Remove invalid buf with key: " << keys[i]);
            continue;
        }
        keys_vector.emplace_back(keys[i]);
        bufs_vector.emplace_back(bufs[i]);
    }

    MMC_RETURN_ERROR(gClientHandler->BatchPut(keys_vector, bufs_vector, options, flags),
                     gClientHandler->Name() << " batch_put failed!");

    return MMC_OK;
}