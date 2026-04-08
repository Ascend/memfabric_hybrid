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
#ifndef MF_SMEM_TRANS_ENTRY_H
#define MF_SMEM_TRANS_ENTRY_H

#include <thread>
#include <mutex>
#include <unordered_map>
#include <condition_variable>

#include "smem_common_includes.h"
#include "smem_config_store.h"
#include "mf_net.h"
#include "hybm_def.h"
#include "mf_rwlock.h"
#include "smem_trans.h"
#include "smem_net_group_engine.h"
#include "smem_net_common.h"

namespace ock {
namespace smem {

/*
 * lookup key of peer transfer entry
 */
using PeerEntryKey = std::pair<std::string, uint32_t>;
/*
 * peer transfer entry value, to store peer address etc.
 */
struct PeerEntryValue {
    void *address = nullptr;
};

struct WorkerUniqueId {
    ock::mf::net_addr_t address{};
    uint16_t port{0};
    uint16_t reserved{0};
};

using WorkerId = std::array<uint8_t, sizeof(WorkerUniqueId)>;

struct WorkerIdHash {
    size_t operator()(const WorkerId &id) const
    {
        return std::hash<std::string>()(std::string(id.begin(), id.end()));
    }
};

union WorkerIdUnion {
    WorkerUniqueId session;
    WorkerId workerId;

    explicit WorkerIdUnion(WorkerUniqueId ws) : session(ws) {}
    explicit WorkerIdUnion(WorkerId id) : workerId{id} {}
    explicit WorkerIdUnion() {}
};

struct LocalMapAddress {
    void *address;
    uint64_t size;
    LocalMapAddress() : address{nullptr}, size{0} {}
    LocalMapAddress(void *p, uint64_t s) : address{p}, size{s} {}
};

struct SmemTransExchangeInfo {
    hybm_exchange_info hybmInfo;
    union U {
        WorkerUniqueId session;
        LocalMapAddress address;
        U() noexcept {}
    } u;
};

class SmemTransEntry;
using SmemTransEntryPtr = SmRef<SmemTransEntry>;
using Local2GlobalMap = std::map<const void *, LocalMapAddress, std::greater<const void *>>;

class SmemTransEntry : public SmReferable {
public:
    static SmemTransEntryPtr Create(const std::string &name, const std::string &storeUrl,
                                    const smem_trans_config_t &config);

public:
    explicit SmemTransEntry(const smem_trans_config_t &config, std::string &name, uint32_t rank,
                            uint32_t id, StorePtr &store)
        : config_(config), name_(name), store_(store), entityId_(id), rankId_(rank)
    {}

    ~SmemTransEntry() override;

    const std::string &Name() const;
    const smem_trans_config_t &Config() const;

    Result Initialize();
    void UnInitialize();

    void*  MallocDram(uint64_t size);
    Result FreeDram(void *address);
    Result RegisterLocalMemory(const void *address, uint64_t size, uint32_t flags);
    Result RegisterLocalMemories(const std::vector<std::pair<const void *, size_t>> &regMemories, uint32_t flags);
    Result SyncTransfer(void *localAddr, const std::string &remoteUniqueId, void *remoteAddr, size_t dataSize,
                        smem_bm_copy_type opcode, void *stream, uint32_t flags);
    Result BatchSyncTransfer(void *localAddrs[], const std::string &remoteUniqueId, void *remoteAddrs[],
                             const size_t dataSizes[], uint32_t batchSize, smem_bm_copy_type opcode, void *stream,
                             uint32_t flags);
    Result BatchQuantTransfer(smem_trans_quant_copy_param_t *params, smem_bm_copy_type opcode);

private:
    Result CreateGlobalTeam(uint32_t rankId);
    Result JoinHandle(uint32_t rk);
    Result UpdateHandle(uint32_t rk);
    Result GroupOpBarrier(int32_t input);
    Result LeaveHandle(uint32_t rk);
    Result Join(uint32_t flags);
    Result Update(uint32_t flags);
    Result Leave(uint32_t flags);

    void AddRemoteInfo(uint32_t rk, smem_trans_role_t role, WorkerId &id, std::vector<void *> &global,
                       std::vector<LocalMapAddress> &local);
    void AddRemoteInfo(uint32_t rk, std::vector<void *> &global, std::vector<LocalMapAddress> &local);
    smem_trans_role_t QueryRole(uint32_t rk);

    bool ParseTransName(const std::string &name, ock::mf::net_addr_t &ip, uint16_t &port, uint16_t &reserved);
    void RemoveRanks(std::vector<uint32_t> &rankSet);
    Result ParseNameToUniqueId(const std::string &name, WorkerId &uniqueId);
    void AlignMemory(const void *&address, uint64_t &size);
    std::vector<std::pair<const void *, size_t>> CombineMemories(std::vector<std::pair<const void *, size_t>> &input);
    Result RegisterOneMemory(const void *address, uint64_t size, uint32_t flags);
    hybm_options GenerateHybmOptions();
    void StoreSlice(hybm_mem_slice_t slice, void *vaAddr);
    hybm_mem_slice_t RemoveSlice(void *addr);
    Result TransformAddr(Local2GlobalMap &maps, std::vector<void *> &addr, void *remoteAddrs[],
                         const size_t dataSizes[], uint32_t size);

private:
    hybm_entity_t entity_ = nullptr;                       /* local hybm entity */

    uint32_t rankId_ = 0;
    uint16_t entityId_ = 0;

    std::mutex memMutex_;
    std::vector<SmemTransExchangeInfo> registedInfo_;

    const std::string name_;
    UrlExtraction storeUrlExtraction_;
    smem_trans_config_t config_{}; /* config of transfer entry */
    WorkerUniqueId workerUniqueId_;

    SmemTransExchangeInfo entityInfo_;
    ock::mf::ReadWriteLock remoteSliceRwMutex_;
    std::unordered_map<WorkerId, Local2GlobalMap, WorkerIdHash> remoteSlices_;
    std::unordered_map<uint32_t, WorkerId> rankToWorkerId_;
    std::map<std::string, WorkerId> nameToWorkerId_; /* To accelerate name parsed */
    std::unordered_map<void *, hybm_mem_slice_t> addrToSliceMap_; // dram slice info
    std::unordered_map<uint32_t, uint32_t> rankUpdateIdx_;
    std::unordered_map<uint32_t, smem_trans_role_t> ranksRole_;
    mutable std::mutex addrMapMutex_;

    StorePtr store_;
    SmemGroupEnginePtr globalGroup_ = nullptr;
};

inline const std::string &SmemTransEntry::Name() const
{
    return name_;
}

inline const smem_trans_config_t &SmemTransEntry::Config() const
{
    return config_;
}
} // namespace smem
} // namespace ock

#endif // MF_SMEM_TRANS_ENTRY_H