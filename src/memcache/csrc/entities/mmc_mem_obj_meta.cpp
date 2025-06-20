#include "mmc_mem_obj_meta.h"
#include "mmc_lookup_map.h" // just for test
#include <algorithm>
#include <chrono>

namespace ock {
namespace mmc {

void MmcMemObjMeta::AddBlob(const MmcMemBlobPtr& blob)
{
    std::lock_guard<Spinlock> guard(spinlock_);
    if (numBlobs_ < MAX_NUM_BLOB_CHAINS) {
        for (size_t i = 0; i < MAX_NUM_BLOB_CHAINS; ++i) {
            if (blobs_[i] == nullptr) {
                blobs_[i] = blob;
                numBlobs_++;
                break;
            }
        }
    } else {
        MmcMemBlobPtr last = blobs_[MAX_NUM_BLOB_CHAINS - 1];
        while (last->Next() != nullptr) {
            last = last->Next();
        }
        last->Next(blob);
        numBlobs_++;
    }
}

Result MmcMemObjMeta::RemoveBlob(const MmcMemBlobPtr& blob)
{
    if (blob == nullptr) {
        return MMC_INVALID_PARAM;
    }

    std::lock_guard<Spinlock> guard(spinlock_);
    for (size_t i = 0; i < MAX_NUM_BLOB_CHAINS; ++i) {
        if (blobs_[i] == blob) {
            blobs_[i] = nullptr;
            numBlobs_--;
            return MMC_OK;
        }
    }
    MmcMemBlobPtr last = blobs_[MAX_NUM_BLOB_CHAINS - 1];
    while (last != nullptr) {
        last = last->Next();
        if (last == blob) {
            last = nullptr;
            numBlobs_--;
            return MMC_OK;
        }
    }
    return MMC_ERROR;
}

} // namespace mmc
} // namespace ock