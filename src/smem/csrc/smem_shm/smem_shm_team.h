/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef SMEM_SMEM_SHM_TEAM_H
#define SMEM_SMEM_SHM_TEAM_H

#include <atomic>
#include "smem_common_includes.h"
#include "smem_net_group_engine.h"

namespace ock {
namespace smem {

class SmemShmTeam : public SmReferable {
public:
    SmemShmTeam(uint32_t id, uint32_t entryId, uint32_t rankSize, uint32_t rank)
        : teamId_(id), shmEntryId_(entryId), rankSize_(rankSize), localRank_(rank) {}
    ~SmemShmTeam();
    // teamTimeout(unit: second)
    Result Initialize(uint32_t teamTimeout, StorePtr store);

    Result TeamBarrier();

    Result TeamAllGather(const char *sendBuf, uint32_t sendSize, char *recvBuf, uint32_t recvSize);

    uint32_t GetLocalRank() const;

    uint32_t GetRankSize() const;

    uint32_t GetShmEntryId() const;

    uint32_t GetTeamId() const;

private:
    SmemGroupEnginePtr ge_ = nullptr;
    std::string prefix_;
    uint32_t rankSize_ = UINT32_MAX;
    uint32_t localRank_ = UINT32_MAX;
    uint32_t teamId_ = UINT32_MAX;
    uint32_t shmEntryId_ = UINT32_MAX;
    bool inited_ = false;
};
using SmemShmTeamPtr = SmRef<SmemShmTeam>;

inline uint32_t SmemShmTeam::GetLocalRank() const
{
    return localRank_;
}

inline uint32_t SmemShmTeam::GetRankSize() const
{
    return rankSize_;
}

inline uint32_t SmemShmTeam::GetShmEntryId() const
{
    return shmEntryId_;
}

inline uint32_t SmemShmTeam::GetTeamId() const
{
    return teamId_;
}
} // namespace smem
} // namespace ock
#endif // SMEM_SMEM_SHM_TEAM_H
