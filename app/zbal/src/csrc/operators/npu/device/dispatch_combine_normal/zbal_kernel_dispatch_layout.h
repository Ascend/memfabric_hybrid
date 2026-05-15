/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#ifndef ZBAL_KERNEL_DISPATCH_LAYOUT_H
#define ZBAL_KERNEL_DISPATCH_LAYOUT_H

#include <climits>
#include "kernel_operator.h"
#include "zbal_def.h"
#include "zbal_kernel_utils.h"
#include "zbal_kernel_constant.h"

namespace MoeDispatchLayout {
using namespace AscendC;
using namespace Moe;

template<AscendC::HardEvent event>
ZBAL_KERNEL void SyncFunc()
{
    int32_t eventID = static_cast<int32_t>(GetTPipePtr()->FetchEventID(event));
    AscendC::SetFlag<event>(eventID);
    AscendC::WaitFlag<event>(eventID);
}

constexpr uint16_t MAX_BLOCK = 50U;

template<typename T>
class DispatchLayout {
public:
    ZBAL_KERNEL DispatchLayout() {};

    ZBAL_KERNEL void Init(GM_ADDR topkIdx, uint32_t numTokens, uint32_t numExperts, uint32_t numTopk, uint32_t numRanks,
                          uint32_t rank, GM_ADDR numTokensPerRank, GM_ADDR numTokensPerExpert, GM_ADDR isTokenInRank,
                          GM_ADDR sendTokenIdx, GM_ADDR notifySendData, TPipe *pipe)
    {
        numTokens_ = numTokens;
        numRanks_ = numRanks;
        rankId_ = rank;
        numExperts_ = numExperts;
        numTopk_ = numTopk;
        pipe_ = pipe;

        blockIdx_ = GetBlockIdx();
        uint32_t maxAivNum = GetBlockNum();
        blockNum_ = numTokens_ <= maxAivNum ? numTokens_ : maxAivNum;
        uint32_t initBlockNum = blockNum_ == 0 ? 1 : blockNum_;
        if (blockIdx_ >= initBlockNum) {
            SyncAll<true>();
            return;
        }
        uint32_t temp = numTokens_ / initBlockNum;
        uint32_t restNum = numTokens_ % initBlockNum;
        tempTokens_ = temp; // 1
        if (blockIdx_ < restNum) {
            tempTokens_++;
        }
        topkIdx32AlignIntLen_ = Ceil(tempTokens_ * numTopk_ * sizeof(int32_t), UB_ALIGN) * UB_ALIGN;   // 32
        topkIdx64AlignIntLen_ = Ceil(tempTokens_ * numTopk_ * sizeof(int64_t), UB_ALIGN) * UB_ALIGN;   // 64
        numTokensPerRank32AlignIntLen_ = Ceil(numRanks_ * sizeof(T), UB_ALIGN) * UB_ALIGN;             // 32
        numTokensPerExpert32AlignIntLen_ = Ceil(numExperts_ * sizeof(T), UB_ALIGN) * UB_ALIGN;         // 128
        isTokenInRank32AlignIntLen_ = Ceil(tempTokens_ * numRanks_ * sizeof(T), UB_ALIGN) * UB_ALIGN;  // 32
        sendTokenIdx32AlignIntLen_ = Ceil(tempTokens_ * numExperts_ * sizeof(T), UB_ALIGN) * UB_ALIGN; // 128

        int64_t topkIdxOffset; // 0, 64, 128, ...
        int64_t isTokenOffset; // 0, 8, 16, ...
        if (blockIdx_ < restNum) {
            topkIdxOffset = blockIdx_ * tempTokens_ * numTopk_ * sizeof(int64_t);
            isTokenOffset = blockIdx_ * tempTokens_ * numRanks_ * sizeof(T);
        } else {
            topkIdxOffset = (restNum + blockIdx_ * tempTokens_) * numTopk_ * sizeof(int64_t);
            isTokenOffset = (restNum + blockIdx_ * tempTokens_) * numRanks_ * sizeof(T);
        }

        topkIdxGM_.SetGlobalBuffer((__gm__ int64_t *)(topkIdx + topkIdxOffset));
        numTokensPerRankGM_.SetGlobalBuffer((__gm__ T *)numTokensPerRank);
        numTokensPerExpertGM_.SetGlobalBuffer((__gm__ T *)numTokensPerExpert);
        isTokenInRankGM_.SetGlobalBuffer((__gm__ T *)(isTokenInRank + isTokenOffset));
        sendTokenIdxGM_.SetGlobalBuffer((__gm__ T *)(sendTokenIdx + topkIdxOffset / 2));
        tempExpertGM_.SetGlobalBuffer((__gm__ T *)notifySendData);

        // reset output GM
        pipe_->InitBuffer(tmpBuf_, numTokensPerExpert32AlignIntLen_); // experts是ranks的整数倍，按照大的初始化
        LocalTensor<T> zerosLt = tmpBuf_.Get<T>();
        Duplicate<T>(zerosLt, 0, numTokensPerExpert32AlignIntLen_ / sizeof(T));
        SyncFunc<AscendC::HardEvent::V_MTE3>();
        DataCopyExtParams expCopyParams{1U, static_cast<uint32_t>(numExperts_ * sizeof(T)), 0U, 0U, 0U};
        DataCopyPad(numTokensPerExpertGM_, zerosLt, expCopyParams);
        DataCopyExtParams multiExpCopyParams{MAX_BLOCK, static_cast<uint32_t>(numExperts_ * sizeof(T)), 0U, 0U, 0U};
        for (int i = 0; i < static_cast<int>(MAX_BLOCK); ++i) { // reset tempExpertGM_ 所有区域
            DataCopyPad(tempExpertGM_[i * numExperts_], zerosLt, expCopyParams);
        }
        DataCopyExtParams rankCopyParams{1U, static_cast<uint32_t>(numRanks_ * sizeof(T)), 0U, 0U, 0U};
        DataCopyPad(numTokensPerRankGM_, zerosLt, rankCopyParams);
        SyncAll<true>();
    }

    ZBAL_KERNEL void Process()
    {
        if (blockIdx_ >= blockNum_) {
            SyncAll<true>();
            return;
        }
        pipe_->Reset();
        pipe_->InitBuffer(topkIdxBuf_, topkIdx64AlignIntLen_);
        pipe_->InitBuffer(numTokensPerRankBuf_, numTokensPerRank32AlignIntLen_);
        pipe_->InitBuffer(numTokensPerExpertBuf_, numTokensPerExpert32AlignIntLen_);
        pipe_->InitBuffer(isTokenInRankBuf_, isTokenInRank32AlignIntLen_);
        pipe_->InitBuffer(seenRankBuf_, numTokensPerRank32AlignIntLen_);
        pipe_->InitBuffer(sendTokenIdxBuf_, topkIdx32AlignIntLen_);

        LocalTensor<int64_t> topkIdxTensor = topkIdxBuf_.Get<int64_t>();
        LocalTensor<T> numTokensPerRankTensor = numTokensPerRankBuf_.Get<T>();
        LocalTensor<T> numTokensPerExpertTensor = numTokensPerExpertBuf_.Get<T>();
        LocalTensor<T> isTokenInRankTensor = isTokenInRankBuf_.Get<T>();
        LocalTensor<T> seenRankTensor = seenRankBuf_.Get<T>();
        LocalTensor<T> sendTokenIdxTensor = sendTokenIdxBuf_.Get<T>();
        Duplicate<T>(numTokensPerRankTensor, 0, numTokensPerRank32AlignIntLen_ / sizeof(T));
        Duplicate<T>(numTokensPerExpertTensor, 0, numTokensPerExpert32AlignIntLen_ / sizeof(T));
        Duplicate<T>(isTokenInRankTensor, 0, isTokenInRank32AlignIntLen_ / sizeof(T));
        SyncFunc<AscendC::HardEvent::V_S>();

        DataCopyExtParams dataCopyParams{1U, static_cast<uint32_t>(tempTokens_ * numTopk_ * sizeof(int64_t)), 0U, 0U,
                                         0U};
        DataCopyPadExtParams<int64_t> padParams{false, 0U, 0U, 0U};
        SyncFunc<AscendC::HardEvent::MTE3_MTE2>();
        DataCopyPad(topkIdxTensor, topkIdxGM_, dataCopyParams, padParams);
        SyncFunc<AscendC::HardEvent::MTE2_S>();

        int expertPerRank = numExperts_ / numRanks_;
        for (int i = 0; i < tempTokens_; ++i) {
            SyncFunc<AscendC::HardEvent::S_V>();
            Duplicate<T>(seenRankTensor, 0, numRanks_);
            SyncFunc<AscendC::HardEvent::V_S>();
            for (int j = 0; j < numTopk_; ++j) {
                int64_t expertId = topkIdxTensor.GetValue(i * numTopk_ + j);
                if (expertId < 0 || expertId >= numExperts_) {
                    continue;
                }
                uint32_t perExpertNum = numTokensPerExpertTensor.GetValue(expertId) + 1;
                numTokensPerExpertTensor.SetValue(expertId, perExpertNum);
                int rankId = expertId / expertPerRank;
                if (!seenRankTensor.GetValue(rankId)) {
                    uint32_t perRankNum = numTokensPerRankTensor.GetValue(rankId) + 1;
                    isTokenInRankTensor.SetValue(i * numRanks_ + rankId, 1);
                    seenRankTensor.SetValue(rankId, 1);
                    numTokensPerRankTensor.SetValue(rankId, perRankNum);
                }
            }
        }
        SyncFunc<AscendC::HardEvent::MTE2_MTE3>();

        uint32_t sendSize = tempTokens_ * numRanks_ * sizeof(T);
        DataCopyExtParams isTokenInRankDataCopyParams{1U, sendSize, 0U, 0U, 0U};
        DataCopyPad(isTokenInRankGM_, isTokenInRankTensor, isTokenInRankDataCopyParams);

        AscendC::SetAtomicAdd<T>();
        DataCopyExtParams tempDataCopyParams{1U, static_cast<uint32_t>(numExperts_ * sizeof(T)), 0U, 0U, 0U};
        for (int i = blockIdx_ + 1; i < blockNum_; ++i) {
            DataCopyPad(tempExpertGM_[i * numExperts_], numTokensPerExpertTensor, tempDataCopyParams);
        }
        DataCopyExtParams perRankDataCopyParams{1U, static_cast<uint32_t>(numRanks_ * sizeof(T)), 0U, 0U, 0U};
        DataCopyPad(numTokensPerRankGM_, numTokensPerRankTensor, perRankDataCopyParams);
        DataCopyExtParams perExpertDataCopyParams{1U, static_cast<uint32_t>(numExperts_ * sizeof(T)), 0U, 0U, 0U};
        DataCopyPad(numTokensPerExpertGM_, numTokensPerExpertTensor, perExpertDataCopyParams);
        AscendC::SetAtomicNone();
        PipeBarrier<PIPE_MTE3>();
        SyncAll<true>();

        SyncFunc<AscendC::HardEvent::MTE3_MTE2>();
        DataCopyPadExtParams<T> tempPadParams{false, 0U, 0U, 0U};
        DataCopyPad(numTokensPerExpertTensor, tempExpertGM_[blockIdx_ * numExperts_], tempDataCopyParams,
                    tempPadParams);

        SyncFunc<AscendC::HardEvent::MTE2_S>();
        for (int i = 0; i < tempTokens_; ++i) {
            for (int j = 0; j < numTopk_; ++j) {
                int64_t expertId = topkIdxTensor.GetValue(i * numTopk_ + j);
                if (expertId < 0 || expertId >= numExperts_) {
                    continue;
                }
                T valT = numTokensPerExpertTensor(expertId);
                sendTokenIdxTensor(i * numTopk_ + j) = valT;
                numTokensPerExpertTensor(expertId) = valT + 1;
            }
        }
        SyncFunc<AscendC::HardEvent::S_MTE3>();

        DataCopyExtParams topkCopyParams{1U, static_cast<uint32_t>(tempTokens_ * numTopk_ * sizeof(T)), 0U, 0U, 0U};
        DataCopyPad(sendTokenIdxGM_, sendTokenIdxTensor, topkCopyParams);
        PipeBarrier<PIPE_ALL>();
    }

private:
    TPipe *pipe_{nullptr};
    uint32_t blockIdx_{0};
    uint32_t blockNum_{0};

    GlobalTensor<int64_t> topkIdxGM_;
    GlobalTensor<T> numTokensPerRankGM_;
    GlobalTensor<T> numTokensPerExpertGM_;
    GlobalTensor<T> isTokenInRankGM_;
    GlobalTensor<T> tempExpertGM_;
    GlobalTensor<T> sendTokenIdxGM_;

    TBuf<> topkIdxBuf_;
    TBuf<> numTokensPerRankBuf_;
    TBuf<> numTokensPerExpertBuf_;
    TBuf<> isTokenInRankBuf_;
    TBuf<> seenRankBuf_;
    TBuf<> sendTokenIdxBuf_;
    TBuf<> tmpBuf_;

    uint32_t numTokens_{0};
    uint32_t rankId_{0};
    uint32_t numRanks_{0};
    uint32_t numExperts_{0};
    uint32_t numTopk_{0};
    uint32_t tempTokens_{0};

    uint32_t topkIdx32AlignIntLen_{0};
    uint32_t topkIdx64AlignIntLen_{0};
    uint32_t numTokensPerRank32AlignIntLen_{0};
    uint32_t numTokensPerExpert32AlignIntLen_{0};
    uint32_t isTokenInRank32AlignIntLen_{0};
    uint32_t sendTokenIdx32AlignIntLen_{0};
};
} // namespace MoeDispatchLayout

#endif // ZBAL_KERNEL_DISPATCH_LAYOUT_H