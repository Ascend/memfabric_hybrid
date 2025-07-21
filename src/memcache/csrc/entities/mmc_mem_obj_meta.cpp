#include "mmc_mem_obj_meta.h"
#include <algorithm>
#include <chrono>

namespace ock {
namespace mmc {

// TODO: 此处去重成本比较高，先不做
Result MmcMemObjMeta::AddBlob(const MmcMemBlobPtr &blob)
{
    if (numBlobs_ != 0 && size_ != blob->Size()) {
        return MMC_ERROR;
    }

    //std::lock_guard<Spinlock> guard(spinlock_);
    if (numBlobs_ < MAX_NUM_BLOB_CHAINS) {
        for (size_t i = 0; i < MAX_NUM_BLOB_CHAINS; ++i) {
            if (blobs_[i] == nullptr) {
                blobs_[i] = blob;
                numBlobs_++;
                size_ = blob->Size();
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
    return MMC_OK;
}

Result MmcMemObjMeta::RemoveBlobs(const MmcBlobFilterPtr &filter, bool revert)
{
    //std::lock_guard<Spinlock> guard(spinlock_);
    uint8_t oldNumBlobs = numBlobs_;
    for (size_t i = 0; i < MAX_NUM_BLOB_CHAINS - 1; ++i) {
        if (blobs_[i] != nullptr && blobs_[i]->MatchFilter(filter) ^ revert) {
            blobs_[i] = nullptr;
            numBlobs_--;
        }
    }

    MmcMemBlobPtr pre = blobs_[MAX_NUM_BLOB_CHAINS - 1];
    while (pre != nullptr && pre->Next() != nullptr) {
        MmcMemBlobPtr cur = pre->Next();
        if (cur->MatchFilter(filter) ^ revert) {
            pre->Next() = cur->Next();
            cur = nullptr;
            numBlobs_--;
        }
        pre = pre->Next();
    }

    MmcMemBlobPtr head = blobs_[MAX_NUM_BLOB_CHAINS - 1];
    if (head != nullptr && head->MatchFilter(filter) ^ revert) {
        blobs_[MAX_NUM_BLOB_CHAINS - 1] = head->Next();
        head = nullptr;
        numBlobs_--;
    }

    return numBlobs_ < oldNumBlobs ? MMC_OK : MMC_ERROR;
}

std::vector<MmcMemBlobPtr> MmcMemObjMeta::GetBlobs(const MmcBlobFilterPtr &filter, bool revert)
{
    std::vector<MmcMemBlobPtr> blobs;
    for (size_t i = 0; i < MAX_NUM_BLOB_CHAINS; ++i) {
        auto curBlob = blobs_[i];
        if (curBlob != nullptr && (curBlob->MatchFilter(filter) ^ revert)) {
            blobs.push_back(curBlob);
        }
    }
    return blobs;
}

}  // namespace mmc
}  // namespace ock