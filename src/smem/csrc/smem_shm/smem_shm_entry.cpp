/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <algorithm>
#include "smem_common_includes.h"
#include "smem_shm_entry_manager.h"
#include "hybm_big_mem.h"
#include "smem_store_factory.h"
#include "smem_shm_entry.h"

namespace ock {
namespace smem {

SmemShmEntry::SmemShmEntry(uint32_t id) : id_{id}, entity_{nullptr}, gva_{nullptr}
{
    (void)smem_shm_config_init(&extraConfig_);

    auto emptyRollback = []() {
    };
    initSteps_.emplace_back(ShmEntryInitStep{"01_create_entity", [this]() { return InitStepCreateEntity(); },
                                             [this]() {
                                                 InitStepDestroyEntity();
                                             }});
    initSteps_.emplace_back(ShmEntryInitStep{"02_reserve_memory", [this]() { return InitStepReserveMemory(); },
                                             [this]() {
                                                 InitStepUnreserveMemory();
                                             }});
    initSteps_.emplace_back(ShmEntryInitStep{"03_alloc_slice", [this]() { return InitStepAllocSlice(); },
                                             [this]() {
                                                 InitStepFreeSlice();
                                             }});
    initSteps_.emplace_back(
        ShmEntryInitStep{"04_exchange_slice", [this]() { return InitStepExchangeSlice(); }, emptyRollback});
    initSteps_.emplace_back(
        ShmEntryInitStep{"05_exchange_entity", [this]() { return InitStepExchangeEntity(); }, emptyRollback});
    initSteps_.emplace_back(ShmEntryInitStep{"05_map_memory", [this]() { return InitStepMap(); }, emptyRollback});
}

SmemShmEntry::~SmemShmEntry()
{
    if (globalGroup_ != nullptr) {
        globalGroup_->GroupSnClean();
        globalGroup_ = nullptr;
    }

    uint32_t flags = 0;
    if (entity_ != nullptr && slice_ != nullptr) {
        hybm_free_local_memory(entity_, slice_, 1, flags);
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

    std::string prefix = "SHM_(" + std::to_string(id_) + ")_";
    StorePtr store = StoreFactory::PrefixStore(client, prefix);
    SM_ASSERT_RETURN(store != nullptr, SM_ERROR);

    SmemGroupOption opt = {rankSize, rankId,  extraConfig_.controlOperationTimeout * SECOND_TO_MILLSEC,
                           false,    nullptr, nullptr};
    SmemGroupEnginePtr group = SmemNetGroupEngine::Create(store, opt);
    SM_ASSERT_RETURN(group != nullptr, SM_ERROR);

    globalGroup_ = group;
    return globalGroup_->GroupBarrier();  // 保证所有rank都初始化了
}

Result SmemShmEntry::Initialize(hybm_options &options)
{
    localRank_ = options.rankId;
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(CreateGlobalTeam(options.rankCount, options.rankId), "create global team failed");

    options_ = options;
    for (auto it = initSteps_.begin(); it != initSteps_.end(); ++it) {
        SM_LOG_DEBUG("process init step : " << it->name);
        auto stepRet = it->processor();
        if (stepRet != 0) {
            SM_LOG_ERROR("init step(" << it->name << ") process failed: " << stepRet);
            auto fit = it;
            while (fit != initSteps_.begin()) {
                --fit;
                fit->rollback();
            }
            return stepRet;
        }
    }

    inited_ = true;
    return SM_OK;
}

void SmemShmEntry::SetConfig(const smem_shm_config_t &config)
{
    extraConfig_ = config;
    SM_LOG_INFO("shmId: " << id_ << " set_config control_operation_timeout: " << extraConfig_.controlOperationTimeout);
}

Result SmemShmEntry::SetExtraContext(const void *context, uint32_t size)
{
    if (!inited_ || entity_ == nullptr) {
        SM_LOG_ERROR("smem shm entry has not been initialized");
        return SM_ERROR;
    }

    return hybm_set_extra_context(entity_, context, size);
}

void *SmemShmEntry::GetGva() const
{
    return gva_;
}

int32_t SmemShmEntry::InitStepCreateEntity()
{
    auto entity = hybm_create_entity(id_ << 1, &options_, 0);
    if (entity == nullptr) {
        SM_LOG_ERROR("create entity failed");
        return SM_ERROR;
    }

    entity_ = entity;
    return SM_OK;
}

void SmemShmEntry::InitStepDestroyEntity()
{
    hybm_destroy_entity(entity_, 0);
    entity_ = nullptr;
}

int32_t SmemShmEntry::InitStepReserveMemory()
{
    void *reservedMem = nullptr;
    auto ret = hybm_reserve_mem_space(entity_, 0, &reservedMem);
    if (ret != 0 || reservedMem == nullptr) {
        SM_LOG_ERROR("reserve mem failed, result: " << ret);
        return SM_ERROR;
    }

    gva_ = reservedMem;
    return SM_OK;
}

void SmemShmEntry::InitStepUnreserveMemory()
{
    auto reservedMem = gva_;
    auto ret = hybm_unreserve_mem_space(entity_, 0, reservedMem);
    if (ret != 0) {
        SM_LOG_WARN("unreserve mem space failed: " << ret);
    }
    gva_ = nullptr;
}

int32_t SmemShmEntry::InitStepAllocSlice()
{
    auto slice = hybm_alloc_local_memory(entity_, HYBM_MEM_TYPE_DEVICE, options_.singleRankVASpace, 0);
    if (slice == nullptr) {
        SM_LOG_ERROR("alloc local mem failed, size: " << options_.singleRankVASpace);
        return SM_ERROR;
    }

    slice_ = slice;
    return SM_OK;
}

void SmemShmEntry::InitStepFreeSlice()
{
    auto slice = slice_;
    auto ret = hybm_free_local_memory(entity_, slice, 0, 0);
    if (ret != 0) {
        SM_LOG_WARN("free mem slice failed: " << ret);
    }
    slice_ = nullptr;
}

int32_t SmemShmEntry::InitStepExchangeSlice()
{
    hybm_exchange_info exInfo;
    bzero(&exInfo, sizeof(exInfo));
    auto ret = hybm_export(entity_, slice_, 0, &exInfo);
    if (ret != 0) {
        SM_LOG_ERROR("hybm export slice failed, result: " << ret);
        return ret;
    }

    hybm_exchange_info allExInfo[options_.rankCount];
    ret = globalGroup_->GroupAllGather((char *)&exInfo, sizeof(hybm_exchange_info), (char *)allExInfo,
                                       sizeof(hybm_exchange_info) * options_.rankCount);
    if (ret != 0) {
        SM_LOG_ERROR("hybm gather export slice failed, result: " << ret);
        return ret;
    }

    ret = hybm_import(entity_, allExInfo, options_.rankCount, nullptr, 0);
    if (ret != 0) {
        SM_LOG_ERROR("hybm import failed, result: " << ret);
        return ret;
    }

    ret = globalGroup_->GroupBarrier();
    if (ret != 0) {
        SM_LOG_ERROR("hybm barrier for slice failed, result: " << ret);
        return ret;
    }

    return SM_OK;
}

int32_t SmemShmEntry::InitStepExchangeEntity()
{
    hybm_exchange_info exInfo;
    bzero(&exInfo, sizeof(exInfo));
    auto ret = hybm_export(entity_, nullptr, 0, &exInfo);
    if (ret != 0) {
        SM_LOG_ERROR("hybm export entity failed, result: " << ret);
        return ret;
    }

    if (exInfo.descLen == 0) {
        return SM_OK;
    }

    hybm_exchange_info allExInfo[options_.rankCount];
    ret = globalGroup_->GroupAllGather((char *)&exInfo, sizeof(hybm_exchange_info), (char *)allExInfo,
                                       sizeof(hybm_exchange_info) * options_.rankCount);
    if (ret != 0) {
        SM_LOG_ERROR("hybm gather export entity failed, result: " << ret);
        return ret;
    }

    ret = hybm_import(entity_, allExInfo, options_.rankCount, nullptr, 0);
    if (ret != 0) {
        SM_LOG_ERROR("hybm import entity failed, result: " << ret);
        return ret;
    }

    ret = globalGroup_->GroupBarrier();
    if (ret != 0) {
        SM_LOG_ERROR("hybm barrier for entity failed, result: " << ret);
        return ret;
    }

    return SM_OK;
}

int32_t SmemShmEntry::InitStepMap()
{
    auto ret = hybm_mmap(entity_, 0);
    if (ret != 0) {
        SM_LOG_ERROR("hybm mmap failed, result: " << ret);
        return ret;
    }
    return SM_OK;
}

Result SmemShmEntry::GetReachInfo(uint32_t remoteRank, uint32_t &reachInfo) const
{
    if (entity_ == nullptr) {
        SM_LOG_ERROR("entity_ is null, cannot get reach info.");
        return SM_NOT_STARTED;
    }

    hybm_data_op_type reachesTypes;
    auto ret = hybm_entity_reach_types(entity_, remoteRank, reachesTypes, 0);
    if (ret != 0) {
        SM_LOG_ERROR("hybm_entity_reach_types() failed: " << ret);
        return SM_ERROR;
    }

    reachInfo = 0U;
    if (reachesTypes & HYBM_DOP_TYPE_MTE) {
        reachInfo |= SMEMS_DATA_OP_MTE;
    }

    if (reachesTypes & HYBM_DOP_TYPE_SDMA) {
        reachInfo |= SMEMS_DATA_OP_RDMA;
    }

    if (reachesTypes & HYBM_DOP_TYPE_DEVICE_RDMA) {
        reachInfo |= SMEMS_DATA_OP_SDMA;
    }

    return SM_OK;
}

}  // namespace smem
}  // namespace ock