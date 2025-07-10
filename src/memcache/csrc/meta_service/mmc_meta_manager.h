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

namespace ock {
namespace mmc {

static const uint16_t NUM_BUCKETS = 29;

class MmcMetaManager : public MmcReferable {
public:
    MmcMetaManager(uint64_t defaultTtl) : defaultTtl_(defaultTtl)
    {
        globalAllocator_ = MmcMakeRef<MmcGlobalAllocator>();
    };

    /**
     * @brief Get the meta object and extend the lease
     * @param key          [in] key of the meta object
     * @param objMeta      [out] the meta object obtained
     */
    Result Get(const std::string &key, MmcMemObjMetaPtr &objMeta);

    /**
     * @brief Alloc the global memeory space and create the meta object
     * @param key          [in] key of the meta object
     * @param metaInfo     [out] the meta object created
     */

    Result Alloc(const std::string &key, const AllocOptions &allocOpt, MmcMemObjMetaPtr &objMeta);

    /**
     * @brief Get the blobs by key and the filter, and renew the lease
     * @param key          [in] key of the meta object
     * @param filter       [in] filter used to choose the blobs
     * @param blobs        [out] the blobs found by the key and filter
     */
    Result GetBlobs(const std::string &key, const MmcBlobFilterPtr &filter, std::vector<MmcMemBlobPtr> &blobs);

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
     */
    Result ExistKey(const std::string &key);

    /**
     * @brief Check if a list of meta objects (keys) is in memory
     * @param keys          [in] keys of the meta objects
     * @param results       [out] the accessible state corresponding to each key
     */
    Result BatchExistKey(const std::vector<std::string> &keys, std::vector<Result> &results);

private:
    /**
     * @brief force remove the blobs and object meta(if all its blobs are removed)
     * @param key          [in] key of the to-be-removed meta object
     * @param filter       [in] filter used to choose the to-be-removed blobs
     */
    Result ForceRemoveBlobs(const MmcMemObjMetaPtr &objMeta, const MmcBlobFilterPtr &filter = nullptr);

    MmcLookupMap<std::string, MmcMemObjMetaPtr, NUM_BUCKETS> objMetaLookupMap_;
    MmcGlobalAllocatorPtr globalAllocator_;
    uint64_t defaultTtl_; /* defult ttl in miliseconds*/
};
using MmcMetaManagerPtr = MmcRef<MmcMetaManager>;
}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_META_MANAGER_H
