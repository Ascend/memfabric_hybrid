/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_NET_CTX_STORE_H
#define MEM_FABRIC_MMC_NET_CTX_STORE_H

#include "mmc_common_includes.h"
#include "mmc_net_common_acc.h"

namespace ock {
namespace mmc {

constexpr uint32_t TRY_GET_FLAT_TIME = 3; // Try to get empty flat bucket 3 times

class NetContextStore : public MmcReferable {
public:
    explicit NetContextStore(uint32_t flatCapacity) : mFlatCapacity(flatCapacity) {}

    ~NetContextStore() override
    {
        UnInitialize();
    }

    /*
     * @brief Initialize the ctx store
     *
     * @return 0 return if successful
     */
    Result Initialize() noexcept
    {
        /* validate the capacity */
        if (mFlatCapacity < UN128) {
            mFlatCapacity = UN128;
        } else if (mFlatCapacity > UN16777216) {
            mFlatCapacity = UN16777216; /* each bucket is an uint64_t, 128MB is occupied */
        }

        /* get aligned capacity */
        mFlatCapacity = 1 << (UN32 - __builtin_clz(mFlatCapacity) - 1);
        /* get seqNo mask */
        mSeqNoMask = mFlatCapacity - 1;
        /* get version shift for move right */
        mVersionShift = __builtin_popcount(mSeqNoMask);
        /* get version and seqNo mask, as version occupied 6 bits */
        mSeqNoAndVersionMask = (1 << (mVersionShift + gVersionBitWidth)) - 1;

        mFlatCtxBucks = new (std::nothrow) uint64_t[mFlatCapacity];
        if (mFlatCtxBucks == nullptr) {
            MMC_LOG_ERROR("Failed to new service flat context buckets, probably out of memory");
            return MMC_NEW_OBJECT_FAILED;
        }

        /* make physical memory allocated and set them to 0 */
        bzero(mFlatCtxBucks, sizeof(uint64_t) * mFlatCapacity);

        /* reserved hash bucket for unordered map */
        for (auto &i : mHashCtxMap) {
            i.reserve(N1024);
        }
        MMC_LOG_INFO("Initialized context store, flatten capacity " << mFlatCapacity << ", versionAndSeqMask " <<
            mSeqNoAndVersionMask << ", seqNoMask " << mSeqNoMask << ", seqNoAndVersionIndex " << mSeqNoAndVersionIndex);

        return MMC_OK;
    }

    void UnInitialize()
    {
        if (mFlatCtxBucks != nullptr) {
            delete[] mFlatCtxBucks;
            mFlatCtxBucks = nullptr;
        }
    }

    /*
     * @brief Create a seq no, and store it
     *
     * @param ctx          [in] ctx ptr to store
     * @param output       [out] seqNo created
     *
     * @return K_OK if successful
     * SER_INVALID_PARAM if param is invalid
     * SER_STORE_SEQ_DUP if seq is duplicated in map
     */
    template <typename T> Result PutAndGetSeqNo(T *ctx, uint32_t &output)
    {
        if (UNLIKELY(ctx == nullptr)) {
            return MMC_INVALID_PARAM;
        }

        auto value = reinterpret_cast<uint64_t>(ctx);
        /* pre-defined variables because of goto */
        NetSeqNo sn(0);
        uint32_t mapIndex = 0;

        /*
         * Try to get empty flat bucket 3 times,
         * if got emtpy bucket, store it in that flat bucket,
         * if not got, store it into hash map
         *
         * Note: don't do this in a loop (i.e. while), expanded code has better performance than loop
         *
         * step1: first time to get free flat bucket according to index
         */

        uint32_t newSeqAndVersion = 0;
        uint32_t seqNo = 0;
        uint64_t version = 0;
        for (uint32_t i = 0; i < TRY_GET_FLAT_TIME; i++) {
            /* get the seqNo with increasing and mask, if the seqNo is 0, increase again */
            newSeqAndVersion = __sync_fetch_and_add(&mSeqNoAndVersionIndex, 1);
            if (UNLIKELY(newSeqAndVersion & mSeqNoMask) == 0) {
                newSeqAndVersion = __sync_fetch_and_add(&mSeqNoAndVersionIndex, 1);
            }

            /* get seqNo and version, and mixed value with version and ctx ptr for CAS */
            seqNo = newSeqAndVersion & mSeqNoMask;
            version = (newSeqAndVersion >> mVersionShift) & gVersionMask;
            value = (version << UN58) | value; /* high 6 bits store version */
            if (__sync_bool_compare_and_swap(&mFlatCtxBucks[seqNo], 0, value)) {
                sn.SetValue(1, static_cast<uint32_t>(version), seqNo);
                output = sn.wholeSeq;
                /* increase ref count */
                ctx->IncreaseRef();
                return MMC_OK;
            }
        }

        /* tried 3 times no luck to get an empty bucket, store in hash map */
        mapIndex = seqNo % gHashCount;
        sn.SetValue(0, static_cast<uint32_t>(version), seqNo);
        output = sn.wholeSeq;
        {
            std::lock_guard<std::mutex> guard(mHashCtxMutex[mapIndex]);
            bool inserted = mHashCtxMap[mapIndex].emplace(sn.wholeSeq, value).second;
            if (inserted) {
                /* increase ref count */
                ctx->IncreaseRef();
            }
            return inserted ? MMC_OK : MMC_NET_SEQ_DUP;
        }
    }

    /*
     * @brief Get the pointer of ctx with seqNo and clean it
     *
     * @param seqNo        [in] seqNo, which whole got from response and timer
     * @param out          [out] ctx ptr
     *
     * @return MMC_OK if successful
     * SER_INVALID_PARAM if param is invalid
     * SER_STORE_SEQ_NO_FOUND if seq is not existed, probably removed already
     *
     */
    template <typename T> Result GetSeqNoAndRemove(uint32_t seqNo, T *&out, bool decreaseRef = true)
    {
        NetSeqNo no(0);
        no.wholeSeq = seqNo;

        if (LIKELY(no.fromFlat == 1)) {
            /* create the old pointer and */
            uint64_t value = mFlatCtxBucks[no.realSeq] & gPtrMask;
            uint64_t tmpVersion = no.version;

            /* if timeout thread already get seq no, next time will
               1、CAS OK, but get value is 0
               2、CAS ERR by version++ */
            if (__sync_bool_compare_and_swap(&mFlatCtxBucks[no.realSeq], (tmpVersion << UN58) | value, 0)) {
                if (UNLIKELY(value == 0)) {
                    return MMC_NET_SEQ_NO_FOUND;
                }

                out = reinterpret_cast<T *>(value);
                /* decrease ref count */
                if (decreaseRef) {
                    out->DecreaseRef();
                }
                return MMC_OK;
            }

            return MMC_NET_SEQ_NO_FOUND;
        }

        uint32_t mapIndex = no.realSeq % gHashCount;
        no.isResp = 0;
        {
            std::lock_guard<std::mutex> guard(mHashCtxMutex[mapIndex]);
            auto iter = mHashCtxMap[mapIndex].find(no.wholeSeq);
            if (LIKELY(iter != mHashCtxMap[mapIndex].end())) {
                out = reinterpret_cast<T *>(iter->second & gPtrMask);
                /* decrease ref count */
                if (decreaseRef) {
                    out->DecreaseRef();
                }
                mHashCtxMap[mapIndex].erase(iter);
                return MMC_OK;
            }
        }

        return MMC_NET_SEQ_NO_FOUND;
    }

    template <typename T> inline void RemoveSeqNo(uint32_t seqNo)
    {
        T *out = nullptr;
        if (UNLIKELY(GetSeqNoAndRemove<T>(seqNo, out) != MMC_OK)) {
            NetSeqNo dumpSeq(seqNo);
            MMC_LOG_ERROR("Failed to remove ctx with seqNo " << dumpSeq.ToString() << " as not found");
            return;
        }
    }

private:
    static constexpr uint32_t gVersionMask = 0x3F;           /* mask to reverse version */
    static constexpr uint32_t gVersionBitWidth = 6L;         /* mask to reverse version */
    static constexpr uint32_t gHashCount = 4L;               /* hash map count */
    static constexpr uint64_t gPtrMask = 0x03FFFFFFFFFFFFFF; /* ptr mask */

private:
    /* Note:
     * 1 make sure those frequently accessed variables are at first place
     * 2 make sure those variables are aligned
     * 3 make sure total size of those variables are less than the size of 1 cache line
     */
    uint32_t mSeqNoAndVersionIndex = 1; /* atomic increase seqNo and version */
    uint32_t mSeqNoAndVersionMask = 0;  /* mask to reverse the seqNo and version */
    uint32_t mSeqNoMask = 0;            /* mask to reverse the seqNo */
    uint32_t mVersionShift = 0;         /* move right shift num to get version */
    uint32_t mFlatCapacity = N8192;     /* flat array capacity */
    uint64_t *mFlatCtxBucks = nullptr;  /* actually array to store the ptr */

    std::mutex mHashCtxMutex[gHashCount];                           /* mutex to guard unordered_map */
    std::unordered_map<uint32_t, uint64_t> mHashCtxMap[gHashCount]; /* unordered_map to store un-flat */
};
}
}

#endif //MEM_FABRIC_MMC_NET_CTX_STORE_H
