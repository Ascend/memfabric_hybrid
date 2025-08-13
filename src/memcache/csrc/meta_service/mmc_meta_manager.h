/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_META_MANAGER_H
#define MEM_FABRIC_MMC_META_MANAGER_H

#include "mmc_global_allocator.h"
#include "mmc_mem_obj_meta.h"
#include "mmc_meta_container.h"
#include "mmc_meta_service.h"
#include "mmc_msg_packer.h"
#include "mmc_meta_backup_mgr.h"
#include <functional>
#include <thread>
#include <list>

namespace ock {
namespace mmc {

static const uint16_t NUM_BUCKETS = 29;

class MmcMetaManager : public MmcReferable {
public:
    explicit MmcMetaManager(uint64_t defaultTtl, uint16_t evictThresholdHigh, uint16_t evictThresholdLow)
        : defaultTtlMs_(defaultTtl),
          evictThresholdHigh_(evictThresholdHigh),
          evictThresholdLow_(evictThresholdLow)
        {
        }

    ~MmcMetaManager() override
    {
        Stop();
    }

    Result Start()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (started_) {
            MMC_LOG_INFO("MmcMetaMgrProxyDefault already started");
            return MMC_OK;
        }
        globalAllocator_ = MmcMakeRef<MmcGlobalAllocator>();
        MMC_ASSERT_RETURN(globalAllocator_ != nullptr, MMC_MALLOC_FAILED);
        metaContainer_ = MmcMetaContainer<std::string, MmcMemObjMetaPtr>::Create();
        MMC_ASSERT_RETURN(metaContainer_ != nullptr, MMC_MALLOC_FAILED);
        started_ = true;
        removeThread_ = std::thread(std::bind(&MmcMetaManager::AsyncRemoveThreadFunc, this));
        return MMC_OK;
    }

    void Stop()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!started_) {
            return;
        }
        {
            std::lock_guard<std::mutex> lk(removeThreadLock_);
            started_ = false;
            removeThreadCv_.notify_all();
        }
        removeThread_.join();
        MMC_LOG_INFO("Stop MmcMetaManager");
    }

    /**
     * @brief Get the meta object and extend the lease
     * @param key          [in] key of the meta object
     * @param objMeta      [out] the meta object obtained
     */
    Result Get(const std::string& key, MmcMemObjMetaPtr& objMeta);

    /**
     * @brief Get multiple meta objects and extend their leases
     * @param keys          [in] List of keys for the meta objects to retrieve
     * @param objMetas      [out] Vector of pointers to the retrieved meta objects, corresponding to the keys in order
     * @param getResults    [out] Vector of results indicating the success/failure of retrieving each key's meta object
     */
    Result BatchGet(const std::vector<std::string> &keys, std::vector<MmcMemObjMetaPtr> &objMetas,
                    std::vector<Result> &getResults);

    /**
     * @brief Alloc the global memory space and create the meta object
     * @param key          [in] key of the meta object
     * @param metaInfo     [out] the meta object created
     */
    Result Alloc(const std::string& key, const AllocOptions& allocOpt, uint64_t requestId, MmcMemObjMetaPtr& objMeta);

    /**
     * @brief Alloc the global memory space and create the meta object
     * @param key          [in] key of the meta object
     * @param metaInfo     [out] the meta object created
     */
    Result BatchAlloc(const std::vector<std::string>& keys, const std::vector<AllocOptions>& allocOpts,
                      uint64_t requestId, std::vector<MmcMemObjMetaPtr>& objMetas, std::vector<Result>& allocResults);

    /**
     * @brief Update the state
     * @param req          [in] update state request
     */
    Result UpdateState(const std::string& key, const MmcLocation& loc, const BlobActionResult& actRet,
                       uint64_t operateId);

    /**
     * @brief Update the state
     * @param req          [in] update state request
     */
    Result BatchUpdateState(const std::vector<std::string>& keys, const std::vector<MmcLocation>& locs,
                            const std::vector<BlobActionResult>& actRets, uint64_t operateId,
                            std::vector<Result>& updateResults);

    /**
     * @brief remove the meta object
     * @param key          [in] key of the to-be-removed meta object
     */
    Result Remove(const std::string &key);

    /**
     * @brief Batch remove multiple meta objects
     * @param keys          [in] List of keys of the to-be-removed meta objects
     * @param results       [out] Results of each removal operation
     */
    Result BatchRemove(const std::vector<std::string> &keys, std::vector<Result> &results);

    /**
     * @brief unmount new mem pool contributor
     * @param loc               [in] location of the new mem pool contributor
     * @param localMemInitInfo  [in] info of the new mem pool contributor
     * @param blobMap           [in] if not empty, the allocator will be rebuild from blobMap
     */
    Result Mount(const MmcLocation &loc, const MmcLocalMemlInitInfo &localMemInitInfo,
        std::map<std::string, MmcMemBlobDesc> &blobMap);

    /**
     * @brief unmount the mempool contributor at given location
     * @param loc          [in] location of the mem pool contributor to be unmounted
     */
    Result Unmount(const MmcLocation &loc);

    /**
     * @brief Check if a meta object (key) is in memory
     * @param key          [in] key of the meta object
     */
    Result ExistKey(const std::string &key);

    /**
     * @brief Check if a list of meta objects (keys) is in memory
     * @param keys          [in] keys of the meta objects
     * @param results       [out] the accessible state corresponding to each key
     */
    Result BatchExistKey(const std::vector<std::string> &keys, std::vector<Result> &results);

    /**
     * @brief Get blob query info with key
     * @param key            [in] key of the meta object
     * @param queryInfo      [out] the query info of the meta object
     */
    Result Query(const std::string &key, MemObjQueryInfo &queryInfo);

    inline uint64_t Ttl()
    {
        return defaultTtlMs_;
    }

private:
    Result RebuildMeta(std::map<std::string, MmcMemBlobDesc> &blobMap);

    void AsyncRemoveThreadFunc();

    void CheckAndEvict();

    std::mutex mutex_;
    bool started_ = false;
    MmcRef<MmcMetaContainer<std::string, MmcMemObjMetaPtr>> metaContainer_;
    MmcGlobalAllocatorPtr globalAllocator_;
    std::thread removeThread_;
    std::mutex removeThreadLock_;
    std::condition_variable removeThreadCv_;
    std::mutex removeListLock_;
    std::list<std::pair<std::string, MmcMemObjMetaPtr>> removeList_;
    uint64_t defaultTtlMs_; /* defult ttl in miliseconds*/
    uint16_t evictThresholdHigh_;
    uint16_t evictThresholdLow_;
};
using MmcMetaManagerPtr = MmcRef<MmcMetaManager>;
}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_META_MANAGER_H
