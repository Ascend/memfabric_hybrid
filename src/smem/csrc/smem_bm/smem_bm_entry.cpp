/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_big_mem.h"
#include "hybm_data_op.h"
#include "mf_num_util.h"
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

SmemBmEntry::~SmemBmEntry()
{
    if (entity_ == nullptr) {
        return;
    }
    uint32_t flags = 0;
    if (slice_ != nullptr) {
        hybm_free_local_memory(entity_, slice_, 1, flags);
    }

    if (gva_ != nullptr) {
        hybm_unreserve_mem_space(entity_, flags, gva_);
    }

    hybm_destroy_entity(entity_, flags);
    gva_ = nullptr;
    entity_ = nullptr;
}

int32_t SmemBmEntry::Initialize(const hybm_options &options)
{
    uint32_t flags = 0;
    hybm_entity_t entity = nullptr;
    void *reservedMem = nullptr;
    hybm_mem_slice_t slice = nullptr;
    Result ret = SM_ERROR;

    SM_LOG_INFO("SmemBmEntry initialize with options.bmType:" << options.bmType
                                                              << ", bmDataOpType=" << options.bmDataOpType);
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(CreateGlobalTeam(options.rankCount, options.rankId), "create global team failed");

    do {
        entity = hybm_create_entity((Id() << 1) + 1U, &options, flags);
        if (entity == nullptr) {
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
        slice_ = slice;
        bzero(&exInfo_, sizeof(hybm_exchange_info));
        ret = hybm_export(entity, slice, flags, &exInfo_);
        if (ret != 0) {
            SM_LOG_ERROR("hybm export failed, result: " << ret);
            break;
        }

        bzero(&entityInfo_, sizeof(hybm_exchange_info));
        ret = hybm_export(entity, nullptr, flags, &entityInfo_);
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

    ret = hybm_import(entity_, allExInfo, globalGroup_->GetRankSize(), nullptr, 0);
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

    ret = hybm_import(entity_, allExInfo, globalGroup_->GetRankSize(), nullptr, 0);
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
    if (localGvaAddress == nullptr) {
        SM_LOG_ERROR("the input localGvaAddress is nullptr.");
        return SM_INVALID_PARAM;
    }

    auto ret = globalGroup_->GroupJoin();
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "join failed, ret: " << ret);

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

static hybm_data_copy_direction directMap[SMEMB_COPY_BUTT] = {
    HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST,
    HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE,   HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST,   HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE,
    HYBM_GLOBAL_HOST_TO_LOCAL_HOST,     HYBM_LOCAL_HOST_TO_GLOBAL_HOST,     HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE,
};

static hybm_data_copy_direction dramDirectMap[SMEMB_COPY_BUTT] = {
    HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, HYBM_GLOBAL_HOST_TO_LOCAL_HOST,
    HYBM_LOCAL_HOST_TO_GLOBAL_HOST,   HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE,
    HYBM_GLOBAL_HOST_TO_LOCAL_HOST,   HYBM_LOCAL_HOST_TO_GLOBAL_HOST,   HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE,
};

Result SmemBmEntry::DataCopy(const void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags)
{
    SM_VALIDATE_RETURN(src != nullptr, "invalid param, src is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(dest != nullptr, "invalid param, dest is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(size != 0, "invalid param, size is 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(t < SMEMB_COPY_BUTT, "invalid param, type invalid: " << t, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);

    switch (t) {
        case SMEMB_COPY_L2G:
        case SMEMB_COPY_H2G:
        case SMEMB_COPY_L2GH:
        case SMEMB_COPY_H2GH:
            SM_VALIDATE_RETURN(AddressInRange(dest, size),
                               "dest address: " << dest << ", size: " << size << " invalid.", SM_INVALID_PARAM);
            break;
        default:
            SM_VALIDATE_RETURN(AddressInRange(src, size), "src address: " << src << ", size: " << size << " invalid.",
                               SM_INVALID_PARAM);
            break;
    }

    hybm_copy_params copyParams = {src, dest, size};
    auto direct = coreOptions_.memType == HYBM_MEM_TYPE_HOST ? dramDirectMap[t] : directMap[t];
    return hybm_data_copy(entity_, &copyParams, direct, nullptr, flags);
}

Result SmemBmEntry::DataCopyBatch(const void **src, void **dest, const size_t *size, uint32_t count,
                                  smem_bm_copy_type t, uint32_t flags)
{
    SM_VALIDATE_RETURN(src != nullptr, "invalid param, src is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(dest != nullptr, "invalid param, dest is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(count != 0, "invalid param, size is 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(t < SMEMB_COPY_BUTT, "invalid param, type invalid: " << t, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);

    auto direct = coreOptions_.memType == HYBM_MEM_TYPE_HOST ? dramDirectMap[t] : directMap[t];
    hybm_batch_copy_params copyParams = {src, dest, size, count};
    return hybm_data_batch_copy(entity_, &copyParams, direct, nullptr, flags);
}

Result SmemBmEntry::DataCopy2d(smem_copy_2d_params &params, smem_bm_copy_type t, uint32_t flags)
{
    SM_VALIDATE_RETURN(params.src != nullptr, "invalid param, src is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params.dest != nullptr, "invalid param, dest is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params.width != 0, "invalid param, width is 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params.height != 0, "invalid param, height is 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(t < SMEMB_COPY_BUTT, "invalid param, type invalid: " << t, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);

    switch (t) {
        case SMEMB_COPY_L2G:
        case SMEMB_COPY_H2G:
        case SMEMB_COPY_L2GH:
        case SMEMB_COPY_H2GH:
            SM_VALIDATE_RETURN(!ock::mf::NumUtil::IsOverflowCheck(params.dpitch, params.height - 1, UINT64_MAX, '*'),
                "copy target range invalid: dpitch * (height - 1) would overflow: dpitch=" << params.dpitch
                << ", height=" << params.height, SM_INVALID_PARAM);
            SM_VALIDATE_RETURN(!ock::mf::NumUtil::IsOverflowCheck(params.dpitch * (params.height - 1), params.width,
                UINT64_MAX, '+'), "copy target range invalid: dpitch * (height - 1) +  would width: dpitch="
                << params.dpitch << ", height=" << params.height << ", width=" << params.width, SM_INVALID_PARAM);
            SM_VALIDATE_RETURN(AddressInRange(params.dest, params.dpitch * (params.height - 1) + params.width),
                               "dest address: " << params.dest << " dpitch: " << params.dpitch << " width: "
                                                << params.width << " height: " << params.height << " invalid.",
                               SM_INVALID_PARAM);
            break;
        default:
            SM_VALIDATE_RETURN(!ock::mf::NumUtil::IsOverflowCheck(params.spitch, params.height - 1, UINT64_MAX, '*'),
                "copy target range invalid: dpitch * (height - 1) would overflow: dpitch=" << params.dpitch
                << ", height=" << params.height, SM_INVALID_PARAM);
            SM_VALIDATE_RETURN(!ock::mf::NumUtil::IsOverflowCheck(params.spitch * (params.height - 1), params.width,
                UINT64_MAX, '+'), "copy target range invalid: dpitch * (height - 1) +  would width: dpitch="
                << params.dpitch << ", height=" << params.height << ", width=" << params.width, SM_INVALID_PARAM);
            SM_VALIDATE_RETURN(AddressInRange(params.src, params.spitch * (params.height - 1) + params.width),
                               "src address: " << params.src << ", spitch: " << params.spitch << " width: "
                                               << params.width << " height: " << params.height << " invalid.",
                               SM_INVALID_PARAM);
            break;
    }
    hybm_copy_2d_params copy2dparams = {.src = params.src,
                                        .spitch = params.spitch,
                                        .dest = params.dest,
                                        .dpitch = params.dpitch,
                                        .width = params.width,
                                        .height = params.height};
    auto direct = coreOptions_.memType == HYBM_MEM_TYPE_HOST ? dramDirectMap[t] : directMap[t]; // TODO: liuqzh : 两种介质同时存在怎么办？
    return hybm_data_copy_2d(entity_, &copy2dparams, direct, nullptr, flags);
}

Result SmemBmEntry::CreateGlobalTeam(uint32_t rankSize, uint32_t rankId)
{
    SmemGroupChangeCallback joinFunc = std::bind(&SmemBmEntry::JoinHandle, this, std::placeholders::_1);
    SmemGroupChangeCallback leaveFunc = std::bind(&SmemBmEntry::LeaveHandle, this, std::placeholders::_1);
    SmemGroupOption opt = {rankSize, rankId,   options_.controlOperationTimeout * SECOND_TO_MILLSEC,
                           true,     joinFunc, leaveFunc};
    SmemGroupEnginePtr group = SmemNetGroupEngine::Create(_configStore, opt);
    SM_ASSERT_RETURN(group != nullptr, SM_ERROR);

    globalGroup_ = group;
    return SM_OK;
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
