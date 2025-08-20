/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include "smem_trans.h"

#include "smem_common_includes.h"
#include "hybm.h"
#include "smem_trans_entry.h"
#include "smem_trans_entry_manager.h"

using namespace ock::smem;

#ifdef UT_ENABLED
thread_local std::mutex g_smemTransMutex_;
thread_local bool g_smemTransInited = false;
#else
std::mutex g_smemTransMutex_;
bool g_smemTransInited = false;
#endif

SMEM_API int32_t smem_trans_config_init(smem_trans_config_t *config)
{
    SM_VALIDATE_RETURN(config != nullptr, "Invalid config", SM_INVALID_PARAM);

    config->initTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->role = SMEM_TRANS_NONE;
    config->deviceId = UINT32_MAX;
    config->flags = 0;

    return SM_OK;
}

SMEM_API int32_t smem_trans_init(const smem_trans_config_t *config)
{
    SM_VALIDATE_RETURN(config != nullptr, "invalid config, which is null", SM_INVALID_PARAM);

    if (g_smemTransInited) {
        SM_LOG_INFO("smem trans initialized already");
        return SM_OK;
    }
    auto ret = hybm_init(config->deviceId, config->flags);
    if (ret != 0) {
        SM_LOG_ERROR("hybm core init failed: " << ret);
        return ret;
    }

    g_smemTransInited = true;
    return SM_OK;
}

SMEM_API smem_trans_t smem_trans_create(const char *storeUrl, const char *sessionId, const smem_trans_config_t *config)
{
    SM_VALIDATE_RETURN(g_smemTransInited, "smem trans not initialized yet", nullptr);
    SM_VALIDATE_RETURN(storeUrl != nullptr, "invalid storeUrl, which is null", nullptr);
    SM_VALIDATE_RETURN(sessionId != nullptr, "invalid sessionId, which is null", nullptr);
    SM_VALIDATE_RETURN(config != nullptr, "invalid config, which is null", nullptr);
    SM_VALIDATE_RETURN(strlen(storeUrl) != 0, "invalid storeUrl, which is empty", nullptr);
    SM_VALIDATE_RETURN(strlen(sessionId) != 0, "invalid engineId, which is empty", nullptr);

    /* create entry */
    auto entry = SmemTransEntry::Create(sessionId, storeUrl, *config);
    if (entry == nullptr) {
        SM_LOG_ERROR("create entity happen error.");
        return nullptr;
    }

    return reinterpret_cast<smem_trans_t>(entry.Get());
}

SMEM_API void smem_trans_destroy(smem_trans_t handle, uint32_t flags)
{
    SM_ASSERT_RET_VOID(handle != nullptr);

    /* remove entry by ptr */
    auto result = SmemTransEntryManager::Instance().RemoveEntryByPtr(reinterpret_cast<uintptr_t>(handle));
    if (result == SM_OBJECT_NOT_EXISTS) {
        SM_LOG_AND_SET_LAST_ERROR("not found handle " << handle);
        return;
    } else if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("failed to erase entry by handle " << handle);
        return;
    }
}

SMEM_API void smem_trans_uninit(uint32_t flags)
{
    if (!g_smemTransInited) {
        SM_LOG_WARN("smem trans not initialized yet");
        return;
    }

    hybm_uninit();
    g_smemTransInited = false;
    SM_LOG_INFO("smem_trans_uninit finished");
}

SMEM_API int32_t smem_trans_register_mem(smem_trans_t handle, void *address, size_t capacity, uint32_t flags)
{
    SM_VALIDATE_RETURN(g_smemTransInited, "smem trans not initialized yet", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(handle != nullptr, "invalid handle, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(address != nullptr, "invalid address, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(capacity != 0, "invalid capacity, which is 0", SM_INVALID_PARAM);

    /* get entry by ptr */
    SmemTransEntryPtr entry;
    auto result = SmemTransEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("get entry by handle " << handle << " failed ");
        return result;
    }

    /* register memory to entry */
    result = entry->RegisterLocalMemory(address, capacity, flags);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("register local failed, result: " << result);
        return result;
    }

    return SM_OK;
}

SMEM_API int32_t smem_trans_batch_register_mem(smem_trans_t handle, void *addresses[], size_t capacities[], uint32_t count,
                                          uint32_t flags)
{
    SM_VALIDATE_RETURN(g_smemTransInited, "smem trans not initialized yet", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(handle != nullptr, "invalid handle, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(addresses != nullptr, "invalid address, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(capacities != nullptr, "invalid capacities, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(count != 0, "invalid capacity, which is 0", SM_INVALID_PARAM);

    std::vector<std::pair<const void *, size_t>> regMemories;
    for (auto i = 0U; i < count; i++) {
        SM_VALIDATE_RETURN(addresses[i] != nullptr, "invalid address, which is null", SM_INVALID_PARAM);
        SM_VALIDATE_RETURN(capacities[i] != 0, "invalid capacities, which is 0", SM_INVALID_PARAM);
        regMemories.push_back(std::make_pair(addresses[i], capacities[i]));
    }

    /* get entry by ptr */
    SmemTransEntryPtr entry;
    auto result = SmemTransEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("get entry by handle " << handle << " failed ");
        return result;
    }

    /* register memory to entry */
    result = entry->RegisterLocalMemories(regMemories, flags);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("register local failed, result: " << result);
        return result;
    }

    return SM_OK;
}

SMEM_API int32_t smem_trans_deregister_mem(smem_trans_t handle, void *address)
{
    SM_VALIDATE_RETURN(g_smemTransInited, "smem trans not initialized yet", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(handle != nullptr, "invalid handle, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(address != nullptr, "invalid address, which is null", SM_INVALID_PARAM);

    return SM_OK;
}

SMEM_API int32_t smem_trans_write(smem_trans_t handle, const void *srcAddress, const char *destSession,
                                  void *destAddress, size_t dataSize)
{
    SM_VALIDATE_RETURN(g_smemTransInited, "smem trans not initialized yet", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(handle != nullptr, "invalid handle, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(destSession != nullptr, "invalid destSession, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(srcAddress != nullptr, "invalid srcAddress, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(destAddress != nullptr, "invalid destAddress, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(dataSize != 0, "invalid dataSize, which is 0", SM_INVALID_PARAM);

    /* get entry by ptr */
    SmemTransEntryPtr entry;
    auto result = SmemTransEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("get entry by handle " << handle << " failed ");
        return result;
    }

    return entry->SyncWrite(srcAddress, destSession, destAddress, dataSize);
}

SMEM_API int32_t smem_trans_batch_write(smem_trans_t handle, const void *srcAddresses[], const char *destSession,
                                        void *destAddresses[], size_t dataSizes[], uint32_t batchSize)
{
    SM_VALIDATE_RETURN(g_smemTransInited, "smem trans not initialized yet", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(handle != nullptr, "invalid handle, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(srcAddresses != nullptr, "invalid srcAddresses, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(destSession != nullptr, "invalid srcAddress, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(destAddresses != nullptr, "invalid destAddress, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(dataSizes != nullptr, "invalid destAddress, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(batchSize != 0, "invalid batchSize, which is 0", SM_INVALID_PARAM);
    for (auto i = 0U; i < batchSize; i++) {
        SM_VALIDATE_RETURN(srcAddresses[i] != nullptr, "srcAddresses, which is null", SM_INVALID_PARAM);
        SM_VALIDATE_RETURN(destAddresses[i] != nullptr, "destAddresses, which is null", SM_INVALID_PARAM);
        SM_VALIDATE_RETURN(dataSizes[i] != 0, "invalid dataSizes, which is 0", SM_INVALID_PARAM);
    }

    /* get entry by ptr */
    SmemTransEntryPtr entry;
    auto result = SmemTransEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("get entry by handle " << handle << " failed ");
        return result;
    }

    return entry->SyncWrite(srcAddresses, destSession, destAddresses, dataSizes, batchSize);
}
