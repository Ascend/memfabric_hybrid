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
#include "smem_trans_store_helper.h"

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

struct LocalMapAddress {
    void *address;
    uint64_t size;
    LocalMapAddress() : address{nullptr}, size{0} {}
    LocalMapAddress(void *p, uint64_t s) : address{p}, size{s} {}
};

class SmemTransEntry;
using SmemTransEntryPtr = SmRef<SmemTransEntry>;

class SmemTransEntry : public SmReferable {
public:
    static SmemTransEntryPtr Create(const std::string &name, const std::string &storeUrl,
                                    const smem_trans_config_t &config);

public:
    explicit SmemTransEntry(const std::string &name, SmemStoreHelper helper)
        : name_(name),
          storeHelper_{std::move(helper)}
    {
    }

    ~SmemTransEntry() override;

    const std::string &Name() const;
    const smem_trans_config_t &Config() const;

    Result Initialize(const smem_trans_config_t &config);
    void UnInitialize();

    Result RegisterLocalMemory(const void *address, uint64_t size, uint32_t flags);
    Result RegisterLocalMemories(const std::vector<std::pair<const void *, size_t>> &regMemories, uint32_t flags);
    Result SyncTransfer(void *localAddr, const std::string &remoteUniqueId, void *remoteAddr, size_t dataSize,
                        smem_bm_copy_type opcode, void *stream);
    Result BatchSyncTransfer(void *localAddrs[], const std::string &remoteUniqueId, void *remoteAddrs[],
                             const size_t dataSizes[], uint32_t batchSize, smem_bm_copy_type opcode, void *stream);

private:
    bool ParseTransName(const std::string &name, ock::mf::net_addr_t &ip, uint16_t &port);
    void CleanupRemoteSlices(const std::vector<StoredSliceInfo> &rmSs);
    void RemoveRanks(std::set<uint32_t> &rankSet);
    Result StartWatchConnectThread();
    Result WatchConnectTaskOneLoop();
    Result StartWatchThread();
    void WatchTaskOneLoop();
    void WatchTaskFindNewRanks();
    void WatchTaskFindNewSlices();
    Result ParseNameToUniqueId(const std::string &name, WorkerId &uniqueId);
    void AlignMemory(const void *&address, uint64_t &size);
    std::vector<std::pair<const void *, size_t>> CombineMemories(std::vector<std::pair<const void *, size_t>> &input);
    Result RegisterOneMemory(const void *address, uint64_t size, uint32_t flags);
    hybm_options GenerateHybmOptions();
    int32_t ReInitialize();

private:
    hybm_entity_t entity_ = nullptr;                       /* local hybm entity */
    std::map<PeerEntryKey, PeerEntryValue> peerEntries_{}; /* peer transfer entry look up map */

    uint16_t rankId_ = 0;
    uint16_t entityId_ = 0;
    SmemStoreHelper storeHelper_;

    std::mutex entryMutex_;
    bool inited_ = false;
    const std::string name_;
    UrlExtraction storeUrlExtraction_;
    smem_trans_config_t config_{}; /* config of transfer entry */
    WorkerUniqueId workerUniqueId_;
    uint32_t sliceInfoSize_{0};
    hybm_exchange_info deviceInfo_{};
    std::thread watchThread_;
    std::thread watchConnectThread_;
    std::mutex watchMutex_;
    std::condition_variable watchCond_;
    bool watchRunning_{true};
    bool watchConnectRunning_{true};

    ock::mf::ReadWriteLock remoteSliceRwMutex_;
    std::unordered_map<
        WorkerId, std::map<const void *, LocalMapAddress, std::greater<const void *>>, WorkerIdHash> remoteSlices_;
    std::map<std::string, WorkerId> nameToWorkerId;     /* To accelerate name parsed */
};

inline const std::string &SmemTransEntry::Name() const
{
    return name_;
}

inline const smem_trans_config_t &SmemTransEntry::Config() const
{
    return config_;
}
}
}

#endif  // MF_SMEM_TRANS_ENTRY_H