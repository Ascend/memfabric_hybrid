/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_client.h"
#include "mmc_common_includes.h"
#include "mmc_client_default.h"

using namespace ock::mmc;

namespace {
constexpr uint32_t MAX_BATCH_COUNT = 16 * 1024;
}

MMC_API int32_t mmcc_init(mmc_client_config_t *config)
{
    MMC_VALIDATE_RETURN(config != nullptr, "invalid param, config is null", MMC_INVALID_PARAM);

    MMC_RETURN_ERROR(MmcClientDefault::RegisterInstance(), "register client failed");
    auto ret = MmcClientDefault::GetInstance()->Start(*config);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR(MmcClientDefault::GetInstance()->Name() << " init client failed, ret:" << ret);
        MmcClientDefault::UnregisterInstance();
        return ret;
    }
    return ret;
}

MMC_API void mmcc_uninit()
{
    MMC_VALIDATE_RETURN_VOID(MmcClientDefault::GetInstance() != nullptr, "client is not initialize");
    MmcClientDefault::GetInstance()->Stop();
    MmcClientDefault::UnregisterInstance();
}

MMC_API int32_t mmcc_put(const char *key, mmc_buffer *buf, mmc_put_options options, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) != 0, "invalid param, key's len equals 0", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) <= 256, "invalid param, key's len more than 256", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(buf != nullptr, "invalid param, buf is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN((void *)buf->addr != nullptr, "invalid param, buf addr is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(MmcClientDefault::GetInstance() != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    auto ret = MmcClientDefault::GetInstance()->Put(key, buf, options, flags);
    if (ret != MMC_DUPLICATED_OBJECT && ret != MMC_OK) {
        MMC_RETURN_ERROR(ret, MmcClientDefault::GetInstance()->Name() << " put key " << key << " failed!");
    }

    return ret;
}

MMC_API int32_t mmcc_get(const char *key, mmc_buffer *buf, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) != 0, "invalid param, key's len equals 0", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) <= 256, "invalid param, key's len more than 256", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(buf != nullptr, "invalid param, buf is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(buf->addr != 0, "invalid param, buf addr is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(MmcClientDefault::GetInstance() != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    MMC_RETURN_ERROR(MmcClientDefault::GetInstance()->Get(key, buf, flags), MmcClientDefault::GetInstance()->Name()
                                                                              << " get key " << key << " failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_query(const char *key, mmc_data_info *info, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) != 0, "invalid param, key's len equals 0", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) <= 256, "invalid param, key's len more than 256", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(info != nullptr, "invalid param, info is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(MmcClientDefault::GetInstance() != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    MMC_RETURN_ERROR(MmcClientDefault::GetInstance()->Query(key, *info, flags),
                     MmcClientDefault::GetInstance()->Name() << " query key " << key << " failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_batch_query(const char **keys, size_t keys_count, mmc_data_info *info, uint32_t flags)
{
    MMC_VALIDATE_RETURN(keys != nullptr, "invalid param, keys is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(keys_count != 0 && keys_count <= MAX_BATCH_COUNT, "invalid param, keys_count: "
                        << keys_count, MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(info != nullptr, "invalid param, info is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(MmcClientDefault::GetInstance() != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

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

    MMC_RETURN_ERROR(MmcClientDefault::GetInstance()->BatchQuery(keys_vector, info_vector, flags),
                     MmcClientDefault::GetInstance()->Name() << " batch query failed!");
    MMC_VALIDATE_RETURN(keys_count == info_vector.size() + invalids.size(),
                        "invalid results' size (" << info_vector.size() << "), should be keys_count ("
                        << keys_count << ") - invalid_keys' size (" << invalids.size() << ")", MMC_ERROR);

    for (size_t i = 0, j = 0; i + j < keys_count;) {
        if (j < invalids.size() && i + j == invalids[j]) {
            info[i + j] = {};
            ++j;
        } else {
            info[i + j] = info_vector[i];
            ++i;
        }
    }
    return MMC_OK;
}

MMC_API mmc_location_t mmcc_get_location(const char *key, uint32_t flags)
{
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", {});
    MMC_VALIDATE_RETURN(strlen(key) != 0, "invalid param, key's len equals 0", {});
    MMC_VALIDATE_RETURN(strlen(key) <= 256, "invalid param, key's len more than 256", {});
    MMC_VALIDATE_RETURN(MmcClientDefault::GetInstance() != nullptr, "client is not initialize", {});

    return MmcClientDefault::GetInstance()->GetLocation(key, flags);
}

MMC_API int32_t mmcc_remove(const char *key, uint32_t flags)
{
    MMC_VALIDATE_RETURN(MmcClientDefault::GetInstance() != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) != 0, "invalid param, key's len equals 0", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) <= 256, "invalid param, key's len more than 256", MMC_INVALID_PARAM);
    MMC_RETURN_ERROR(MmcClientDefault::GetInstance()->Remove(key, flags), MmcClientDefault::GetInstance()->Name() << " remove key " << key << " failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_batch_remove(const char **keys, const uint32_t keys_count, int32_t *remove_results, uint32_t flags)
{
    MMC_VALIDATE_RETURN(MmcClientDefault::GetInstance() != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);
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

    MMC_RETURN_ERROR(MmcClientDefault::GetInstance()->BatchRemove(keys_vector, remove_results_vector, flags),
                     MmcClientDefault::GetInstance()->Name() << " batch_remove failed!");
    MMC_VALIDATE_RETURN(keys_count == remove_results_vector.size() + invalids.size(),
                        "invalid results' size (" << remove_results_vector.size() << "), should be keys_count ("
                        << keys_count << ") - invalid_keys' size (" << invalids.size() << ")", MMC_ERROR);

    invalids.emplace_back(-1);
    for (size_t i = 0, j = 0; i + j < keys_count;) {
        if (i + j == invalids[j]) {
            remove_results[i + j] = MMC_INVALID_PARAM;
            ++j;
        } else {
            remove_results[i + j] = remove_results_vector[i];
            ++i;
        }
    }
    return MMC_OK;
}

MMC_API int32_t mmcc_exist(const char *key, uint32_t flags)
{
    MMC_VALIDATE_RETURN(MmcClientDefault::GetInstance() != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(key != nullptr, "invalid param, key is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) != 0, "invalid param, key's len equals 0", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(strlen(key) <= 256, "invalid param, key's len more than 256", MMC_INVALID_PARAM);
    Result result = MmcClientDefault::GetInstance()->IsExist(key, flags);
    if (result == MMC_UNMATCHED_KEY) {
        // not exist, but does not need write error log
        return result;
    }
    MMC_RETURN_ERROR(result, MmcClientDefault::GetInstance()->Name() << " is_exist failed!");
    return MMC_OK;
}

MMC_API int32_t mmcc_batch_exist(const char **keys, const uint32_t keys_count, int32_t *exist_results, uint32_t flags)
{
    MMC_VALIDATE_RETURN(MmcClientDefault::GetInstance() != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);
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

    MMC_RETURN_ERROR(MmcClientDefault::GetInstance()->BatchIsExist(keys_vector, exist_results_vector, flags),
                     MmcClientDefault::GetInstance()->Name() << " batch_is_exist failed!");
    MMC_VALIDATE_RETURN(keys_count == exist_results_vector.size() + invalids.size(),
                        "invalid results' size (" << exist_results_vector.size() << "), should be keys_count ("
                        << keys_count << ") - invalid_keys' size (" << invalids.size() << ")", MMC_ERROR);

    invalids.emplace_back(-1);
    for (size_t i = 0, j = 0; i + j < keys_count;) {
        if (i + j == invalids[j]) {
            exist_results[i + j] = MMC_INVALID_PARAM;
            ++j;
        } else {
            exist_results[i + j] = exist_results_vector[i];
            ++i;
        }
    }
    return MMC_OK;
}

MMC_API int32_t mmcc_batch_get(const char **keys, uint32_t keys_count, mmc_buffer *bufs, uint32_t flags, int* results)
{
    MMC_VALIDATE_RETURN(keys != nullptr, "invalid param, keys is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(keys_count != 0 && keys_count <= MAX_BATCH_COUNT, "invalid param, keys_count: "
                        << keys_count, MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(bufs != nullptr, "invalid param, bufs is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(MmcClientDefault::GetInstance() != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    std::vector<std::string> keys_vector;
    std::vector<mmc_buffer> bufs_vector;
    std::vector<int> batchResult(keys_count, MMC_OK);
    keys_vector.reserve(keys_count);
    bufs_vector.reserve(keys_count);

    for (size_t i = 0; i < keys_count; ++i) {
        if (keys[i] == nullptr || strlen(keys[i]) == 0 || strlen(keys[i]) > 256) {
            MMC_LOG_ERROR("Remove invalid key: " << keys[i]);
            return MMC_INVALID_PARAM;  // 这个错误属于入参不合法，直接给调用者返回错误
        }
        if (bufs == nullptr) {
            MMC_LOG_ERROR("Remove invalid buf with key: " << keys[i]);
            return MMC_INVALID_PARAM;
        }
        keys_vector.emplace_back(keys[i]);
        bufs_vector.emplace_back(bufs[i]);
    }

    MMC_RETURN_ERROR(MmcClientDefault::GetInstance()->BatchGet(keys_vector, bufs_vector, flags, batchResult),
                     MmcClientDefault::GetInstance()->Name() << " batch_get failed!");
    for (uint32_t i = 0; i < keys_count; ++i) {
        results[i] = batchResult[i];
    }
    return MMC_OK;
}

MMC_API int32_t mmcc_batch_put(const char** keys, uint32_t keys_count, const mmc_buffer* bufs, mmc_put_options& options,
                               uint32_t flags, int* results)
{
    MMC_VALIDATE_RETURN(keys != nullptr, "invalid param, keys is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(keys_count != 0 && keys_count <= MAX_BATCH_COUNT, "invalid param, keys_count: "
                        << keys_count, MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(bufs != nullptr, "invalid param, bufs is null", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(MmcClientDefault::GetInstance() != nullptr, "client is not initialize", MMC_CLIENT_NOT_INIT);

    std::vector<std::string> keys_vector;
    std::vector<mmc_buffer> bufs_vector;
    std::vector<int> batchResult(keys_count, MMC_OK);
    keys_vector.reserve(keys_count);
    bufs_vector.reserve(keys_count);

    // remove invalid keys, bufs
    for (size_t i = 0; i < keys_count; ++i) {
        if (keys[i] == nullptr || strlen(keys[i]) == 0 || strlen(keys[i]) > 256) {
            MMC_LOG_ERROR("Remove invalid key: " << keys[i]);
            return MMC_INVALID_PARAM;  // 这个错误属于入参不合法，直接给调用者返回错误
        }
        if (bufs == nullptr || bufs[i].addr == 0 || bufs[i].type >= 2 || bufs[i].dimType >= 2) {
            MMC_LOG_ERROR("Remove invalid buf with key: " << keys[i] << ", type: " << bufs[i].type
                                                          << ", dim:" << bufs[i].dimType);
            return MMC_INVALID_PARAM;
        }
        keys_vector.emplace_back(keys[i]);
        bufs_vector.emplace_back(bufs[i]);
    }

    MMC_RETURN_ERROR(MmcClientDefault::GetInstance()->BatchPut(keys_vector, bufs_vector, options, flags, batchResult),
                     MmcClientDefault::GetInstance()->Name() << " batch_put failed!");

    for (uint32_t i = 0; i < keys_count; ++i) {
        results[i] = batchResult[i];
    }
    return MMC_OK;
}