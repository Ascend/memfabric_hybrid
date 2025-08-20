/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_big_mem.h"
#include "hybm_data_op.h"
#include "smem_store_factory.h"
#include "smem_bm_entry.h"

namespace ock {
namespace smem {

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

int32_t SmemBmEntry::Initialize(const hybm_options &options)
{
    uint32_t flags = 0;
    hybm_entity_t entity = nullptr;
    void *reservedMem = nullptr;
    hybm_mem_slice_t slice = nullptr;
    Result ret = SM_ERROR;

    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(CreateGlobalTeam(options.rankCount, options.rankId), "create global team failed");

    do {
        entity = hybm_create_entity((Id() << 1) + 1U, &options, flags);
        if (entity == nullptr) {
            SM_LOG_ERROR("create entity failed");
            ret = SM_ERROR;
            break;
        }

        ret = hybm_reserve_mem_space(entity, flags, &reservedMem);
        if (ret != 0 || reservedMem == nullptr) {
            SM_LOG_ERROR("reserve mem failed, result: " << ret);
            ret = SM_ERROR;
            break;
        }

        slice = hybm_alloc_local_memory(entity, HYBM_MEM_TYPE_DEVICE, options.singleRankVASpace, flags);
        if (slice == nullptr) {
            SM_LOG_ERROR("alloc local mem failed, size: " << options.singleRankVASpace);
            ret = SM_ERROR;
            break;
        }

        bzero(&exInfo_, sizeof(hybm_exchange_info));
        ret = hybm_export(entity, slice, flags, &exInfo_);
        if (ret != 0) {
            SM_LOG_ERROR("hybm export failed, result: " << ret);
            break;
        }

        bzero(&entityInfo_, sizeof(hybm_exchange_info));
        ret = hybm_entity_export(entity, flags, &entityInfo_);
        if (ret != 0) {
            SM_LOG_ERROR("hybm entity export failed, result: " << ret);
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
    hostGva_ = hybm_get_memory_ptr(entity, HYBM_MEM_TYPE_HOST);
    deviceGva_ = hybm_get_memory_ptr(entity, HYBM_MEM_TYPE_DEVICE);
    inited_ = true;
    return 0;
}

Result SmemBmEntry::JoinHandle(uint32_t rk)
{
    SM_LOG_INFO("do join func, receive_rk: " << rk);
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);

    hybm_exchange_info allExInfo[coreOptions_.rankCount];
    auto ret = globalGroup_->GroupAllGather((char *)&exInfo_, sizeof(hybm_exchange_info), (char *)allExInfo,
                                       sizeof(hybm_exchange_info) * globalGroup_->GetRankSize());
    if (ret != 0) {
        SM_LOG_ERROR("hybm gather export failed, result: " << ret);
        return SM_ERROR;
    }

    ret = hybm_import(entity_, allExInfo, globalGroup_->GetRankSize(), 0);
    if (ret != 0) {
        SM_LOG_ERROR("hybm import failed, result: " << ret);
        return SM_ERROR;
    }

    ret = globalGroup_->GroupBarrier();
    if (ret != 0) {
        SM_LOG_ERROR("hybm barrier failed, result: " << ret);
        return SM_ERROR;
    }

    ret = hybm_mmap(entity_, 0);
    if (ret != 0) {
        SM_LOG_ERROR("hybm mmap failed, result: " << ret);
        return SM_ERROR;
    }

    ret = globalGroup_->GroupAllGather((char *)&entityInfo_, sizeof(hybm_exchange_info), (char *)allExInfo,
                                            sizeof(hybm_exchange_info) * globalGroup_->GetRankSize());
    if (ret != 0) {
        SM_LOG_ERROR("hybm gather export failed, result: " << ret);
        return SM_ERROR;
    }

    ret = hybm_entity_import(entity_, allExInfo, globalGroup_->GetRankSize(), 0);
    if (ret != 0) {
        SM_LOG_ERROR("hybm import failed, result: " << ret);
        return SM_ERROR;
    }

    ret = globalGroup_->GroupBarrier();
    if (ret != 0) {
        SM_LOG_ERROR("hybm barrier failed, result: " << ret);
        return SM_ERROR;
    }

    // TODO: rollback after join failed
    return SM_OK;
}

Result SmemBmEntry::LeaveHandle(uint32_t rk)
{
    SM_LOG_INFO("do leave func, receive_rk: " << rk);
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);
    auto ret = hybm_remove_imported(entity_, rk, 0);
    if (ret != 0) {
        SM_LOG_ERROR("hybm leave failed, result: " << ret);
        return SM_ERROR;
    }
    return SM_OK;
}

Result SmemBmEntry::Join(uint32_t flags, void **localGvaAddress)
{
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);
    auto ret = globalGroup_->GroupJoin();
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "join failed, ret: " << ret);

    if (localGvaAddress == nullptr) {
        SM_LOG_ERROR("the input localGvaAddress is nullptr.");
        return SM_INVALID_PARAM;
    }

    *localGvaAddress = (void *)(reinterpret_cast<uint64_t>(gva_) + coreOptions_.singleRankVASpace * options_.rank);
    return SM_OK;
}

Result SmemBmEntry::Leave(uint32_t flags)
{
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);
    auto ret = globalGroup_->GroupLeave();
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "leave failed, ret: " << ret);

    return SM_OK;
}

static hybm_data_copy_direction directMap[SMEMB_COPY_BUTT + 1][SMEM_MEM_TYPE_BUTT + 1] = {
    {HYBM_DATA_COPY_DIRECTION_BUTT, HYBM_DATA_COPY_DIRECTION_BUTT, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE,
     HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, HYBM_DATA_COPY_DIRECTION_BUTT},
    {HYBM_DATA_COPY_DIRECTION_BUTT, HYBM_DATA_COPY_DIRECTION_BUTT, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE,
     HYBM_LOCAL_HOST_TO_GLOBAL_HOST, HYBM_DATA_COPY_DIRECTION_BUTT},
    {HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE,
     HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST, HYBM_DATA_COPY_DIRECTION_BUTT},
    {HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE,
     HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, HYBM_DATA_COPY_DIRECTION_BUTT},
    {HYBM_DATA_COPY_DIRECTION_BUTT, HYBM_DATA_COPY_DIRECTION_BUTT, HYBM_DATA_COPY_DIRECTION_BUTT,
     HYBM_DATA_COPY_DIRECTION_BUTT, HYBM_DATA_COPY_DIRECTION_BUTT},
};

smem_bm_mem_type SmemBmEntry::GetHybmMemTypeFromGva(const void *addr, uint64_t size)
{
    if (AddrInHostGva(addr, size)) {
        return SMEM_MEM_TYPE_HOST;
    }
    if (AddrInDeviceGva(addr, size)) {
        return SMEM_MEM_TYPE_DEVICE;
    }
    return SMEM_MEM_TYPE_BUTT;
}

hybm_data_copy_direction SmemBmEntry::TransToHybmDirection(const smem_bm_copy_type &smemDirect, const void *src,
                                                           uint64_t srcSize, const void *dest, uint64_t destSize)
{
    smem_bm_mem_type srcMemType = GetHybmMemTypeFromGva(src, srcSize);
    smem_bm_mem_type destMemType = GetHybmMemTypeFromGva(dest, destSize);
    switch (smemDirect) {
        case SMEMB_COPY_L2G:
            srcMemType = SMEM_MEM_TYPE_LOCAL_DEVICE;
            break;
        case SMEMB_COPY_G2L:
            destMemType = SMEM_MEM_TYPE_LOCAL_DEVICE;
            break;
        case SMEMB_COPY_G2H:
            destMemType = SMEM_MEM_TYPE_LOCAL_HOST;
            break;
        case SMEMB_COPY_H2G:
            srcMemType = SMEM_MEM_TYPE_LOCAL_HOST;
            break;
        case SMEMB_COPY_G2G:
        default:
            break;
    }

    return directMap[srcMemType][destMemType];
}

Result SmemBmEntry::DataCopy(const void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags)
{
    SM_PARAM_VALIDATE(src == nullptr, "invalid param, src is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(dest == nullptr, "invalid param, dest is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(size == 0, "invalid param, size is 0", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(t >= SMEMB_COPY_BUTT, "invalid param, type invalid: " << t, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);

    auto direct = TransToHybmDirection(t, src, size, dest, size);
    if (direct == HYBM_DATA_COPY_DIRECTION_BUTT) {
        SM_LOG_ERROR("Failed to trans to hybm direct, smem direct: " << t << " src: " << src << " dest: " << dest);
        return SM_INVALID_PARAM;
    }
    return hybm_data_copy(entity_, src, dest, size, direct, nullptr, flags);
}

Result SmemBmEntry::DataCopy2d(const void *src, uint64_t spitch, void *dest, uint64_t dpitch,
                               uint64_t width, uint64_t height, smem_bm_copy_type t, uint32_t flags)
{
    SM_PARAM_VALIDATE(src == nullptr, "invalid param, src is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(dest == nullptr, "invalid param, dest is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(width == 0, "invalid param, width is 0", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(height == 0, "invalid param, height is 0", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(t >= SMEMB_COPY_BUTT, "invalid param, type invalid: " << t, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);

    auto direct = TransToHybmDirection(t, src, spitch * height, dest, dpitch * height);
    if (direct == HYBM_DATA_COPY_DIRECTION_BUTT) {
        SM_LOG_ERROR("Failed to trans to hybm direct, smem direct: " << t << " src: " << src << " dest: " << dest);
        return SM_INVALID_PARAM;
    }

    return hybm_data_copy_2d(entity_, src, spitch, dest, dpitch, width, height, direct, nullptr, flags);
}

Result SmemBmEntry::CreateGlobalTeam(uint32_t rankSize, uint32_t rankId)
{
    SmemGroupChangeCallback joinFunc = std::bind(&SmemBmEntry::JoinHandle, this, std::placeholders::_1);
    SmemGroupChangeCallback leaveFunc = std::bind(&SmemBmEntry::LeaveHandle, this, std::placeholders::_1);
    SmemGroupOption opt = {rankSize, rankId,  options_.controlOperationTimeout * SECOND_TO_MILLSEC,
                           true, joinFunc, leaveFunc};
    SmemGroupEnginePtr group = SmemNetGroupEngine::Create(_configStore, opt);
    SM_ASSERT_RETURN(group != nullptr, SM_ERROR);

    globalGroup_ = group;
    return SM_OK;
}

bool SmemBmEntry::AddrInHostGva(const void *address, uint64_t size)
{
    if (hostGva_ == nullptr) {
        return false;
    }

    auto totalSize = coreOptions_.hostVASpace * coreOptions_.rankCount;
    if ((const uint8_t *)address + size >= (const uint8_t *)hostGva_ + totalSize) {
        return false;
    }

    return true;
}

bool SmemBmEntry::AddrInDeviceGva(const void *address, uint64_t size)
{
    if (deviceGva_ == nullptr) {
        return false;
    }

    auto totalSize = coreOptions_.deviceVASpace * coreOptions_.rankCount;
    if ((const uint8_t *)address + size >= (const uint8_t *)deviceGva_ + totalSize) {
        return false;
    }

    return true;
}
}  // namespace smem
}  // namespace ock
