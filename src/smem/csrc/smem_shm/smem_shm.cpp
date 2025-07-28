/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_big_mem.h"
#include "smem_shm.h"
#include "smem_logger.h"
#include "smem_shm_entry.h"
#include "smem_shm_entry_manager.h"

using namespace ock::smem;
#ifdef UT_ENABLED
thread_local std::mutex g_smemShmMutex_;
thread_local bool g_smemShmInited = false;
#else
std::mutex g_smemShmMutex_;
bool g_smemShmInited = false;
#endif

SMEM_API smem_shm_t smem_shm_create(uint32_t id, uint32_t rankSize, uint32_t rankId, uint64_t symmetricSize,
                                    smem_shm_data_op_type dataOpType, uint32_t flags, void **gva)
{
    SM_VALIDATE_RETURN(!(rankSize > SMEM_WORLD_SIZE_MAX || rankId >= rankSize),
                       "invalid param, input size: " << rankSize << " limit: " << SMEM_WORLD_SIZE_MAX
                                                   << " input rank: " << rankId,
                       nullptr);
    SM_VALIDATE_RETURN(dataOpType == SMEMS_DATA_OP_MTE, "only support SMEMS_DATA_OP_MTE now", nullptr);
    SM_VALIDATE_RETURN(gva != nullptr, "invalid param, gva is NULL", nullptr);
    SM_VALIDATE_RETURN(g_smemShmInited, "smem shm not initialized yet", nullptr);
    SM_VALIDATE_RETURN(symmetricSize <= SMEM_LOCAL_SIZE_MAX, "symmetric size exceeded", nullptr);

    std::lock_guard<std::mutex> guard(g_smemShmMutex_);
    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().CreateEntryById(id, entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("malloc entry failed, id: " << id << ", result: " << ret);
        return nullptr;
    }

    hybm_options options;
    options.bmType = HYBM_TYPE_HBM_AI_CORE_INITIATE;
    options.bmDataOpType = HYBM_DOP_TYPE_MTE;
    options.bmScope = HYBM_SCOPE_CROSS_NODE;
    options.bmRankType = HYBM_RANK_TYPE_STATIC;
    options.rankCount = rankSize;
    options.rankId = rankId;
    options.singleRankVASpace = symmetricSize;
    options.preferredGVA = 0;
    options.globalUniqueAddress = true;

    ret = entry->Initialize(options);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("entry init failed, result: " << ret);
        SmemShmEntryManager::Instance().RemoveEntryByPtr(reinterpret_cast<uintptr_t>(entry.Get()));
        return nullptr;
    }

    *gva = entry->GetGva();
    return reinterpret_cast<void *>(entry.Get());
}

SMEM_API int32_t smem_shm_destroy(smem_shm_t handle, uint32_t flags)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    return SmemShmEntryManager::Instance().RemoveEntryByPtr(reinterpret_cast<uintptr_t>(handle));
}

SMEM_API int32_t smem_shm_set_extra_context(smem_shm_t handle, const void *context, uint32_t size)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(context != nullptr, "invalid param, context is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(!(size == 0 || size > UN65536), "invalid param, size must be between 1~65536", SM_INVALID_PARAM);

    SM_VALIDATE_RETURN(g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }
    return entry->SetExtraContext(context, size);
}

SMEM_API uint32_t smem_shm_get_global_rank(smem_shm_t handle)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", UINT32_MAX);
    SM_VALIDATE_RETURN(g_smemShmInited, "smem shm not initialized yet", UINT32_MAX);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return UINT32_MAX;
    }
    auto group = entry->GetGroup();
    SM_VALIDATE_RETURN(group != nullptr, "smem shm not init group yet", UINT32_MAX);
    return group->GetLocalRank();
}

SMEM_API uint32_t smem_shm_get_global_rank_size(smem_shm_t handle)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", UINT32_MAX);
    SM_VALIDATE_RETURN(g_smemShmInited, "smem shm not initialized yet", UINT32_MAX);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return UINT32_MAX;
    }
    auto group = entry->GetGroup();
    SM_VALIDATE_RETURN(group != nullptr, "smem shm not init group yet", UINT32_MAX);
    return group->GetRankSize();
}

SMEM_API int32_t smem_shm_control_barrier(smem_shm_t handle)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }
    auto group = entry->GetGroup();
    SM_VALIDATE_RETURN(group != nullptr, "smem shm not init group yet", SM_NOT_INITIALIZED);
    return group->GroupBarrier();
}

SMEM_API int32_t smem_shm_control_allgather(smem_shm_t handle, const char *sendBuf, uint32_t sendSize, char *recvBuf,
                                            uint32_t recvSize)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(sendBuf != nullptr, "invalid param, sendBuf is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(recvBuf != nullptr, "invalid param, recvBuf is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(!(sendSize == 0 || sendSize > UN65536), "Invalid sendSize, sendSize must be 1~65536",
                       SM_INVALID_PARAM);

    SM_VALIDATE_RETURN(g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }
    auto group = entry->GetGroup();
    SM_VALIDATE_RETURN(group != nullptr, "smem shm not init group yet", SM_NOT_INITIALIZED);
    return group->GroupAllGather(sendBuf, sendSize, recvBuf, recvSize);
}

SMEM_API int32_t smem_shm_topology_can_reach(smem_shm_t handle, uint32_t remoteRank, uint32_t *reachInfo)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(reachInfo != nullptr, "invalid param, reachInfo is NULL", SM_INVALID_PARAM);

    SM_VALIDATE_RETURN(g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }

    // 当前仅支持MTE
    *reachInfo = SMEMS_DATA_OP_MTE;
    return SM_OK;
}

SMEM_API int32_t smem_shm_config_init(smem_shm_config_t *config)
{
    SM_VALIDATE_RETURN(config != nullptr, "invalid param, config is NULL", SM_INVALID_PARAM);
    config->shmInitTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->shmCreateTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->controlOperationTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->startConfigStore = true;
    config->flags = 0;
    return SM_OK;
}

static int32_t SmemShmConfigCheck(const smem_shm_config_t *config)
{
    SM_VALIDATE_RETURN(config != nullptr, "config is null", SM_INVALID_PARAM);

    SM_VALIDATE_RETURN(config->shmInitTimeout != 0, "initTimeout is zero", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->shmInitTimeout <= SMEM_SHM_TIMEOUT_MAX, "initTimeout is too large", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->shmCreateTimeout != 0, "createTimeout is zero", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->shmCreateTimeout <= SMEM_SHM_TIMEOUT_MAX, "initTimeout is too large", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->controlOperationTimeout != 0, "controlOperationTimeout is zero", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->controlOperationTimeout <= SMEM_SHM_TIMEOUT_MAX, "controlOperationTimeout is too large",
                       SM_INVALID_PARAM);
    return 0;
}

SMEM_API int32_t smem_shm_init(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId, uint16_t deviceId,
                               smem_shm_config_t *config)
{
    SM_VALIDATE_RETURN(configStoreIpPort != nullptr, "invalid param, ipport is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(SmemShmConfigCheck(config) == 0, "config is invalid", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(!(worldSize > SMEM_WORLD_SIZE_MAX || rankId >= worldSize),
                       "invalid param, input size: " << worldSize << " limit: " << SMEM_WORLD_SIZE_MAX
                                                   << " input rank: " << rankId,
                       SM_INVALID_PARAM);

    std::lock_guard<std::mutex> guard(g_smemShmMutex_);
    if (g_smemShmInited) {
        SM_LOG_INFO("smem shm initialized already");
        return SM_OK;
    }

    int32_t ret = SmemShmEntryManager::Instance().Initialize(configStoreIpPort, worldSize, rankId, deviceId, config);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("init shm entry manager failed, result: " << ret);
        return SM_ERROR;
    }

    ret = hybm_init(deviceId, config->flags);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("init hybm failed, result: " << ret << ", flags: 0x" << std::hex << config->flags);
        return SM_ERROR;
    }

    g_smemShmInited = true;
    SM_LOG_INFO("smem_shm_init success. world_size: " << worldSize);
    return SM_OK;
}

SMEM_API void smem_shm_uninit(uint32_t flags)
{
    if (!g_smemShmInited) {
        SM_LOG_WARN("smem shm not initialized yet");
        return;
    }

    hybm_uninit();
    SmemShmEntryManager::Instance().Destroy();
    g_smemShmInited = false;
    SM_LOG_INFO("smem_shm_uninit finished");
}

SMEM_API uint32_t smem_shm_query_support_data_operation(void)
{
    return SMEMS_DATA_OP_MTE;
}