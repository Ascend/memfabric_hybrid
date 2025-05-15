/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_core_api.h"
#include "smem_store_factory.h"
#include "smem_bm_entry.h"

namespace ock {
namespace smem {

static void ReleaseAfterFailed(hybm_entity_t entity, hybm_mem_slice_t slice, void *reservedMem)
{
    uint32_t flags = 0;
    if (entity != nullptr && slice != 0) {
        HybmCoreApi::HybmFreeLocalMemory(entity, slice, 1, flags);
    }

    if (entity != nullptr && reservedMem != nullptr) {
        HybmCoreApi::HybmUnreserveMemSpace(entity, flags, reservedMem);
    }

    if (entity != nullptr) {
        HybmCoreApi::HybmDestroyEntity(entity, flags);
    }
}

void SmemBmEntry::SetConfig(const smem_bm_config_t &config)
{
    extraConfig_ = config;
    SM_LOG_INFO("bmId: " << options_.id
                         << " set_config control_operation_timeout: " << extraConfig_.controlOperationTimeout);
}

int32_t SmemBmEntry::Initialize(const hybm_options &options)
{
    uint32_t flags = 0;
    hybm_entity_t entity = nullptr;
    void *reservedMem = nullptr;
    hybm_mem_slice_t slice = nullptr;
    Result ret = SM_ERROR;
    localRank_ = options.rankId;

    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(CreateGlobalTeam(options.rankCount, options.rankId), "create global team failed");

    do {
        entity = HybmCoreApi::HybmCreateEntity((Id() << 1) + 1U, &options, flags);
        if (entity == nullptr) {
            SM_LOG_ERROR("create entity failed");
            ret = SM_ERROR;
            break;
        }

        ret = HybmCoreApi::HybmReserveMemSpace(entity, flags, &reservedMem);
        if (ret != 0 || reservedMem == nullptr) {
            SM_LOG_ERROR("reserve mem failed, result: " << ret);
            ret = SM_ERROR;
            break;
        }

        slice = HybmCoreApi::HybmAllocLocalMemory(entity, HyBM_MEM_TYPE_DEVICE, options.singleRankVASpace, flags);
        if (slice == nullptr) {
            SM_LOG_ERROR("alloc local mem failed, size: " << options.singleRankVASpace);
            ret = SM_ERROR;
            break;
        }

        hybm_exchange_info exInfo;
        bzero(&exInfo, sizeof(hybm_exchange_info));
        ret = HybmCoreApi::HybmExport(entity, slice, flags, &exInfo);
        if (ret != 0) {
            SM_LOG_ERROR("hybm export failed, result: " << ret);
            break;
        }

        hybm_exchange_info allExInfo[options.rankCount];
        ret = globalGroup_->GroupAllGather((char *)&exInfo, sizeof(hybm_exchange_info), (char *)allExInfo,
                                           sizeof(hybm_exchange_info) * options.rankCount);
        if (ret != 0) {
            SM_LOG_ERROR("hybm gather export failed, result: " << ret);
            break;
        }

        ret = HybmCoreApi::HybmImport(entity, allExInfo, options.rankCount, flags);
        if (ret != 0) {
            SM_LOG_ERROR("hybm import failed, result: " << ret);
            break;
        }

        ret = globalGroup_->GroupBarrier();
        if (ret != 0) {
            SM_LOG_ERROR("hybm barrier failed, result: " << ret);
            break;
        }

        ret = HybmCoreApi::HybmMmap(entity, flags);
        if (ret != 0) {
            SM_LOG_ERROR("hybm mmap failed, result: " << ret);
            break;
        }

        ret = HybmCoreApi::HybmStart(entity, flags);
        if (ret != 0) {
            SM_LOG_ERROR("hybm start failed, result: " << ret);
            break;
        }
    } while (0);

    if (ret != 0) {
        ReleaseAfterFailed(entity, slice, reservedMem);
        globalGroup_ = nullptr;
        return ret;
    }

    coreOptions_ = options;
    entity_ = entity;
    gva_ = reservedMem;
    inited_ = true;
    return 0;
}

Result SmemBmEntry::Join(uint32_t flags, void **localGvaAddress)
{
    SM_ASSERT_RETURN(!inited_, SM_NOT_INITIALIZED);

    return SM_OK;
}

Result SmemBmEntry::Leave(uint32_t flags)
{
    SM_ASSERT_RETURN(!inited_, SM_NOT_INITIALIZED);

    return SM_OK;
}

Result SmemBmEntry::DataCopy(const void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags)
{
    SM_PARAM_VALIDATE(src == nullptr, "invalid param, src is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(dest == nullptr, "invalid param, dest is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(size == 0, "invalid param, size is 0", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(t >= SMEMB_COPY_BUTT, "invalid param, type invalid: " << t, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(!inited_, SM_NOT_INITIALIZED);

    hybm_data_copy_direction direction;
    if (t == SMEMB_COPY_L2G || t == SMEMB_COPY_H2G) {
        SM_PARAM_VALIDATE(!AddressInRange(dest, size), "dest address: " << dest << ", size: " << size << " invalid.",
                          SM_INVALID_PARAM);
        direction = t == SMEMB_COPY_L2G ? HyBM_LOCAL_TO_SHARED : HyBM_DRAM_TO_SHARED;
    } else {
        SM_PARAM_VALIDATE(!AddressInRange(src, size), "src address: " << src << ", size: " << size << " invalid.",
                          SM_INVALID_PARAM);
        direction = t == SMEMB_COPY_G2L ? HyBM_SHARED_TO_LOCAL : HyBM_SHARED_TO_DRAM;
    }

    return HybmCoreApi::HybmDataCopy(entity_, src, dest, size, direction, flags);
}
Result SmemBmEntry::CreateGlobalTeam(uint32_t rankSize, uint32_t rankId)
{
    SmemGroupOption opt = {rankSize, rankId,  extraConfig_.controlOperationTimeout * SECOND_TO_MILLSEC,
                           false,    nullptr, nullptr};
    SmemGroupEnginePtr group = SmemNetGroupEngine::Create(_configStore, opt);
    SM_ASSERT_RETURN(group != nullptr, SM_ERROR);

    globalGroup_ = group;
    return globalGroup_->GroupBarrier();  // 保证所有rank都初始化了
}

bool SmemBmEntry::AddressInRange(const void *address, uint64_t size)
{
    if (address < gva_) {
        return false;
    }

    auto totalSize = coreOptions_.singleRankVASpace * coreOptions_.rankCount;
    if ((const uint8_t *)address + size >= (const uint8_t *)gva_ + totalSize) {
        return false;
    }

    return true;
}
}  // namespace smem
}  // namespace ock
