/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "smem_bm_entry.h"

#include "hybm_big_mem.h"
#include "hybm_data_op.h"
#include "smem_store_factory.h"

#include "mf_num_util.h"

namespace ock {
namespace smem {

static void ReleaseAfterFailed(hybm_entity_t entity, hybm_mem_slice_t slice)
{
    uint32_t flags = 0;
    if (entity != nullptr && slice != 0) {
        hybm_free_local_memory(entity, slice, 1, flags);
    }

    if (entity != nullptr) {
        hybm_unreserve_mem_space(entity, flags);
    }

    if (entity != nullptr) {
        hybm_destroy_entity(entity, flags);
    }
}

int32_t SmemBmEntry::Initialize(const hybm_options &options)
{
    if (inited_) {
        return SM_OK;
    }
    uint32_t flags = 0;
    hybm_entity_t entity = nullptr;
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

        ret = hybm_reserve_mem_space(entity, flags);
        if (ret != 0) {
            SM_LOG_ERROR("reserve mem failed, result: " << ret);
            ret = SM_ERROR;
            break;
        }

        bzero(&hbmSliceInfo_, sizeof(hybm_exchange_info));
        if (options.deviceVASpace > 0) {
            slice = hybm_alloc_local_memory(entity, HYBM_MEM_TYPE_DEVICE, options.deviceVASpace, flags);
            if (slice == nullptr) {
                SM_LOG_ERROR("alloc local device mem failed, size: " << options.deviceVASpace);
                ret = SM_ERROR;
                break;
            }

            ret = hybm_export(entity, slice, flags, &hbmSliceInfo_);
            if (ret != 0) {
                SM_LOG_ERROR("hybm export device slice failed, result: " << ret);
                break;
            }
        }

        bzero(&dramSliceInfo_, sizeof(hybm_exchange_info));
        if (options.hostVASpace > 0) {
            slice = hybm_alloc_local_memory(entity, HYBM_MEM_TYPE_HOST, options.hostVASpace, flags);
            if (slice == nullptr) {
                SM_LOG_ERROR("alloc local host mem failed, size: " << options.hostVASpace);
                ret = SM_ERROR;
                break;
            }

            ret = hybm_export(entity, slice, flags, &dramSliceInfo_);
            if (ret != 0) {
                SM_LOG_ERROR("hybm export host slice failed, result: " << ret);
                break;
            }
        }

        bzero(&entityInfo_, sizeof(hybm_exchange_info));
        ret = hybm_entity_export(entity, flags, &entityInfo_);
        if (ret != 0) {
            SM_LOG_ERROR("hybm entity export failed, result: " << ret);
            break;
        }
    } while (0);

    if (ret != 0) {
        ReleaseAfterFailed(entity, slice);
        globalGroup_ = nullptr;
        return ret;
    }

    coreOptions_ = options;
    entity_ = entity;
    hostGva_ = hybm_get_memory_ptr(entity, HYBM_MEM_TYPE_HOST);
    deviceGva_ = hybm_get_memory_ptr(entity, HYBM_MEM_TYPE_DEVICE);
    inited_ = true;
    return 0;
}

void SmemBmEntry::UnInitalize()
{
    if (!inited_) {
        return;
    }
    uint32_t flags = 0;
    if (entity_ != nullptr) {
        hybm_destroy_entity(entity_, flags);
    }
    inited_ = false;
}

Result SmemBmEntry::JoinHandle(uint32_t rk)
{
    SM_LOG_INFO("do join func, local_rk: " << options_.rank << " receive_rk: " << rk
        << ", rank size is: " << globalGroup_->GetRankSize());
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);

    std::vector<uint32_t> allRanks(globalGroup_->GetRankSize());
    auto ret = globalGroup_->GroupAllGather((const char *)&options_.rank, sizeof(uint32_t), (char *)allRanks.data(),
                                            sizeof(uint32_t) * globalGroup_->GetRankSize());
    if (ret != 0) {
        SM_LOG_ERROR("hybm gather ranks failed, result: " << ret);
        return SM_ERROR;
    }

    if ((ret = ExchangeSliceForJoin(hbmSliceInfo_)) != SM_OK) {
        return ret;
    }

    if ((ret = ExchangeSliceForJoin(dramSliceInfo_)) != SM_OK) {
        return ret;
    }

    ret = hybm_mmap(entity_, 0);
    if (ret != 0) {
        SM_LOG_ERROR("hybm mmap failed, result: " << ret);
        return SM_ERROR;
    }

    if ((ret = ExchangeEntityForJoin()) != SM_OK) {
        return ret;
    }

    globalGroup_->SetBitmapFromRanks(allRanks);
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

Result SmemBmEntry::ExchangeSliceForJoin(const hybm_exchange_info &sliceInfo)
{
    if (sliceInfo.descLen == 0) {
        return SM_OK;
    }

    std::vector<hybm_exchange_info> allExInfo(coreOptions_.rankCount);
    auto totalSize = sizeof(hybm_exchange_info) * globalGroup_->GetRankSize();
    auto ret = globalGroup_->GroupAllGather((const char *)&sliceInfo, sizeof(hybm_exchange_info),
                                            (char *)allExInfo.data(), totalSize);
    if (ret != 0) {
        SM_LOG_ERROR("hybm gather export failed, result: " << ret);
        return SM_ERROR;
    }

    ret = hybm_import(entity_, allExInfo.data(), globalGroup_->GetRankSize(), nullptr, 0);
    if (ret != 0) {
        SM_LOG_ERROR("hybm import failed, result: " << ret);
        return SM_ERROR;
    }

    ret = globalGroup_->GroupBarrier();
    if (ret != 0) {
        SM_LOG_ERROR("hybm barrier failed, result: " << ret);
        return SM_ERROR;
    }

    return SM_OK;
}

Result SmemBmEntry::ExchangeEntityForJoin()
{
    std::vector<hybm_exchange_info> allExInfo(coreOptions_.rankCount);
    auto ret = globalGroup_->GroupAllGather((char *)&entityInfo_, sizeof(hybm_exchange_info), (char *)allExInfo.data(),
                                            sizeof(hybm_exchange_info) * globalGroup_->GetRankSize());
    if (ret != 0) {
        SM_LOG_ERROR("hybm gather export failed, result: " << ret);
        return SM_ERROR;
    }

    ret = hybm_entity_import(entity_, allExInfo.data(), globalGroup_->GetRankSize(), 0);
    if (ret != 0) {
        SM_LOG_ERROR("hybm import failed, result: " << ret);
        return SM_ERROR;
    }

    ret = globalGroup_->GroupBarrier();
    if (ret != 0) {
        SM_LOG_ERROR("hybm barrier failed, result: " << ret);
        return SM_ERROR;
    }
    return SM_OK;
}

Result SmemBmEntry::Join(uint32_t flags)
{
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);
    auto ret = globalGroup_->GroupJoin();
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "join failed, ret: " << ret);
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
        case SMEMB_COPY_L2G: srcMemType = SMEM_MEM_TYPE_LOCAL_DEVICE; break;
        case SMEMB_COPY_G2L: destMemType = SMEM_MEM_TYPE_LOCAL_DEVICE; break;
        case SMEMB_COPY_G2H: destMemType = SMEM_MEM_TYPE_LOCAL_HOST; break;
        case SMEMB_COPY_H2G: srcMemType = SMEM_MEM_TYPE_LOCAL_HOST; break;
        case SMEMB_COPY_G2G:
        default: break;
    }

    return directMap[srcMemType][destMemType];
}

Result SmemBmEntry::DataCopy(const void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags)
{
    SM_VALIDATE_RETURN(src != nullptr, "invalid param, src is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(dest != nullptr, "invalid param, dest is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(size != 0, "invalid param, size is 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(t < SMEMB_COPY_BUTT, "invalid param, type invalid: " << t, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);

    auto direct = TransToHybmDirection(t, src, size, dest, size);
    if (direct == HYBM_DATA_COPY_DIRECTION_BUTT) {
        SM_LOG_ERROR("Failed to trans to hybm direct, smem direct: " << t << " src: " << src << " dest: " << dest);
        return SM_INVALID_PARAM;
    }

    hybm_copy_params copyParams = {const_cast<void *>(src), dest, size};
    return hybm_data_copy(entity_, &copyParams, direct, nullptr, flags);
}

Result SmemBmEntry::DataCopy2d(smem_copy_2d_params *params, smem_bm_copy_type t, uint32_t flags)
{
    SM_VALIDATE_RETURN(params->src != nullptr, "invalid param, src is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params->dest != nullptr, "invalid param, dest is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params->width != 0, "invalid param, width is 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params->height != 0, "invalid param, height is 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(t < SMEMB_COPY_BUTT, "invalid param, type invalid: " << t, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);

    SM_ASSERT_RETURN(!mf::NumUtil::IsOverflowCheck(params->dpitch, params->height, UINT64_MAX, '*'), SM_INVALID_PARAM);
    auto direct = TransToHybmDirection(t, params->src, params->spitch * params->height,
                                       params->dest, params->dpitch * params->height);
    if (direct == HYBM_DATA_COPY_DIRECTION_BUTT) {
        SM_LOG_ERROR("Failed to trans to hybm direct, smem direct: " << t << " src: " << params->src
            << " dest: " << params->dest);
        return SM_INVALID_PARAM;
    }

    hybm_copy_2d_params copy2dparams = {.src = params->src,
                                        .spitch = params->spitch,
                                        .dest = params->dest,
                                        .dpitch = params->dpitch,
                                        .width = params->width,
                                        .height = params->height};
    return hybm_data_copy_2d(entity_, &copy2dparams, direct, nullptr, flags);
}

Result SmemBmEntry::Wait()
{
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);
    return hybm_wait(entity_);
}

Result SmemBmEntry::RegisterMem(uint64_t addr, uint64_t size)
{
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);
    return hybm_register_user_mem(entity_, addr, size);
}

Result SmemBmEntry::DataCopyBatch(smem_batch_copy_params *params, smem_bm_copy_type t, uint32_t flags)
{
    SM_VALIDATE_RETURN(params->sources != nullptr, "invalid param, src is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params->destinations != nullptr, "invalid param, dest is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params->batchSize != 0, "invalid param, size is 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(t < SMEMB_COPY_BUTT, "invalid param, type invalid: " << t, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(inited_, SM_NOT_INITIALIZED);

    auto direct = TransToHybmDirection(t, params->sources[0], params->dataSizes[0],
                                       params->destinations[0], params->dataSizes[0]);
    if (direct == HYBM_DATA_COPY_DIRECTION_BUTT) {
        SM_LOG_ERROR("Failed to trans to hybm direct, smem direct: " << t << " src: " << params->sources[0]
                                                                     << " dest: " << params->destinations[0]);
        return SM_INVALID_PARAM;
    }
    hybm_batch_copy_params copyParams = {params->sources, params->destinations, params->dataSizes, params->batchSize};
    return hybm_data_batch_copy(entity_, &copyParams, direct, nullptr, flags);
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

bool SmemBmEntry::AddrInHostGva(const void *address, uint64_t size)
{
    if (hostGva_ == nullptr) {
        return false;
    }

    if (coreOptions_.hostVASpace >
        std::numeric_limits<uint64_t>::max() / static_cast<uint64_t>(coreOptions_.rankCount)) {
        return false;
    }
    auto totalSize = coreOptions_.hostVASpace * coreOptions_.rankCount;
    uintptr_t addrVal = reinterpret_cast<uintptr_t>(address);
    uintptr_t hostVal = reinterpret_cast<uintptr_t>(hostGva_);
    if (size > std::numeric_limits<uintptr_t>::max() - addrVal) {
        return false;
    }
    if (totalSize > std::numeric_limits<uintptr_t>::max() - hostVal) {
        return false;
    }

    if ((const uint8_t *)address + size > (const uint8_t *)hostGva_ + totalSize) {
        return false;
    }

    if ((const uint8_t *)address < (const uint8_t *)hostGva_) {
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
    if ((const uint8_t *)address + size > (const uint8_t *)deviceGva_ + totalSize) {
        return false;
    }

    if ((const uint8_t *)address < (const uint8_t *)deviceGva_) {
        return false;
    }

    return true;
}
}  // namespace smem
}  // namespace ock
