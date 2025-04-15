/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "smem_shm_team.h"
#include "smem_shm_entry_manager.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {

SmemShmTeam::~SmemShmTeam()
{
    ge_ = nullptr;
}

Result SmemShmTeam::Initialize(uint32_t smemId, uint32_t teamTimeout, StorePtr store)
{
    SM_ASSERT_RETURN(store != nullptr, SM_INVALID_PARAM);
    if (teamTimeout > UINT32_MAX / SECOND_TO_MILLSEC) {
        SM_LOG_ERROR("input timeout is too large :" << teamTimeout);
        return SM_INVALID_PARAM;
    }

    prefix_ = "shmem(" + std::to_string(smemId) + ")_T(" + std::to_string(teamId_) + ")";

    ge_ = SmMakeRef<SmemNetGroupEngine>(store, teamTimeout * SECOND_TO_MILLSEC);
    SM_ASSERT_RETURN(ge_ != nullptr, SM_ERROR);

    inited_ = true;
    return SM_OK;
}

Result SmemShmTeam::TeamBarrier()
{
    SM_ASSERT_RETURN(inited_, SM_NOT_STARTED);
    return ge_->GroupBarrier(prefix_, rankSize_);
}

Result SmemShmTeam::TeamAllGather(const char *sendBuf, uint32_t sendSize, char *recvBuf, uint32_t recvSize)
{
    SM_ASSERT_RETURN(inited_, SM_NOT_STARTED);
    return ge_->GroupAllGather(prefix_, localRank_, rankSize_, sendBuf, sendSize, recvBuf, recvSize);
}

}
}