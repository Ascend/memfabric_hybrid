/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm.h"
#include "smem_shm.h"
#include "smem_logger.h"
#include "smem_shm_entry.h"
#include "smem_shm_team.h"
#include "smem_shm_entry_manager.h"

using namespace ock::smem;
std::mutex g_smemShmMutex_;

smem_shm_t smem_shm_create(uint32_t id, uint32_t rankSize, uint32_t rankId,
                           uint64_t symmetricSize, smem_shm_data_op_type dataOpType, uint32_t flags, void **gva)
{
    SM_PARAM_VALIDATE(
        rankSize > UINT16_MAX || rankId >= rankSize,
        "Invalid param, input size: " << rankSize << " limit: " << UINT16_MAX << " input rank: " << rankId, nullptr);

    std::lock_guard<std::mutex> guard(g_smemShmMutex_);
    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().CreateEntryById(id, entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_ERROR("malloc entry failed, id: " << id << " ret: " << ret);
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
        SM_LOG_ERROR("entry init failed(" << ret << ")");
        return nullptr;
    }

    *gva = entry->GetGva();
    return reinterpret_cast<void*>(entry.Get());
}

int32_t smem_shm_destroy(smem_shm_t handle, uint32_t flags)
{
    SM_PARAM_VALIDATE(handle == nullptr, "Invalid param, handle is NULL", SM_INVALID_PARAM);
    return SmemShmEntryManager::Instance().RemoveEntryByPtr(reinterpret_cast<uintptr_t>(handle));
}

int32_t smem_shm_set_extra_context(smem_shm_t handle, const void* context, uint32_t size)
{
    SM_PARAM_VALIDATE(handle == nullptr, "Invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(context == nullptr, "Invalid param, context is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(size == 0 || size > UN65536, "Invalid param, size must be between 1~65536", SM_INVALID_PARAM);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_ERROR("input handle is invalid! ret: " << ret);
        return SM_INVALID_PARAM;
    }
    return entry->SetExtraContext(context, size);
}

smem_shm_team_t smem_shm_team_create(smem_shm_t handle, const uint32_t* rankList, uint32_t rankSize, uint32_t flags)
{
    SM_PARAM_VALIDATE(handle == nullptr, "Invalid param, handle is NULL", nullptr);
    SM_PARAM_VALIDATE(rankList == nullptr, "Invalid param, rankList is NULL", nullptr);
    SM_PARAM_VALIDATE(rankSize == 0 || rankSize > UN65536, "Invalid param, size must be between 1~65536", nullptr);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_ERROR("input handle is invalid! ret: " << ret);
        return nullptr;
    }
    return entry->CreateTeam(rankList, rankSize, flags);
}

int32_t smem_shm_team_destroy(smem_shm_team_t team, uint32_t flags)
{
    SM_PARAM_VALIDATE(team == nullptr, "Invalid param, team is NULL", SM_INVALID_PARAM);

    SmemShmTeam* t = reinterpret_cast<SmemShmTeam*>(team);
    if (t == nullptr) {
        SM_LOG_WARN("input team is null!");
        return 0;
    }

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryById(t->GetShmEntryId(), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_ERROR("input handle is invalid! ret: " << ret);
        return SM_ERROR;
    }

    entry->RemoveTeam(t);
    return 0;
}

smem_shm_team_t smem_shm_get_global_team(smem_shm_t handle)
{
    SM_PARAM_VALIDATE(handle == nullptr, "Invalid param, handle is NULL", nullptr);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_ERROR("input handle is invalid! ret: " << ret);
        return nullptr;
    }
    return reinterpret_cast<void*>(entry->GetGlobalTeam().Get());
}

uint32_t smem_shm_team_get_rank(smem_shm_team_t team)
{
    SM_PARAM_VALIDATE(team == nullptr, "Invalid param, team is NULL", SM_INVALID_PARAM);

    SmemShmTeam* t = reinterpret_cast<SmemShmTeam*>(team);
    if (t == nullptr) {
        SM_LOG_ERROR("input team is null!");
        return UINT32_MAX;
    }
    return t->GetLocalRank();
}

uint32_t smem_shm_team_get_size(smem_shm_team_t team)
{
    SM_PARAM_VALIDATE(team == nullptr, "Invalid param, team is NULL", SM_INVALID_PARAM);

    SmemShmTeam* t = reinterpret_cast<SmemShmTeam*>(team);
    if (t == nullptr) {
        SM_LOG_ERROR("input team is null!");
        return UINT32_MAX;
    }
    return t->GetRankSize();
}

int32_t smem_shm_control_barrier(smem_shm_team_t team)
{
    SM_PARAM_VALIDATE(team == nullptr, "Invalid param, team is NULL", SM_INVALID_PARAM);

    SmemShmTeam* t = reinterpret_cast<SmemShmTeam*>(team);
    if (t == nullptr) {
        SM_LOG_ERROR("input team is null!");
        return SM_INVALID_PARAM;
    }
    return t->TeamBarrier();
}

int32_t smem_shm_control_allgather(smem_shm_team_t team, const char* sendBuf, uint32_t sendSize, char* recvBuf,
                                   uint32_t recvSize)
{
    SM_PARAM_VALIDATE(team == nullptr, "Invalid param, team is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(sendBuf == nullptr, "Invalid param, sendBuf is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(recvBuf == nullptr, "Invalid param, recvBuf is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(sendSize == 0 || sendSize > UN65536, "Invalid sendSize, sendSize must be 1~65536",
                      SM_INVALID_PARAM);

    SmemShmTeam* t = reinterpret_cast<SmemShmTeam*>(team);
    if (t == nullptr) {
        SM_LOG_ERROR("input team is null!");
        return SM_INVALID_PARAM;
    }
    return t->TeamAllGather(sendBuf, sendSize, recvBuf, recvSize);
}

int32_t smem_shm_topology_can_reach(smem_shm_t handle, uint32_t remoteRank, uint32_t *reachInfo)
{
    SM_PARAM_VALIDATE(handle == nullptr, "Invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(reachInfo == nullptr, "Invalid param, reachInfo is NULL", SM_INVALID_PARAM);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_ERROR("input handle is invalid! ret: " << ret);
        return SM_INVALID_PARAM;
    }
    // TODO: 待实现
    *reachInfo = SMEM_TRANSPORT_CAP_MAP | SMEM_TRANSPORT_CAP_MTE;
    return 0;
}

int32_t smem_shm_config_init(smem_shm_config_t *config)
{
    SM_PARAM_VALIDATE(config == nullptr, "Invalid param, config is NULL", SM_INVALID_PARAM);
    config->shmInitTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->shmCreateTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->controlOperationTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->startConfigStore = true;
    config->flags = 0;
    return SM_OK;
}

int32_t smem_shm_init(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId,
                      uint16_t deviceId, uint64_t gvaSpaceSize, smem_shm_config_t *config)
{
    static bool inited = false;
    std::lock_guard<std::mutex> guard(g_smemShmMutex_);
    if (inited) {
        SM_LOG_INFO("smem shm has inited!");
        return SM_OK;
    }

    int32_t ret = SmemShmEntryManager::Instance().Initialize(configStoreIpPort, worldSize, rankId, deviceId, config);
    if (ret != 0) {
        SM_LOG_ERROR("init shm entry manager failed(" << ret << ")");
        return SM_ERROR;
    }

    ret = hybm_init(gvaSpaceSize, deviceId, config->flags);
    if (ret != 0) {
        SM_LOG_ERROR("init hybm failed(" << ret << "), flags(0x" << std::hex << config->flags << ")");
        return SM_ERROR;
    }

    inited = true;
    SM_LOG_INFO("smem_shm_init success. space_size: " << gvaSpaceSize << " world_size: " <<
        worldSize << " config_ip: " << configStoreIpPort);
    return SM_OK;
}

void smem_shm_uninit(uint32_t flags)
{
    hybm_uninit();
    SM_LOG_INFO("smem_shm_uninit finish");
}