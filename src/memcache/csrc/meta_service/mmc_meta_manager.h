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

struct MmcMemMetaDesc {
    uint16_t prot_{0};
    uint8_t priority_{0};
    uint8_t numBlobs_{0};
    uint64_t size_{0};
    std::vector<MmcMemBlobDesc> blobs_;

    MmcMemMetaDesc() = default;
    MmcMemMetaDesc(const uint16_t& prot, const uint8_t& priority, const uint8_t& numBlobs, const uint64_t& size)
        : prot_(prot), priority_(priority), numBlobs_(numBlobs), size_(size)
    {}

    MmcMemMetaDesc(const uint16_t& prot, const uint8_t& priority, const uint8_t& numBlobs, const uint64_t& size,
                   const std::vector<MmcMemBlobPtr>& blobs)
        : prot_(prot), priority_(priority), numBlobs_(numBlobs), size_(size)
    {
        for (const auto& blob : blobs) {
            AddBlob(blob);
        }
    }

    void AddBlob(const MmcMemBlobPtr& blob) { blobs_.push_back(blob->GetDesc()); }

    void AddBlobs(const std::vector<MmcMemBlobPtr>& blobs)
    {
        for (const auto& blob : blobs) {
            AddBlob(blob);
        }
    }

    uint16_t Prot() { return prot_; };

    uint8_t Priority() { return priority_; };

    uint8_t NumBlobs() { return numBlobs_; };

    uint64_t Size() { return size_; };
};

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
    Result Get(const std::string& key, uint64_t operateId, MmcBlobFilterPtr filterPtr, MmcMemMetaDesc& objMeta);

    /**
     * @brief Alloc the global memory space and create the meta object
     * @param key          [in] key of the meta object
     * @param metaInfo     [out] the meta object created
     */
    Result Alloc(const std::string& key, const AllocOptions& allocOpt, uint64_t operateId, MmcMemMetaDesc& objMeta);

    /**
     * @brief Update the state
     * @param req          [in] update state request
     */
    Result UpdateState(const std::string& key, const MmcLocation& loc, const BlobActionResult& actRet,
                       uint64_t operateId);

    /**
     * @brief remove the meta object
     * @param key          [in] key of the to-be-removed meta object
     */
    Result Remove(const std::string &key);

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
