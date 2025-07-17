/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_META_MANAGER_H
#define MEM_FABRIC_MMC_META_MANAGER_H

#include "mmc_global_allocator.h"
#include "mmc_lookup_map.h"
#include "mmc_mem_obj_meta.h"
#include "mmc_meta_service.h"
#include "mmc_msg_packer.h"
#include <functional>
#include <list>
#include <thread>

namespace ock {
namespace mmc {

static const uint16_t NUM_BUCKETS = 29;
static const uint16_t EVICT_THRESHOLD_HIGH = 70;
static const uint16_t EVICT_THRESHOLD_LOW = 90;

class MmcMetaManager : public MmcReferable {
public:
    explicit MmcMetaManager(uint64_t defaultTtl) : defaultTtlMs_(defaultTtl)
    {
        globalAllocator_ = MmcMakeRef<MmcGlobalAllocator>();
        MMC_ASSERT(globalAllocator_ != nullptr);
        removeThread_ = std::thread(std::bind(&MmcMetaManager::AsyncRemoveThreadFunc, this));
    }

    ~MmcMetaManager()
    {
        {
            std::lock_guard<std::mutex> lk(removeThreadLock_);
            removePredicate_ = true;
            removeThreadCv_.notify_all();
        }
        removeThread_.join();
    }

    /**
     * @brief Get the meta object and extend the lease
     * @param key          [in] key of the meta object
     * @param objMeta      [out] the meta object obtained
     */
    Result Get(const std::string &key, MmcMemObjMetaPtr &objMeta);

    /**
     * @brief Get multiple meta objects and extend their leases
     * @param keys          [in] List of keys for the meta objects to retrieve
     * @param objMetas      [out] Vector of pointers to the retrieved meta objects, corresponding to the keys in order
     * @param getResults    [out] Vector of results indicating the success/failure of retrieving each key's meta object
     */
    Result BatchGet(const std::vector<std::string> &keys, std::vector<MmcMemObjMetaPtr> &objMetas,
                    std::vector<Result> &getResults);
    /**
     * @brief Alloc the global memeory space and create the meta object
     * @param key          [in] key of the meta object
     * @param metaInfo     [out] the meta object created
     */
    Result Alloc(const std::string &key, const AllocOptions &allocOpt, MmcMemObjMetaPtr &objMeta);

    /**
     * @brief Update the state
     * @param req          [in] update state request
     */
    Result UpdateState(const std::string &key, const MmcLocation &loc, const BlobActionResult &actRet);

    /**
     * @brief remove the meta object
     * @param key          [in] key of the to-be-removed meta object
     */
    Result Remove(const std::string &key);

    /**
     * @brief unmount new mem pool contributor
     * @param loc               [in] location of the new mem pool contributor
     * @param localMemInitInfo  [in] info of the new mem pool contributor
     */
    Result Mount(const MmcLocation &loc, const MmcLocalMemlInitInfo &localMemInitInfo);

    /**
     * @brief unmount the mempool contributor at given location
     * @param loc          [in] location of the mem pool contributor to be unmounted
     */
    Result Unmount(const MmcLocation &loc);

    /**
     * @brief Check if a meta object (key) is in memory
     * @param key          [in] key of the meta object
     * @param found        [out] status regarding whether the key was found
     */
    Result ExistKey(const std::string &key, Result &found);

    /**
     * @brief Check if a list of meta objects (keys) is in memory
     * @param keys          [in] keys of the meta objects
     * @param results       [out] the accessible state corresponding to each key
     * @param found        [out] status regarding whether the key was found
     */
    Result BatchExistKey(const std::vector<std::string> &keys, std::vector<Result> &results, Result &found);

    /**
     * @brief Batch remove multiple meta objects
     * @param keys          [in] List of keys of the to-be-removed meta objects
     * @param remove_results [out] Results of each removal operation
     */
    Result BatchRemove(const std::vector<std::string> &keys, std::vector<Result> &remove_results);

    /**
    * @brief Get blob query info with key
    * @param key            [in] key of the meta object
    * @param queryInfo      [out] the query info of the meta object
    */
    Result Query(const std::string &key, MemObjQueryInfo &queryInfo);

private:
    // LRU
    struct MemObjMetaLruItem {
        MmcMemObjMetaPtr memObjMetaPtr_;
        std::list<std::string>::iterator lruIter_;
    };

    /**
     * @brief force remove the blobs and object meta(if all its blobs are removed)
     * @param key          [in] key of the to-be-removed meta object
     * @param filter       [in] filter used to choose the to-be-removed blobs
     */
    Result ForceRemoveBlobs(const MmcMemObjMetaPtr &objMeta, const MmcBlobFilterPtr &filter = nullptr);

    void AsyncRemoveThreadFunc();

    void UpdateLRU(const std::string &key, MemObjMetaLruItem &lruItem)
    {
        lruList_.erase(lruItem.lruIter_);
        lruList_.push_front(key);
        lruItem.lruIter_ = lruList_.begin();
    }

    void DoEviction()
    {
        uint32_t numEvictObjs = lruList_.size() * EVICT_THRESHOLD_LOW / 100;
        for (uint32_t i = 0; i < numEvictObjs; ++i) {
            if (!lruList_.empty()) {
                std::string key = lruList_.back();
                Remove(key);
            } else {
                break;
            }
        }
    }

    MmcLookupMap<std::string, MemObjMetaLruItem, NUM_BUCKETS> objMetaLookupMap_;
    std::list<std::string> lruList_;
    MmcGlobalAllocatorPtr globalAllocator_;
    std::thread removeThread_;
    std::mutex removeThreadLock_;
    std::condition_variable removeThreadCv_;
    std::mutex removeListLock_;
    std::list<MmcMemObjMetaPtr> removeList_;
    bool removePredicate_ = false;
    uint64_t defaultTtlMs_; /* defult ttl in miliseconds*/
    mutable std::mutex mutex_;
};
using MmcMetaManagerPtr = MmcRef<MmcMetaManager>;
}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_META_MANAGER_H
