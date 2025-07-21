/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_MEM_OBJ_META_H
#define MEM_FABRIC_MMC_MEM_OBJ_META_H

#include "mmc_mem_blob.h"
#include "mmc_msg_packer.h"
#include "mmc_ref.h"
#include "mmc_spinlock.h"
#include "mmc_montotonic.h"
#include "mmc_meta_lease_manager.h"
#include <vector>

namespace ock {
namespace mmc {

static const uint16_t MAX_NUM_BLOB_CHAINS = 5;  // to make sure MmcMemObjMeta <= 64 bytes

class MmcMemObjMeta : public MmcReferable {
public:
    MmcMemObjMeta() = default;
    MmcMemObjMeta(uint16_t prot, uint8_t priority, uint8_t numBlobs, std::vector<MmcMemBlobPtr> blobs, uint64_t lease,
                  uint32_t size)
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
     * @brief Remove blobs from the mem object by filter
     *
     * @param filter remove filter
     * @param revert false: remove those matching the filter; true: remove those not matching the filter
     * @return 0 if removed
     */
    Result RemoveBlobs(const MmcBlobFilterPtr &filter=nullptr, bool revert = false);

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

    /**
     * @brief Get the size
     * @return size
     */
    uint16_t Size();

    void Lock()
    {
        spinlock_.lock();
    }

    void Unlock()
    {
        spinlock_.unlock();
    }

    /**
     * @brief Get the query info
     * @return query info
     */
    MemObjQueryInfo QueryInfo();

private:
    /* make sure the size of this class is 64 bytes */
    uint16_t prot_{0};                         /* prot of the mem object, i.e. accessibility */
    uint8_t priority_{0};                      /* priority of the memory object, used for eviction */
    uint8_t numBlobs_{0};                      /* number of blob that the memory object, i.e. replica count */
    MmcMemBlobPtr blobs_[MAX_NUM_BLOB_CHAINS]; /* pointers of blobs */
    uint32_t size_{0};                         /* byteSize of each blob */
    Spinlock spinlock_;                        /* 4 bytes */
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

inline uint16_t MmcMemObjMeta::Size()
{
    return size_;
}

inline MemObjQueryInfo MmcMemObjMeta::QueryInfo()
{
    std::lock_guard<Spinlock> guard(spinlock_);
    return {size_, prot_, numBlobs_, true};
}

}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_MEM_OBJ_META_H
