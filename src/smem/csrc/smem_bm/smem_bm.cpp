/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "smem_common_includes.h"
#include "hybm_big_mem.h"
#include "smem_bm.h"
#include "smem_logger.h"
#include "smem_bm_entry_manager.h"

using namespace ock::smem;
#ifdef UT_ENABLED
thread_local std::mutex g_smemBmMutex_;
thread_local bool g_smemBmInited = false;
#else
std::mutex g_smemBmMutex_;
bool g_smemBmInited = false;
#endif

SMEM_API int32_t smem_bm_config_init(smem_bm_config_t *config)
{
    SM_VALIDATE_RETURN(config != nullptr, "Invalid config", SM_INVALID_PARAM);
    config->initTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->createTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->controlOperationTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->startConfigStore = true;
    config->startConfigStoreOnly = false;
    config->dynamicWorldSize = false;
    config->unifiedAddressSpace = true;
    config->autoRanking = false;
    config->rankId = std::numeric_limits<uint16_t>::max();
    config->flags = 0;
    return SM_OK;
}

static int32_t SmemBmConfigCheck(const smem_bm_config_t *config)
{
    SM_VALIDATE_RETURN(config != nullptr, "config is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->unifiedAddressSpace == true, "unifiedAddressSpace must be true", SM_INVALID_PARAM);

    SM_VALIDATE_RETURN(config->initTimeout != 0, "initTimeout is zero", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->initTimeout <= SMEM_BM_TIMEOUT_MAX, "initTimeout is too large", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->createTimeout != 0, "createTimeout is zero", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->createTimeout <= SMEM_BM_TIMEOUT_MAX, "initTimeout is too large", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->controlOperationTimeout != 0, "controlOperationTimeout is zero", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->controlOperationTimeout <= SMEM_BM_TIMEOUT_MAX, "controlOperationTimeout is too large",
                       SM_INVALID_PARAM);

    // config->rank 在SmemBmEntryManager::PrepareStore中check
    return 0;
}

SMEM_API int32_t smem_bm_init(const char *storeURL, uint32_t worldSize, uint16_t deviceId,
                              const smem_bm_config_t *config)
{
    SM_VALIDATE_RETURN(worldSize != 0, "invalid param, worldSize is 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(worldSize <= SMEM_WORLD_SIZE_MAX, "invalid param, worldSize is too large", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(storeURL != nullptr, "invalid param, storeURL is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(SmemBmConfigCheck(config) == 0, "config is invalid", SM_INVALID_PARAM);

    std::lock_guard<std::mutex> guard(g_smemBmMutex_);
    if (g_smemBmInited) {
        SM_LOG_INFO("smem bm initialized already");
        return SM_OK;
    }

    int32_t ret = SmemBmEntryManager::Instance().Initialize(storeURL, worldSize, deviceId, *config);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("init bm entry manager failed, result: " << ret);
        return SM_ERROR;
    }

    ret = hybm_init(deviceId, config->flags);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("init hybm failed, result: " << ret << ", flags: 0x" << std::hex << config->flags);
        return SM_ERROR;
    }

    g_smemBmInited = true;
    SM_LOG_INFO("smem_bm_init success. config_ip: " << storeURL);
    return SM_OK;
}

SMEM_API void smem_bm_uninit(uint32_t flags)
{
    if (!g_smemBmInited) {
        SM_LOG_WARN("smem bm not initialized yet");
        return;
    }

    hybm_uninit();
    g_smemBmInited = false;
    SM_LOG_INFO("smem_bm_uninit finished");
}

SMEM_API uint32_t smem_bm_get_rank_id()
{
    return SmemBmEntryManager::Instance().GetRankId();
}

SMEM_API smem_bm_t smem_bm_create(uint32_t id, uint32_t memberSize, smem_bm_data_op_type dataOpType,
                                  uint64_t localDRAMSize, uint64_t localHBMSize, uint32_t flags)
{
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", nullptr);
    SM_VALIDATE_RETURN(!(localDRAMSize == 0UL && localHBMSize == 0UL), "localMemorySize is 0", nullptr);

    SmemBmEntryPtr entry;
    auto &manager = SmemBmEntryManager::Instance();
    auto ret = manager.CreateEntryById(id, entry);
    if (ret != 0 || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("create BM entity(" << id << ") failed: " << ret);
        return nullptr;
    }

    hybm_options options;
    options.bmType = HYBM_TYPE_HBM_AI_CORE_INITIATE;
    options.bmDataOpType = HYBM_DOP_TYPE_MTE;
    options.bmScope = HYBM_SCOPE_CROSS_NODE;
    options.bmRankType = HYBM_RANK_TYPE_STATIC;
    options.rankCount = manager.GetWorldSize();
    options.rankId = manager.GetRankId();
    options.singleRankVASpace = localHBMSize;
    options.preferredGVA = 0;

    if (options.singleRankVASpace == 0) {
        SM_LOG_AND_SET_LAST_ERROR("options.singleRankVASpace is 0, cannot be divided.");
        return nullptr;
    }

    if (options.rankCount > std::numeric_limits<uint64_t>::max() / options.singleRankVASpace) {
        SM_LOG_AND_SET_LAST_ERROR("options.rankCount mutiply options.singleRankVASpace exceeds max value of uint64_t");
        return nullptr;
    }

    ret = entry->Initialize(options);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("entry init failed, result: " << ret);
        SmemBmEntryManager::Instance().RemoveEntryByPtr(reinterpret_cast<uintptr_t>(entry.Get()));
        return nullptr;
    }

    return reinterpret_cast<void *>(entry.Get());
}

SMEM_API void smem_bm_destroy(smem_bm_t handle)
{
    SM_ASSERT_RET_VOID(handle != nullptr);
    SM_ASSERT_RET_VOID(g_smemBmInited);

    auto ret = SmemBmEntryManager::Instance().RemoveEntryByPtr(reinterpret_cast<uintptr_t>(handle));
    SM_ASSERT_RET_VOID(ret == SM_OK);
}

SMEM_API int32_t smem_bm_join(smem_bm_t handle, uint32_t flags, void **localGvaAddress)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(localGvaAddress != nullptr, "invalid param, addr is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", SM_NOT_INITIALIZED);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }

    return entry->Join(flags, localGvaAddress);
}

SMEM_API int32_t smem_bm_leave(smem_bm_t handle, uint32_t flags)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", SM_NOT_INITIALIZED);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }

    return entry->Leave(flags);
}

SMEM_API uint64_t smem_bm_get_local_mem_size(smem_bm_t handle)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", 0UL);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", 0UL);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return 0UL;
    }

    return entry->GetCoreOptions().singleRankVASpace;
}

SMEM_API void *smem_bm_ptr(smem_bm_t handle, uint16_t peerRankId)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", nullptr);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", nullptr);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return nullptr;
    }

    auto &coreOption = entry->GetCoreOptions();
    SM_VALIDATE_RETURN(peerRankId < coreOption.rankCount, "invalid param, peerRankId too large", nullptr);

    auto gvaAddress = entry->GetGvaAddress();
    return reinterpret_cast<uint8_t *>(gvaAddress) + coreOption.singleRankVASpace * peerRankId;
}

SMEM_API int32_t smem_bm_copy(smem_bm_t handle, const void *src, void *dest, uint64_t size, smem_bm_copy_type t,
                              uint32_t flags)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", SM_NOT_INITIALIZED);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }

    return entry->DataCopy(src, dest, size, t, flags);
}

SMEM_API int32_t smem_bm_copy_2d(smem_bm_t handle, const void *src, uint64_t spitch, void *dest, uint64_t dpitch,
                                 uint64_t width, uint64_t heigth, smem_bm_copy_type t, uint32_t flags)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", SM_NOT_INITIALIZED);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }

    return entry->DataCopy2d(src, spitch, dest, dpitch, width, heigth, t, flags);
}