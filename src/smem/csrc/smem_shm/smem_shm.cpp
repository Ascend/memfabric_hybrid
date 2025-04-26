/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_core_api.h"
#include "smem_shm.h"
#include "smem_logger.h"
#include "smem_shm_entry.h"
#include "smem_shm_team.h"
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
    SM_PARAM_VALIDATE(
        rankSize > UINT16_MAX || rankId >= rankSize,
        "invalid param, input size: " << rankSize << " limit: " << UINT16_MAX << " input rank: " << rankId, nullptr);
    SM_PARAM_VALIDATE(dataOpType != SMEMS_DATA_OP_MTE, "only support SMEMS_DATA_OP_MTE now", nullptr);

    SM_PARAM_VALIDATE(!g_smemShmInited, "smem shm not initialized yet", nullptr);

    std::lock_guard<std::mutex> guard(g_smemShmMutex_);
    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().CreateEntryById(id, entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("malloc entry failed, id: " << id << ", result: " << ret);
        return nullptr;
    }

    hybm_options options;
    options.bmType = HyBM_TYPE_HBM_AI_CORE_INITIATE;
    options.bmDataOpType = HyBM_DOP_TYPE_MTE;
    options.bmScope = HyBM_SCOPE_CROSS_NODE;
    options.bmRankType = HyBM_RANK_TYPE_STATIC;
    options.rankCount = rankSize;
    options.rankId = rankId;
    options.devId = SmemShmEntryManager::Instance().GetDeviceId();
    options.singleRankVASpace = symmetricSize;
    options.preferredGVA = 0;

    ret = entry->Initialize(options);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("entry init failed, result: " << ret);
        return nullptr;
    }

    *gva = entry->GetGva();
    return reinterpret_cast<void *>(entry.Get());
}

SMEM_API int32_t smem_shm_destroy(smem_shm_t handle, uint32_t flags)
{
    SM_PARAM_VALIDATE(handle == nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);

    SM_PARAM_VALIDATE(!g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    return SmemShmEntryManager::Instance().RemoveEntryByPtr(reinterpret_cast<uintptr_t>(handle));
}

SMEM_API int32_t smem_shm_set_extra_context(smem_shm_t handle, const void *context, uint32_t size)
{
    SM_PARAM_VALIDATE(handle == nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(context == nullptr, "invalid param, context is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(size == 0 || size > UN65536, "invalid param, size must be between 1~65536", SM_INVALID_PARAM);

    SM_PARAM_VALIDATE(!g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }
    return entry->SetExtraContext(context, size);
}

SMEM_API smem_shm_team_t smem_shm_team_create(smem_shm_t handle, const uint32_t *rankList, uint32_t rankSize, uint32_t flags)
{
    SM_PARAM_VALIDATE(handle == nullptr, "invalid param, handle is NULL", nullptr);
    SM_PARAM_VALIDATE(rankList == nullptr, "invalid param, rankList is NULL", nullptr);
    SM_PARAM_VALIDATE(rankSize == 0 || rankSize > UN65536, "invalid param, size must be between 1~65536", nullptr);

    SM_PARAM_VALIDATE(!g_smemShmInited, "smem shm not initialized yet", nullptr);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return nullptr;
    }
    return entry->CreateTeam(rankList, rankSize, flags);
}

SMEM_API int32_t smem_shm_team_destroy(smem_shm_team_t team, uint32_t flags)
{
    SM_PARAM_VALIDATE(team == nullptr, "invalid param, team is NULL", SM_INVALID_PARAM);

    SM_PARAM_VALIDATE(!g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    auto *t = reinterpret_cast<SmemShmTeam *>(team);
    if (t == nullptr) {
        SM_LOG_WARN("input team is null");
        return SM_OK;
    }

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryById(t->GetShmEntryId(), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_ERROR;
    }

    entry->RemoveTeam(t);
    return SM_OK;
}

SMEM_API smem_shm_team_t smem_shm_get_global_team(smem_shm_t handle)
{
    SM_PARAM_VALIDATE(handle == nullptr, "invalid param, handle is NULL", nullptr);

    SM_PARAM_VALIDATE(!g_smemShmInited, "smem shm not initialized yet", nullptr);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return nullptr;
    }
    return reinterpret_cast<void *>(entry->GetGlobalTeam().Get());
}

SMEM_API uint32_t smem_shm_team_get_rank(smem_shm_team_t team)
{
    SM_PARAM_VALIDATE(team == nullptr, "invalid param, team is NULL", SM_INVALID_PARAM);

    SM_PARAM_VALIDATE(!g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    auto *t = reinterpret_cast<SmemShmTeam *>(team);
    if (t == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input team is null");
        return UINT32_MAX;
    }
    return t->GetLocalRank();
}

SMEM_API uint32_t smem_shm_team_get_size(smem_shm_team_t team)
{
    SM_PARAM_VALIDATE(team == nullptr, "invalid param, team is NULL", SM_INVALID_PARAM);

    SM_PARAM_VALIDATE(!g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    auto *t = reinterpret_cast<SmemShmTeam *>(team);
    if (t == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input team is null");
        return UINT32_MAX;
    }
    return t->GetRankSize();
}

SMEM_API int32_t smem_shm_control_barrier(smem_shm_team_t team)
{
    SM_PARAM_VALIDATE(team == nullptr, "invalid param, team is NULL", SM_INVALID_PARAM);

    SM_PARAM_VALIDATE(!g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    auto *t = reinterpret_cast<SmemShmTeam *>(team);
    if (t == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input team is null");
        return SM_INVALID_PARAM;
    }

    return t->TeamBarrier();
}

SMEM_API int32_t smem_shm_control_allgather(smem_shm_team_t team, const char *sendBuf, uint32_t sendSize, char *recvBuf,
                                   uint32_t recvSize)
{
    SM_PARAM_VALIDATE(team == nullptr, "invalid param, team is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(sendBuf == nullptr, "invalid param, sendBuf is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(recvBuf == nullptr, "invalid param, recvBuf is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(sendSize == 0 || sendSize > UN65536, "Invalid sendSize, sendSize must be 1~65536",
                      SM_INVALID_PARAM);

    SM_PARAM_VALIDATE(!g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    auto *t = reinterpret_cast<SmemShmTeam *>(team);
    if (t == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input team is null");
        return SM_INVALID_PARAM;
    }
    return t->TeamAllGather(sendBuf, sendSize, recvBuf, recvSize);
}

SMEM_API int32_t smem_shm_topology_can_reach(smem_shm_t handle, uint32_t remoteRank, uint32_t *reachInfo)
{
    SM_PARAM_VALIDATE(handle == nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(reachInfo == nullptr, "invalid param, reachInfo is NULL", SM_INVALID_PARAM);

    SM_PARAM_VALIDATE(!g_smemShmInited, "smem shm not initialized yet", SM_NOT_INITIALIZED);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }
    // TODO: 待实现
    *reachInfo = SMEMS_DATA_OP_MTE;
    return SM_OK;
}

SMEM_API int32_t smem_shm_config_init(smem_shm_config_t *config)
{
    SM_PARAM_VALIDATE(config == nullptr, "invalid param, config is NULL", SM_INVALID_PARAM);
    config->shmInitTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->shmCreateTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->controlOperationTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->startConfigStore = true;
    config->flags = 0;
    return SM_OK;
}

SMEM_API int32_t smem_shm_init(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId, uint16_t deviceId,
                      uint64_t gvaSpaceSize, smem_shm_config_t *config)
{
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

    ret = HybmCoreApi::HybmCoreInit(gvaSpaceSize, deviceId, config->flags);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("init hybm failed, result: " << ret << ", flags: 0x" << std::hex << config->flags);
        return SM_ERROR;
    }

    g_smemShmInited = true;
    SM_LOG_INFO("smem_shm_init success. space_size: " << gvaSpaceSize << " world_size: " << worldSize
                                                      << " config_ip: " << configStoreIpPort);
    return SM_OK;
}

SMEM_API void smem_shm_uninit(uint32_t flags)
{
    if (!g_smemShmInited) {
        SM_LOG_WARN("smem shm not initialized yet");
        return;
    }

    HybmCoreApi::HybmCoreUninit();
    SM_LOG_INFO("smem_shm_uninit finished");
}

SMEM_API uint32_t smem_shm_query_support_data_operation(void)
{
    return SMEMS_DATA_OP_MTE;
}