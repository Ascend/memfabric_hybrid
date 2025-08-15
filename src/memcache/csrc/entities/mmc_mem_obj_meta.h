/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_MEM_OBJ_META_H
#define MEM_FABRIC_MMC_MEM_OBJ_META_H

#include "mmc_mem_blob.h"
#include "mmc_meta_lease_manager.h"
#include "mmc_montotonic.h"
#include "mmc_msg_packer.h"
#include "mmc_ref.h"
#include "mmc_spinlock.h"
#include "mmc_global_allocator.h"
#include <vector>
#include <mutex>

namespace ock {
namespace mmc {

static const uint16_t MAX_NUM_BLOB_CHAINS = 5;  // to make sure MmcMemObjMeta <= 64 bytes

class MmcMemObjMeta : public MmcReferable {
public:
    MmcMemObjMeta() = default;
    MmcMemObjMeta(uint16_t prot, uint8_t priority, uint8_t numBlobs, std::vector<MmcMemBlobPtr> blobs,
                  uint64_t size)
        : prot_(prot), priority_(priority), numBlobs_(numBlobs)
    {
        for (auto &blob : blobs) {
            AddBlob(blob);
        }
    }
    ~MmcMemObjMeta() override = default;

    /**
     * @brief Add blob info into the mem object,
     * for replicated blob on different rank ids
     *
     * @param blob         [in] blob pointer to be added
     */
    Result AddBlob(const MmcMemBlobPtr &blob);

    /**
     * @brief Free blobs from the mem object by filter
     *
     * @param allocator allocator ptr
     * @return 0 if removed
     */
    Result FreeBlobs(const std::string &key, MmcGlobalAllocatorPtr &allocator,
                     const MmcBlobFilterPtr &filter = nullptr, bool doBackupRemove = true);

    /**
     * @brief Get the prot
     * @return prot
     */
    uint16_t Prot();

    /**
     * @brief Get the priority
     * @return priority
     */
    uint8_t Priority();

    /**
     * @brief Get the number of blobs
     * @return number of blobs
     */
    uint16_t NumBlobs();

    /**
     * @brief Get blobs with filter
     * @return blobs passing the filter
     */
    std::vector<MmcMemBlobPtr> GetBlobs(const MmcBlobFilterPtr &filter = nullptr, bool revert = false);

    void GetBlobsDesc(std::vector<MmcMemBlobDesc>& blobsDesc, const MmcBlobFilterPtr& filter = nullptr,
                      bool revert = false);

    Result UpdateBlobsState(const std::string& key, const MmcBlobFilterPtr& filter, uint64_t operateId,
                            BlobActionResult actRet);

    /**
     * @brief Get the size
     * @return size
     */
    uint64_t Size();

    friend std::ostream &operator<<(std::ostream &os, MmcMemObjMeta &obj)
    {
        os << "MmcMemObjMeta{numBlobs=" << static_cast<int>(obj.numBlobs_) << ",size=" << obj.size_
           << ",priority=" << obj.priority_ << ",prot=" << obj.prot_ << "}";
        return os;
    }

private:
    Result RemoveBlobs(const MmcBlobFilterPtr &filter = nullptr, bool revert = false);

private:
    /* make sure the size of this class is 64 bytes */
    uint16_t prot_{0};                         /* prot of the mem object, i.e. accessibility */
    uint8_t priority_{0};                      /* priority of the memory object, used for eviction */
    uint8_t numBlobs_{0};                      /* number of blob that the memory object, i.e. replica count */
    MmcMemBlobPtr blobs_[MAX_NUM_BLOB_CHAINS]; /* pointers of blobs */
    uint64_t size_{0};                         /* byteSize of each blob */
};

using MmcMemObjMetaPtr = MmcRef<MmcMemObjMeta>;

inline uint16_t MmcMemObjMeta::Prot()
{
    return prot_;
}

inline uint8_t MmcMemObjMeta::Priority()
{
    return priority_;
}

inline uint16_t MmcMemObjMeta::NumBlobs()
{
    return numBlobs_;
}

inline uint64_t MmcMemObjMeta::Size()
{
    return size_;
}

}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_MEM_OBJ_META_H
