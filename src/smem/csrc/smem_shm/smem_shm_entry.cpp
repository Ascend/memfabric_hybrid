/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include <algorithm>
#include "smem_common_includes.h"
#include "hybm_big_mem.h"
#include "smem_shm_entry.h"
#include "smem_shm_entry_manager.h"

namespace ock {
namespace smem {

SmemShmEntry::SmemShmEntry(uint32_t id) : id_{ id }, entity_{ nullptr }, gva_{ nullptr }
{
    (void)smem_shm_config_init(&extraConfig_);
}

SmemShmEntry::~SmemShmEntry()
{
    if (globalTeam_ != nullptr) {
        globalTeam_ = nullptr;
    }

    if (entity_ != nullptr && gva_ != nullptr) {
        hybm_unreserve_mem_space(entity_, 0, gva_);
        gva_ = nullptr;
    }

    if (entity_ != nullptr) {
        hybm_destroy_entity(entity_, 0);
        entity_ = nullptr;
    }
}

static void ReleaseAfterFailed(hybm_entity_t entity, hybm_mem_slice_t slice, void *reservedMem)
{
    uint32_t flags = 0;
    if (entity != nullptr && slice != 0) {
        hybm_free_local_memory(entity, slice, 1, flags);
    }

    if (entity != nullptr && reservedMem != nullptr) {
        hybm_unreserve_mem_space(entity, flags, reservedMem);
    }

    if (entity != nullptr) {
        hybm_destroy_entity(entity, flags);
    }
}

Result SmemShmEntry::CreateGlobalTeam(uint32_t rankSize, uint32_t rankId)
{
    auto client = SmemShmEntryManager::Instance().GetStoreClient();
    SM_ASSERT_RETURN(client != nullptr, SM_INVALID_PARAM);

    SmemShmTeamPtr team = SmMakeRef<SmemShmTeam>(0, rankSize, rankId);
    SM_ASSERT_RETURN(team != nullptr, SM_ERROR);

    auto ret = team->Initialize(id_, extraConfig_.controlOperationTimeout, client);
    if (ret != SM_OK) {
        SM_LOG_ERROR("global team init failed! ret: " << ret);
        return SM_ERROR;
    }

    globalTeam_ = team;
    return team->TeamBarrier(); // 保证所有rank都初始化了
}

Result SmemShmEntry::Initialize(hybm_options &options)
{
    uint32_t flags = 0;
    hybm_entity_t entity = nullptr;
    void *reservedMem = nullptr;
    hybm_mem_slice_t slice = 0;
    Result ret = SM_ERROR;
    localRank_ = options.rankId;

    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(CreateGlobalTeam(options.rankCount, options.rankId),
        "create global team failed.");

    do {
        entity = hybm_create_entity(id_, &options, flags);
        if (entity == nullptr) {
            SM_LOG_ERROR("create entity failed");
            ret = SM_ERROR;
            break;
        }

        ret = hybm_reserve_mem_space(entity, flags, &reservedMem);
        if (ret != 0 || reservedMem == nullptr) {
            SM_LOG_ERROR("reserve mem failed(" << ret << ")");
            ret = SM_ERROR;
            break;
        }

        slice = hybm_alloc_local_memory(entity, HyBM_MEM_TYPE_DEVICE, options.singleRankVASpace, flags);
        if (slice == 0) {
            SM_LOG_ERROR("alloc local mem failed, size(" << options.singleRankVASpace << ")");
            ret = -1;
            break;
        }

        hybm_exchange_info exInfo = { 0 };
        ret = hybm_export(entity, slice, flags, &exInfo);
        if (ret != 0) {
            SM_LOG_ERROR("hybm export failed(" << ret << ")");
            break;
        }

        hybm_exchange_info allExInfo[options.rankCount];
        ret = globalTeam_->TeamAllGather((char *)&exInfo, sizeof(hybm_exchange_info),
            (char *)allExInfo, sizeof(hybm_exchange_info) * options.rankCount);
        if (ret != 0) {
            SM_LOG_ERROR("hybm gather export failed(" << ret << ")");
            break;
        }

        ret = hybm_import(entity, allExInfo, options.rankCount, flags);
        if (ret != 0) {
            SM_LOG_ERROR("hybm import failed(" << ret << ")");
            break;
        }

        ret = globalTeam_->TeamBarrier();
        if (ret != 0) {
            SM_LOG_ERROR("hybm barrier failed(" << ret << ")");
            break;
        }

        ret = hybm_start(entity, flags);
        if (ret != 0) {
            SM_LOG_ERROR("hybm start failed(" << ret << ")");
            break;
        }
    } while (0);

    if (ret != 0) {
        ReleaseAfterFailed(entity, slice, reservedMem);
        globalTeam_ = nullptr;
        return ret;
    }

    entity_ = entity;
    gva_ = reservedMem;
    inited_ = true;
    return 0;
}

SmemShmTeam *SmemShmEntry::CreateTeam(const uint32_t *rankList, uint32_t rankSize, uint32_t flags)
{
    if (!inited_) {
        SM_LOG_ERROR("smem she entry don't inited!");
        return nullptr;
    }

    bool hasRank = false;
    uint32_t id = 0;
    for (uint32_t i = 0; i < rankSize; i++) {
        id += (rankList[i] <= localRank_);
        hasRank |= (rankList[i] == localRank_);
    }

    if (!hasRank) {
        SM_LOG_ERROR("the input rank list don't contain local rank: " << localRank_);
        return nullptr;
    }

    auto client = SmemShmEntryManager::Instance().GetStoreClient();
    SM_ASSERT_RETURN(client != nullptr, nullptr);
    SmemShmTeamPtr team = SmMakeRef<SmemShmTeam>(teamSn_.fetch_add(1U), rankSize, id);
    SM_ASSERT_RETURN(team != nullptr, nullptr);

    auto ret = team->Initialize(id_, extraConfig_.controlOperationTimeout, client);
    if (ret != SM_OK) {
        SM_LOG_ERROR("smem team init failed! ret: " << ret);
        return nullptr;
    }
    return team.Get();
}

void SmemShmEntry::SetConfig(const smem_shm_config_t &config)
{
    extraConfig_ = config;
    SM_LOG_INFO("smemId: " << id_ << " set_config control_operation_timeout: " << extraConfig_.controlOperationTimeout);
}

Result SmemShmEntry::SetExtraContext(const void *context, uint32_t size)
{
    if (!inited_ || entity_ == nullptr) {
        SM_LOG_ERROR("smem she entry don't inited!");
        return SM_ERROR;
    }

    return hybm_set_extra_context(entity_, context, size);
}

void *SmemShmEntry::GetGva() const
{
    return gva_;
}
}
}