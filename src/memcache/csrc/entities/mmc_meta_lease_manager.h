/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MF_HYBRID_MMC_META_LEASE_MANAGER_H
#define MF_HYBRID_MMC_META_LEASE_MANAGER_H
#include "mmc_logger.h"
#include "mmc_montotonic.h"
#include "mmc_ref.h"
#include "mmc_types.h"
#include <condition_variable>
#include <unordered_set>

namespace ock {
namespace mmc {

class MmcMetaLeaseManager : public MmcReferable {
public:
    Result Add(uint32_t id, uint32_t requestId, uint64_t ttl);
    Result Remove(uint32_t id, uint32_t requestId);
    Result Extend(uint64_t ttl);
    void Wait();
    inline uint32_t UseCount();
    inline uint64_t GenerateClientId(uint32_t rankId, uint32_t requestId);
    inline uint32_t RankId(uint64_t clientId);
    inline uint32_t RequestId(uint64_t clientId);

    friend std::ostream &operator<<(std::ostream &os, const MmcMetaLeaseManager &leaseMgr)
    {
        os << "lease={" << leaseMgr.lease_ << ",client:";
        for (const auto& c : leaseMgr.useClient) {
            os << c << ",";
        }
        os << "}";
        return os;
    }

private:
    uint64_t lease_{0}; /* lease of the memory object */
    std::unordered_set<uint64_t> useClient;
    std::mutex lock_;
    std::condition_variable cv_;
};

using MmcMetaLeaseManagerPtr = MmcRef<MmcMetaLeaseManager>;

inline uint32_t MmcMetaLeaseManager::UseCount()
{
    return useClient.size();
}

uint64_t MmcMetaLeaseManager::GenerateClientId(uint32_t rankId, uint32_t requestId)
{
    return (static_cast<uint64_t>(rankId) << 32) | requestId;
}
uint32_t MmcMetaLeaseManager::RankId(uint64_t clientId)
{
    return static_cast<uint32_t>(clientId >> 32);  // 高32位
}
uint32_t MmcMetaLeaseManager::RequestId(uint64_t clientId)
{
    return static_cast<uint32_t>(clientId & 0xFFFFFFFF);  // 低32位
}
}  // namespace mmc
}  // namespace ock

#endif  // MF_HYBRID_MMC_META_LEASE_MANAGER_H
