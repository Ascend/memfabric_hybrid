/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#include <algorithm>
#include "hybm_big_mem.h"
#include "smem_logger.h"
#include "smem_shm_entry.h"
#include "smem_shm_entry_manager.h"
#include "smem_shm.h"

using namespace ock::smem;
std::mutex g_smemShmMutex_;
bool g_smemShmInited = false;

SMEM_API smem_shm_t smem_shm_create(uint32_t id, uint32_t rankSize, uint32_t rankId, uint64_t symmetricSize,
                                    smem_shm_data_op_type dataOpType, uint32_t flags, void **gva)
{
    SM_VALIDATE_RETURN(!(rankSize > SMEM_WORLD_SIZE_MAX || rankId >= rankSize),
                       "invalid param, input size: " << rankSize << " limit: " << SMEM_WORLD_SIZE_MAX
                                                     << " input rank: " << rankId,
                       nullptr);
    SM_VALIDATE_RETURN(!(id > SMEM_ID_MAX), "invalid id, id range is: [0, " << SMEM_ID_MAX << "]", nullptr);
    SM_VALIDATE_RETURN(gva != nullptr, "invalid param, gva is NULL", nullptr);
    SM_VALIDATE_RETURN(g_smemShmInited, "smem shm not initialized yet", nullptr);
    SM_VALIDATE_RETURN(symmetricSize <= SMEM_LOCAL_HBM_SIZE_MAX, "symmetric size exceeded", nullptr);

    std::lock_guard<std::mutex> guard(g_smemShmMutex_);
    SmemShmEntryPtr entry = nullptr;
    auto &manager = SmemShmEntryManager::Instance();
    auto ret = manager.CreateEntryById(id, entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("malloc entry failed, id: " << id << ", result: " << ret);
        return nullptr;
    }

    hybm_options options{};
    options.bmType = HYBM_TYPE_AI_CORE_INITIATE;
    options.memType = HYBM_MEM_TYPE_DEVICE;
    options.bmDataOpType = static_cast<hybm_data_op_type>(HYBM_DOP_TYPE_MTE | HYBM_DOP_TYPE_SDMA);
    if (dataOpType & SMEMS_DATA_OP_RDMA) {
        auto temp = static_cast<uint32_t>(options.bmDataOpType) | HYBM_DOP_TYPE_DEVICE_RDMA;
        options.bmDataOpType = static_cast<hybm_data_op_type>(temp);
    }
    options.bmScope = HYBM_SCOPE_CROSS_NODE;
    options.rankCount = rankSize;
    options.rankId = rankId;
    options.maxHBMSize = symmetricSize;
    options.deviceVASpace = symmetricSize;
    options.role = HYBM_ROLE_PEER;
    options.globalUniqueAddress = true;
    std::string defaultNic = "tcp://127.0.0.1:10002";
    std::copy_n(defaultNic.c_str(), defaultNic.size() + 1, options.transUrl);

    ret = entry->Initialize(options);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("entry init failed, result: " << ret);
        manager.RemoveEntryByPtr(reinterpret_cast<uintptr_t>(entry.Get()));
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

    return entry->GetReachInfo(remoteRank, *reachInfo);
}

SMEM_API int32_t smem_shm_config_init(smem_shm_config_t *config)
{
    SM_VALIDATE_RETURN(config != nullptr, "invalid param, config is NULL", SM_INVALID_PARAM);
    config->shmInitTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->shmCreateTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->controlOperationTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->startConfigStoreServer = true;
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

    uint64_t flags = static_cast<uint64_t>(config->flags) | HYBM_FLAG_INIT_SHMEM_META;
    ret = hybm_init(deviceId, flags);
    if (ret != 0) {
        SmemShmEntryManager::Instance().Destroy();
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

SMEM_API int32_t smem_shm_register_exit(smem_shm_t handle, void (*exit)(int))
{
    SM_VALIDATE_RETURN(exit != nullptr, "set exit function failed, invalid func which is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);

    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }

    SM_VALIDATE_RETURN(entry->GetGroup() != nullptr, "invalid param, Group is NULL", SM_INVALID_PARAM);
    ret = entry->GetGroup()->RegisterExit(exit);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("RegisterExit failed, result: " << ret);
        return SM_ERROR;
    }
    return SM_OK;
}

SMEM_API void smem_shm_global_exit(smem_shm_t handle, int status)
{
    if (handle == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("invalid param, handle is NULL");
        return;
    }
    SmemShmEntryPtr entry = nullptr;
    auto ret = SmemShmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return;
    }
    if (entry->GetGroup() == nullptr) {
        SM_LOG_ERROR("Group is NULL");
        return;
    }
    entry->GetGroup()->GroupBroadcastExit(status);
}