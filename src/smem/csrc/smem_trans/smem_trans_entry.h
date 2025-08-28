/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_SMEM_TRANS_ENTRY_H
#define MF_SMEM_TRANS_ENTRY_H

#include <thread>
#include <mutex>
#include <unordered_map>
#include <condition_variable>

#include "smem_common_includes.h"
#include "smem_config_store.h"
#include "hybm_def.h"
#include "smem_lock.h"
#include "smem_trans.h"

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

struct WorkerSession {
    uint32_t address{0};
    uint16_t port{0};
    uint16_t reserved{0};
};

union WorkerIdUnion {
    WorkerSession unique;
    uint64_t workerId;

    explicit WorkerIdUnion(WorkerSession ws) : unique(ws) {}
    explicit WorkerIdUnion(uint64_t id) : workerId{id} {}
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
    explicit SmemTransEntry(const std::string &name) : name_(name) {}

    ~SmemTransEntry() override;

    const std::string &Name() const;
    const smem_trans_config_t &Config() const;

    Result Initialize(const std::string &storeUrl, const smem_trans_config_t &config);
    void UnInitialize();

    Result RegisterLocalMemory(const void *address, uint64_t size, uint32_t flags);
    Result RegisterLocalMemories(const std::vector<std::pair<const void *, size_t>> &regMemories, uint32_t flags);
    Result SyncWrite(const void *srcAddress, const std::string &remoteName, void *destAddress, size_t dataSize);
    Result SyncWrite(const void *srcAddresses[], const std::string &remoteName, void *destAddresses[],
                     const size_t dataSizes[], uint32_t batchSize);

private:
    bool ParseTransName(const std::string &name, uint32_t &ip, uint16_t &port);
    Result StartWatchThread();
    void WatchTaskOneLoop();
    void WatchTaskFindNewSenders();
    void WatchTaskFindNewSlices();
    Result StoreDeviceInfo();
    Result ParseNameToUniqueId(const std::string &name, uint64_t &unique);
    void AlignMemory(const void *&address, uint64_t &size);
    std::vector<std::pair<const void *, size_t>> CombineMemories(std::vector<std::pair<const void *, size_t>> &input);
    Result RegisterOneMemory(const void *address, uint64_t size, uint32_t flags);

private:
    hybm_entity_t entity_ = nullptr;                     /* local hybm entity */
    std::map<PeerEntryKey, PeerEntryValue> peerEntries_; /* peer transfer entry look up map */

    StorePtr store_ = nullptr;

    std::mutex entryMutex_;
    bool inited_ = false;
    const std::string name_;
    UrlExtraction storeUrlExtraction_;
    smem_trans_config_t config_; /* config of transfer entry */
    WorkerSession workerSession_;
    uint32_t sliceInfoSize_{0};
    hybm_exchange_info deviceInfo_;
    std::thread watchThread_;
    std::mutex watchMutex_;
    std::condition_variable watchCond_;
    bool watchRunning_{true};
    int64_t slicesLastTime_ = 0;
    int64_t sendersLastTime_ = 0;

    ReadWriteLock remoteSliceRwMutex_;
    std::unordered_map<uint64_t, std::map<const void *, LocalMapAddress, std::greater<const void *>>> remoteSlices_;
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