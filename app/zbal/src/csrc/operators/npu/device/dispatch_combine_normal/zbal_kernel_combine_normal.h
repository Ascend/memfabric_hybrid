/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef ZBAL_KERNEL_COMBINE_NORMAL_H
#define ZBAL_KERNEL_COMBINE_NORMAL_H

#include <climits>
#include "kernel_operator.h"
#include "zbal_def.h"
#include "zbal_kernel_utils.h"
#include "zbal_kernel_comm_args.h"

namespace MoeCombineNormal {
using namespace AscendC;
using namespace Moe;

constexpr uint8_t BUFFER_NUM = 2;
constexpr uint64_t COMBINE_STATUS_OFFSET = 20UL * 1024UL;
constexpr uint32_t MUL_256_ALIGN = 256U;
constexpr uint64_t WIN_512_ALIGN = 512UL;

template <AscendC::HardEvent event>
ZBAL_KERNEL void SyncFunc()
{
    int32_t eventID = static_cast<int32_t>(GetTPipePtr()->FetchEventID(event));
    AscendC::SetFlag<event>(eventID);
    AscendC::WaitFlag<event>(eventID);
}

#define TypeClass typename RecvXType, typename XType, typename SrcInfoType
#define TypeFunc RecvXType, XType, SrcInfoType

template <TypeClass>
class CombineNormal {
public:
    ZBAL_KERNEL CombineNormal(){};
    ZBAL_KERNEL void Init(GM_ADDR metaAddr, GM_ADDR recvX, GM_ADDR epRecvCount, GM_ADDR topkWeights,
                           GM_ADDR topkIdx, GM_ADDR sendTokenIdx, GM_ADDR balanceMatrix, uint32_t rank,
                           uint32_t numExperts, uint32_t bs, uint32_t hidden, uint32_t topK, bool enableBalance,
                           GM_ADDR XOut, TPipe *pipe);
    ZBAL_KERNEL void Process();

private:
    ZBAL_KERNEL void ResetMetaState();
    ZBAL_KERNEL void PutShareAddr();
    ZBAL_KERNEL void SetSyncFlag(int metaType);
    ZBAL_KERNEL void WaitSyncFlag(int metaType);
    ZBAL_KERNEL void GetShareAddr();
    ZBAL_KERNEL void HandleAllRankToken();
    ZBAL_KERNEL void ReadAndWriteForTargetRank(uint32_t startId, uint32_t endId, uint32_t tokenCnt, uint32_t tarRankId);
    ZBAL_KERNEL void ReadTokenFromRemote();
    ZBAL_KERNEL void ReadTokenAndWeightedSum(uint32_t tokenIndex, uint32_t tarRankId);

    ZBAL_KERNEL GM_ADDR GetMetaAddrByRankId(const int32_t rankId, const int metaType)
    {
        auto ptr = zbal_ptr((__gm__ uint64_t *)(gva_gm), epRankId, rankId, localMemSize_, peerRanks);

        switch (metaType) {
            case STATE:  // 存放通信结束的state, 12KB
                return (GM_ADDR)(ptr) + stateOffset_;
            case ADDR:  // 存放交换的共享地址
                return (GM_ADDR)(ptr) + addrOffset_;
            case FLAG:  // 存放第一次清理state空间后的同步flag, 12KB
                return (GM_ADDR)(ptr) + flagOffset_;
            default:
                return (GM_ADDR)(ptr);
        }
    }

    ZBAL_KERNEL void SplitCoreCal(uint32_t totalNum, uint32_t &perCoreNum, uint32_t &startIdx, uint32_t &endIdx)
    {
        perCoreNum = totalNum / blockNum;
        uint32_t remainderRankNum = totalNum % blockNum;

        startIdx = perCoreNum * blockIdx;
        if (blockIdx < remainderRankNum) {
            perCoreNum++;
            startIdx += blockIdx;
        } else {
            startIdx += remainderRankNum;
        }
        endIdx = startIdx + perCoreNum;
    }

    uint32_t blockIdx{0};
    uint32_t blockNum{0};
    uint32_t rankNumPerBlock;
    uint32_t curBlockStartRankId;
    uint32_t curBlockEndRankId;

    uint32_t batchSize{0};
    uint32_t h{0};
    uint32_t topK{0};
    uint32_t epRankSize{0};
    uint32_t epRankId{0};
    uint32_t moeExpertNum{0};
    uint32_t moeExpertNumPerRank{0};
    bool isEnableBalance_{false};

    uint32_t hRecvXTypeLen_{0};
    uint32_t h32AlignFloatLen_{0};
    uint32_t h256AlignFloatLen_{0};
    uint32_t h32AlignRecvXLen_{0};
    uint32_t h512AlignRecvXLen_{0};
    uint32_t sendCostStatsBufSize_{0};
    uint32_t k32AlignFloatLen_{0};
    uint32_t k32AlignLen_{0};
    uint32_t addrUint64AlignLen_{0};

    TPipe *tpipe_{nullptr};
    TQue<QuePosition::VECIN, 1> weightedSumQueue_;
    TQue<QuePosition::VECOUT, 1> sendCostStatsOutQueue_;
    TQueBind<QuePosition::VECIN, QuePosition::VECOUT, 1> localCopyQueue_;
    TBuf<> addrBuf;
    TBuf<> statusBuf;
    TBuf<> waitStatusBuf;
    TBuf<> gatherMaskOutBuf;
    TBuf<> statusSumBuf;

    TBuf<> topkWeightsBuf_;
    TBuf<> sendTokenIdxBuf_;
    TBuf<> tokenFloatBuf_;
    TBuf<> sumFloatBuf_;
    TBuf<> weightedMulBuf_;
    TBuf<> xOutBuf_;
    TBuf<> allRecvCountBuf_;
    TBuf<> topkIdxBuf_;
    TBuf<> balanceMatrixBuf_;

    GlobalTensor<RecvXType> dstGT;
    GlobalTensor<RecvXType> recvXGT_;
    GlobalTensor<SrcInfoType> epRecvCountGT_;
    GlobalTensor<float> topkWeightsGT_;
    GlobalTensor<int32_t> sendTokenIdxGT_;
    GlobalTensor<int32_t> topkIdxGT_;
    GlobalTensor<XType> xOutGlobal_;
    GlobalTensor<int32_t> sendCostStatsGT_;
    GlobalTensor<int32_t> balanceMatrixGT_;

    GM_ADDR recvXGM_;
    GM_ADDR localRankGM_;
    GM_ADDR XOutGM_;
    GM_ADDR workspaceGM_;
    GM_ADDR metaStateGvaGM_;
    GM_ADDR dataStateGvaGM_;
    GM_ADDR topkWeightsGM_;
    GM_ADDR topkIdxGM_;
    GM_ADDR sendTokenIdxGM_;
    GM_ADDR epRecvCountGM_;

    GM_ADDR gva_gm;
    __gm__ CommGroupInfo *comm;
    __gm__ uint16_t *peerRanks;
    uint64_t localMemSize_ = 0;
    uint64_t metaSize_ = 0;
    uint64_t addrOffset_ = 0;
    uint64_t stateOffset_ = 0;
    uint64_t flagOffset_ = 0;
    // List of asymmetric output addresses (recvXGM_)
    uint64_t shareRecvXAddrs[MAX_RANK_SIZE];
    // List of asymmetric output addresses (topkIdxGM_)
    uint64_t shareTopkIdxAddrs[MAX_RANK_SIZE];
    // List of asymmetric output addresses (topkWeightsGM_)
    uint64_t shareTopkWeightsAddrs[MAX_RANK_SIZE];
    // List of asymmetric output addresses (sendTokenIdxGM_)
    uint64_t shareSendTokenIdxAddrs[MAX_RANK_SIZE];
    // List of asymmetric output addresses (epRecvCountGM_)
    uint64_t shareRecvCountAddrs[MAX_RANK_SIZE];
    // List of asymmetric output addresses (XOutGM_)
    uint64_t shareXOutAddrs[MAX_RANK_SIZE];
    uint32_t shareAddrNum{6};

    LocalTensor<float> tokenFloatLocal;
    LocalTensor<float> weightedMulBufLocal;
    LocalTensor<float> sumFloatBufLocal;
    LocalTensor<float> topkWeightsLocal;
    LocalTensor<int32_t> sendTokenIdxLocal;
    LocalTensor<uint32_t> stateTensorLocal;
    LocalTensor<int32_t> allRecvCountLocal;
    LocalTensor<int32_t> topkIdxLocal;
    LocalTensor<int32_t> balanceMatrixLocal;
};

template <TypeClass>
ZBAL_KERNEL void CombineNormal<TypeFunc>::Init(GM_ADDR metaAddr, GM_ADDR recvX, GM_ADDR epRecvCount,
    GM_ADDR topkWeights, GM_ADDR topkIdx, GM_ADDR sendTokenIdx, GM_ADDR balanceMatrix, uint32_t rank,
    uint32_t numExperts, uint32_t bs, uint32_t hidden, uint32_t topK, bool enableBalance, GM_ADDR XOut, TPipe *pipe)
{
    tpipe_ = pipe;
    blockIdx = GetBlockIdx();
    blockNum = GetBlockNum();
    batchSize = bs;
    h = hidden;
    this->topK = topK;
    epRankId = rank;
    moeExpertNum = numExperts;
    isEnableBalance_ = enableBalance; // enableBalance

    gva_gm = (GM_ADDR)metaAddr;
    comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaAddr);
    peerRanks = (__gm__ uint16_t *)comm->peerGroupRank2WorldRank;
    localMemSize_ = comm->localDeviceMemSize;
    GM_ADDR meta_addr_gm = reinterpret_cast<__gm__ uint8_t *>(comm->myAddressExchangeGva);
    addrOffset_ = (meta_addr_gm - gva_gm);
    metaSize_ = addrOffset_ + comm->sizeForExchangeAddress;
    stateOffset_ = metaSize_ - COMBINE_STATUS_OFFSET;
    flagOffset_ = metaSize_ - META_FLAG_R_OFFSET;
    epRankSize = comm->groupSize;
    assert(comm->sizeForExchangeAddress >= META_FLAG_R_OFFSET * 2,
        "The group meta size for exchange is %lluKB, the min value should be %lluKB. \
        epRankId:%d, epWorldSize:%d, moeExpertNum:%d, shareAddrNum:%d\n",
        comm->sizeForExchangeAddress / KB_SIZE, META_FLAG_R_OFFSET * 2 / KB_SIZE, epRankId, epRankSize,
        moeExpertNum, shareAddrNum);
    moeExpertNumPerRank = moeExpertNum / epRankSize;

    recvXGM_ = recvX;
    XOutGM_ = XOut;
    topkIdxGM_ = topkIdx;
    topkWeightsGM_ = topkWeights;
    sendTokenIdxGM_ = sendTokenIdx;
    epRecvCountGM_ = epRecvCount;

    recvXGT_.SetGlobalBuffer((__gm__ RecvXType *)recvX);
    epRecvCountGT_.SetGlobalBuffer((__gm__ int32_t *)epRecvCount);  // 放置allReccvCount信息，num_ranks * num_experts
    topkWeightsGT_.SetGlobalBuffer((__gm__ float *)topkWeights);
    topkIdxGT_.SetGlobalBuffer((__gm__ int32_t *)topkIdx);
    sendTokenIdxGT_.SetGlobalBuffer((__gm__ int32_t *)sendTokenIdx);
    xOutGlobal_.SetGlobalBuffer((__gm__ XType *)XOut);
    balanceMatrixGT_.SetGlobalBuffer((__gm__ int32_t *)balanceMatrix);

    uint32_t hFloatSize = h * static_cast<uint32_t>(sizeof(float));
    h32AlignFloatLen_ = Ceil(hFloatSize, UB_ALIGN) * UB_ALIGN;
    h256AlignFloatLen_ = Ceil(hFloatSize, MUL_256_ALIGN) * MUL_256_ALIGN;
    hRecvXTypeLen_ = h * sizeof(RecvXType);
    h32AlignRecvXLen_ = Ceil(hRecvXTypeLen_, UB_ALIGN) * UB_ALIGN;
    h512AlignRecvXLen_ = Ceil(hRecvXTypeLen_, WIN_512_ALIGN) * WIN_512_ALIGN;
    k32AlignFloatLen_ = Ceil(topK * static_cast<uint32_t>(sizeof(float)), UB_ALIGN) * UB_ALIGN;
    k32AlignLen_ = Ceil(topK * static_cast<uint32_t>(sizeof(int32_t)), UB_ALIGN) * UB_ALIGN;
    addrUint64AlignLen_ = Ceil(shareAddrNum * sizeof(uint64_t), UB_ALIGN) * UB_ALIGN;
    tpipe_->InitBuffer(addrBuf, addrUint64AlignLen_);
    // h32AlignFloatLen_:28672, h256AlignFloatLen_:28672, hRecvXTypeLen_:14336, h32AlignRecvXLen_:14336,
    // h512AlignRecvXLen_:14336 k32AlignFloatLen_:32, k32AlignLen_:32

    // rank分核
    SplitCoreCal(epRankSize, rankNumPerBlock, curBlockStartRankId, curBlockEndRankId);
}

template <TypeClass>
ZBAL_KERNEL void CombineNormal<TypeFunc>::ResetMetaState()
{
    if (rankNumPerBlock == 0U) {
        return;
    }
    uint32_t waitStatusBufSize = (((rankNumPerBlock * UB_ALIGN) > 256) ? (rankNumPerBlock * UB_ALIGN) : 256);
    tpipe_->InitBuffer(waitStatusBuf, waitStatusBufSize);  // ranks/48 * 32B = 1 * 32B

    GlobalTensor<float> statusFp32TensorGT;
    auto ptr = GetMetaAddrByRankId(epRankId, STATE);
    statusFp32TensorGT.SetGlobalBuffer((__gm__ float *)(ptr));

    DataCopyParams intriOutParams{static_cast<uint16_t>(rankNumPerBlock), 1, 0, 0};
    uint64_t duplicateMask[2] = {0x101010101010101, 0};
    LocalTensor<int32_t> cleanStateTensor = waitStatusBuf.Get<int32_t>();
    SyncFunc<AscendC::HardEvent::S_V>();
    Duplicate<int32_t>(cleanStateTensor, 0, duplicateMask, Ceil(rankNumPerBlock, 8), 1, 8);
    SyncFunc<AscendC::HardEvent::V_MTE3>();
    DataCopy(statusFp32TensorGT[curBlockStartRankId * STATE_OFFSET / sizeof(float)],
             cleanStateTensor.ReinterpretCast<float>(), intriOutParams);
    SyncFunc<AscendC::HardEvent::MTE3_S>();
}

template <TypeClass>
ZBAL_KERNEL void CombineNormal<TypeFunc>::PutShareAddr()
{
    // 一个核将地址写入本rank的meta
    if (blockIdx != 0) {
        return;
    }
    LocalTensor<uint64_t> addrTensor_ = addrBuf.Get<uint64_t>();
    uint64_t recvXAddr = reinterpret_cast<__gm__ uint64_t>(recvXGM_);
    addrTensor_(0) = recvXAddr;
    uint64_t topkIdxAddr = reinterpret_cast<__gm__ uint64_t>(topkIdxGM_);
    addrTensor_(1) = topkIdxAddr;
    uint64_t topkWeightsAddr = reinterpret_cast<__gm__ uint64_t>(topkWeightsGM_);
    addrTensor_(2) = topkWeightsAddr;
    uint64_t sendTokenIdxAddr = reinterpret_cast<__gm__ uint64_t>(sendTokenIdxGM_);
    addrTensor_(3) = sendTokenIdxAddr;
    uint64_t epRecvCountAddr = reinterpret_cast<__gm__ uint64_t>(epRecvCountGM_);
    addrTensor_(4) = epRecvCountAddr;
    uint64_t XOutAddr = reinterpret_cast<__gm__ uint64_t>(XOutGM_);
    addrTensor_(5) = XOutAddr;
    SyncFunc<AscendC::HardEvent::S_MTE3>();
    SyncFunc<AscendC::HardEvent::MTE2_MTE3>();

    AscendC::GlobalTensor<uint64_t> metaDataGt;
    DataCopyExtParams copyParams{1, static_cast<uint32_t>(shareAddrNum * sizeof(uint64_t)), 0, 0, 0};
    GM_ADDR remote_meta = GetMetaAddrByRankId(epRankId, ADDR);
    metaDataGt.SetGlobalBuffer((__gm__ uint64_t *)(remote_meta));
    DataCopyPad(metaDataGt, addrTensor_, copyParams);
}

template <TypeClass>
ZBAL_KERNEL void CombineNormal<TypeFunc>::GetShareAddr()
{
    LocalTensor<uint64_t> addrTensor_ = addrBuf.Get<uint64_t>();
    DataCopyExtParams copyParams = {1U, static_cast<uint32_t>(addrUint64AlignLen_), 0, 0, 0};
    DataCopyPadExtParams<uint64_t> copyExtParams{false, 0U, 0U, 0U};

    // 从远端获取共享地址
    for (uint32_t i = 0; i < epRankSize; i++) {
        GM_ADDR remote_meta = GetMetaAddrByRankId(i, ADDR);
        AscendC::GlobalTensor<uint64_t> shareAddrGt;
        shareAddrGt.SetGlobalBuffer((__gm__ uint64_t *)(remote_meta));

        SyncFunc<AscendC::HardEvent::MTE3_MTE2>();
        DataCopyPad(addrTensor_, shareAddrGt, copyParams, copyExtParams);
        SyncFunc<AscendC::HardEvent::MTE2_S>();
        shareRecvXAddrs[i] = addrTensor_(0);
        shareTopkIdxAddrs[i] = addrTensor_(1);
        shareTopkWeightsAddrs[i] = addrTensor_(2);
        shareSendTokenIdxAddrs[i] = addrTensor_(3);
        shareRecvCountAddrs[i] = addrTensor_(4);
        shareXOutAddrs[i] = addrTensor_(5);
    }
}

template <TypeClass>
ZBAL_KERNEL void CombineNormal<TypeFunc>::SetSyncFlag(int metaType)
{
    if (rankNumPerBlock == 0U) {
        SyncAll<true>();
        return;
    }
    uint32_t statusCntAlign = Ceil(rankNumPerBlock, 8) * 8;
    tpipe_->InitBuffer(statusBuf, statusCntAlign * UB_ALIGN);
    LocalTensor statusTensor = statusBuf.Get<int32_t>();
    Duplicate<int32_t>(statusTensor, 0, rankNumPerBlock * 8);
    uint64_t mask[2] = {0x101010101010101, 0};
    PipeBarrier<PIPE_V>();
    Duplicate<int32_t>(statusTensor, 0x3F800000, mask, statusCntAlign / 8, 1, 8);
    PipeBarrier<PIPE_ALL>();
    SyncAll<true>();

    AscendC::GlobalTensor<int32_t> gmRemoteStatusGt;
    for (uint32_t i = curBlockStartRankId; i < curBlockEndRankId; i++) {
        auto ptr = GetMetaAddrByRankId(i, metaType) + epRankId * STATE_OFFSET;
        // GM_ADDR remote_meta = (__gm__ uint8_t *)(ptr);
        gmRemoteStatusGt.SetGlobalBuffer((__gm__ int32_t *)(ptr));
        DataCopy<int32_t>(gmRemoteStatusGt, statusTensor[(i - curBlockStartRankId) * 8], 8UL);
    }
    SyncFunc<AscendC::HardEvent::MTE3_S>();
}

template <TypeClass>
ZBAL_KERNEL void CombineNormal<TypeFunc>::WaitSyncFlag(int metaType)
{
    if (rankNumPerBlock == 0U) {
        SyncAll<true>();
        return;
    }
    uint32_t waitStatusBufSize = (((rankNumPerBlock * UB_ALIGN) > 256) ? (rankNumPerBlock * UB_ALIGN) : 256);
    tpipe_->InitBuffer(waitStatusBuf, waitStatusBufSize);  // ranks/48 * 32B = 1 * 32B
    uint32_t maskAlign = Ceil(epRankSize * sizeof(float), UB_ALIGN) * UB_ALIGN;
    tpipe_->InitBuffer(gatherMaskOutBuf, maskAlign);  // rankSize * 4B
    tpipe_->InitBuffer(statusSumBuf, UB_ALIGN);       // 32B

    LocalTensor<float> gatherMaskOutTensor = gatherMaskOutBuf.Get<float>();
    LocalTensor<float> statusSumOutTensor = statusSumBuf.Get<float>(UB_ALIGN);
    LocalTensor<float> statusFp32Tensor = waitStatusBuf.Get<float>();
    GlobalTensor<float> statusFp32TensorGT;
    auto ptr = GetMetaAddrByRankId(epRankId, metaType);
    statusFp32TensorGT.SetGlobalBuffer((__gm__ float *)(ptr));
    uint32_t mask = 1;
    float compareTarget = static_cast<float>(1.0) * rankNumPerBlock;
    float sumOfFlag = static_cast<float>(-1.0);
    DataCopyParams intriParams{static_cast<uint16_t>(rankNumPerBlock), 1, 0, 0};

    SyncFunc<AscendC::HardEvent::S_V>();
    while (sumOfFlag != compareTarget) {
        DataCopy(statusFp32Tensor, statusFp32TensorGT[curBlockStartRankId * STATE_OFFSET / sizeof(float)], intriParams);
        SyncFunc<AscendC::HardEvent::MTE2_V>();
        ReduceSum(statusSumOutTensor, statusFp32Tensor, gatherMaskOutTensor, mask, rankNumPerBlock, 1);
        SyncFunc<AscendC::HardEvent::V_S>();
        sumOfFlag = statusSumOutTensor.GetValue(0);
    }

    // 清标记位
    SyncFunc<AscendC::HardEvent::MTE3_S>();
    DataCopyParams intriOutParams{static_cast<uint16_t>(rankNumPerBlock), 1, 0, 0};
    uint64_t duplicateMask[2] = {0x101010101010101, 0};
    LocalTensor<int32_t> cleanStateTensor = waitStatusBuf.Get<int32_t>();
    SyncFunc<AscendC::HardEvent::S_V>();
    Duplicate<int32_t>(cleanStateTensor, 0, duplicateMask, Ceil(rankNumPerBlock, 8), 1, 8);
    SyncFunc<AscendC::HardEvent::V_MTE3>();
    DataCopy(statusFp32TensorGT[curBlockStartRankId * STATE_OFFSET / sizeof(float)],
             cleanStateTensor.ReinterpretCast<float>(), intriOutParams);
    SyncFunc<AscendC::HardEvent::MTE3_S>();
    SyncAll<true>();
}

template <TypeClass>
ZBAL_KERNEL void CombineNormal<TypeFunc>::ReadTokenAndWeightedSum(uint32_t tokenIndex,
                                                                                           uint32_t tarRankId)
{
    const DataCopyExtParams xOutCopyParams{1U, static_cast<uint32_t>(hRecvXTypeLen_), 0U, 0U, 0U};
    const DataCopyPadExtParams<RecvXType> copyPadExtParams{false, 0U, 0U, 0U};
    Duplicate(sumFloatBufLocal, static_cast<float>(0), h);

    for (uint32_t topkId = 0U; topkId < topK; topkId++) {
        int32_t expertId = topkIdxLocal.GetValue(topkId);
        if (expertId < 0 || expertId >= moeExpertNum) {
            continue;
        }
        float scale = topkWeightsLocal.GetValue(topkId);
        int32_t remoteReadOffset = sendTokenIdxLocal(topkId);
        int32_t remoteReadBase = allRecvCountLocal(expertId * epRankSize + tarRankId);
        uint64_t remoteReadAddr = static_cast<uint64_t>(remoteReadBase + remoteReadOffset) * hRecvXTypeLen_;

        int32_t dstRankId = expertId / moeExpertNumPerRank;
        auto ptr = shareRecvXAddrs[dstRankId];
        dstGT.SetGlobalBuffer((__gm__ XType *)(ptr + hRecvXTypeLen_ * (remoteReadBase + remoteReadOffset)));

        LocalTensor<XType> tmpToken = weightedSumQueue_.AllocTensor<XType>();
        DataCopyPad(tmpToken, dstGT, xOutCopyParams, copyPadExtParams);
        weightedSumQueue_.EnQue(tmpToken);
        tmpToken = weightedSumQueue_.DeQue<XType>();
        Cast(tokenFloatLocal, tmpToken, AscendC::RoundMode::CAST_NONE, h);
        PipeBarrier<PIPE_V>();
        AscendC::Muls(weightedMulBufLocal, tokenFloatLocal, scale, h);
        PipeBarrier<PIPE_V>();
        AscendC::Add(sumFloatBufLocal, sumFloatBufLocal, weightedMulBufLocal, h);
        weightedSumQueue_.FreeTensor<XType>(tmpToken);
        PipeBarrier<PIPE_V>();
    }
    PipeBarrier<PIPE_V>();
    LocalTensor<XType> xOutLocal = xOutBuf_.Get<XType>();
    Cast(xOutLocal, sumFloatBufLocal, AscendC::RoundMode::CAST_RINT, h);
    SyncFunc<AscendC::HardEvent::V_MTE3>();
    DataCopyPad(xOutGlobal_[tokenIndex * h], xOutLocal, xOutCopyParams);
}

template <TypeClass>
ZBAL_KERNEL void CombineNormal<TypeFunc>::ReadTokenFromRemote()
{
    if (batchSize == 0U) {
        return;
    }
    uint32_t tokenPerBlock = 0U, startTokenIndex = 0U, endTokenIndex = 0U;
    SplitCoreCal(batchSize, tokenPerBlock, startTokenIndex, endTokenIndex);
    if (tokenPerBlock == 0U) {
        return;
    }
    tpipe_->Reset();
    tpipe_->InitBuffer(xOutBuf_, h32AlignRecvXLen_);                          // 14KB
    tpipe_->InitBuffer(tokenFloatBuf_, h32AlignFloatLen_);                    // 28KB
    tpipe_->InitBuffer(weightedMulBuf_, h256AlignFloatLen_);                  // 28KB
    tpipe_->InitBuffer(sumFloatBuf_, h32AlignFloatLen_);                      // 28KB
    tpipe_->InitBuffer(weightedSumQueue_, BUFFER_NUM, h32AlignRecvXLen_);  // 2 * 14KB = 28KB
    tpipe_->InitBuffer(topkWeightsBuf_, k32AlignFloatLen_);                   // 32b
    tpipe_->InitBuffer(sendTokenIdxBuf_, k32AlignLen_);                       // 32b
    tpipe_->InitBuffer(topkIdxBuf_, k32AlignLen_);                            // 32b
    // moeExpertNum最大为512，tensor大小为 64*512*4=128kb
    uint32_t recvCountAlignLen_ = Ceil(epRankSize * moeExpertNum * sizeof(int32_t), UB_ALIGN) * UB_ALIGN;
    tpipe_->InitBuffer(allRecvCountBuf_, recvCountAlignLen_);

    topkWeightsLocal = topkWeightsBuf_.Get<float>();
    tokenFloatLocal = tokenFloatBuf_.Get<float>();
    weightedMulBufLocal = weightedMulBuf_.Get<float>();
    sumFloatBufLocal = sumFloatBuf_.Get<float>();
    sendTokenIdxLocal = sendTokenIdxBuf_.Get<int32_t>();
    allRecvCountLocal = allRecvCountBuf_.Get<int32_t>();
    topkIdxLocal = topkIdxBuf_.Get<int32_t>();

    epRecvCountGT_.SetGlobalBuffer((__gm__ int32_t *)epRecvCountGM_);
    topkWeightsGT_.SetGlobalBuffer((__gm__ float *)topkWeightsGM_);
    topkIdxGT_.SetGlobalBuffer((__gm__ int32_t *)topkIdxGM_);
    sendTokenIdxGT_.SetGlobalBuffer((__gm__ int32_t *)sendTokenIdxGM_);
    xOutGlobal_.SetGlobalBuffer((__gm__ XType *)XOutGM_);

    const DataCopyExtParams bskParams{1U, static_cast<uint32_t>(topK * sizeof(float)), 0U, 0U, 0U};
    const DataCopyExtParams bskParams1{1U, static_cast<uint32_t>(topK * sizeof(int32_t)), 0U, 0U, 0U};
    const DataCopyPadExtParams<float> copyPadFloatParams{false, 0U, 0U, 0U};
    const DataCopyPadExtParams<int32_t> copyPadint32Params{false, 0U, 0U, 0U};
    const DataCopyExtParams countParams{1U, static_cast<uint32_t>(epRankSize * moeExpertNum * sizeof(int32_t)), 0U, 0U,
                                        0U};
    SyncFunc<AscendC::HardEvent::MTE3_MTE2>();
    DataCopyPad(allRecvCountLocal, epRecvCountGT_, countParams, copyPadint32Params);
    PipeBarrier<PIPE_V>();
    SyncFunc<AscendC::HardEvent::MTE2_S>();

    for (uint32_t tokenIndex = startTokenIndex; tokenIndex < endTokenIndex; tokenIndex++) {
        SyncFunc<AscendC::HardEvent::MTE3_MTE2>();
        DataCopyPad(topkWeightsLocal, topkWeightsGT_[tokenIndex * topK], bskParams, copyPadFloatParams);
        DataCopyPad(topkIdxLocal, topkIdxGT_[tokenIndex * topK], bskParams1, copyPadint32Params);
        DataCopyPad(sendTokenIdxLocal, sendTokenIdxGT_[tokenIndex * topK], bskParams1, copyPadint32Params);
        SyncFunc<AscendC::HardEvent::MTE2_S>();
        ReadTokenAndWeightedSum(tokenIndex, epRankId);
    }
}

template <TypeClass>
ZBAL_KERNEL void CombineNormal<TypeFunc>::ReadAndWriteForTargetRank(uint32_t startId, uint32_t endId,
                                                                     uint32_t tokenCnt, uint32_t tarRankId)
{
    if (tokenCnt == 0U) {
        return;
    }
    uint32_t tokenPerBlock = 0U, startTokenIndex = 0U, endTokenIndex = 0U;
    SplitCoreCal(tokenCnt, tokenPerBlock, startTokenIndex, endTokenIndex);
    if (tokenPerBlock == 0U) {
        return;
    }
    startTokenIndex += startId;
    endTokenIndex += startId;

    // 以下GT都是目标rank上的
    epRecvCountGT_.SetGlobalBuffer((__gm__ int32_t *)(shareRecvCountAddrs[tarRankId]));
    topkWeightsGT_.SetGlobalBuffer((__gm__ float *)(shareTopkWeightsAddrs[tarRankId]));
    topkIdxGT_.SetGlobalBuffer((__gm__ int32_t *)(shareTopkIdxAddrs[tarRankId]));
    sendTokenIdxGT_.SetGlobalBuffer((__gm__ int32_t *)(shareSendTokenIdxAddrs[tarRankId]));
    xOutGlobal_.SetGlobalBuffer((__gm__ XType *)shareXOutAddrs[tarRankId]);

    const DataCopyExtParams bskParams{1U, static_cast<uint32_t>(topK * sizeof(float)), 0U, 0U, 0U};
    const DataCopyExtParams bskParams1{1U, static_cast<uint32_t>(topK * sizeof(int32_t)), 0U, 0U, 0U};
    const DataCopyPadExtParams<float> copyPadFloatParams{false, 0U, 0U, 0U};
    const DataCopyPadExtParams<int32_t> copyPadint32Params{false, 0U, 0U, 0U};
    const DataCopyExtParams countParams{1U, static_cast<uint32_t>(epRankSize * moeExpertNum * sizeof(int32_t)), 0U, 0U,
                                        0U};

    SyncFunc<AscendC::HardEvent::MTE3_MTE2>();
    DataCopyPad(allRecvCountLocal, epRecvCountGT_, countParams, copyPadint32Params);
    PipeBarrier<PIPE_V>();
    SyncFunc<AscendC::HardEvent::MTE2_S>();

    for (uint32_t tokenIndex = startTokenIndex; tokenIndex < endTokenIndex; tokenIndex++) {
        SyncFunc<AscendC::HardEvent::MTE3_MTE2>();
        DataCopyPad(topkWeightsLocal, topkWeightsGT_[tokenIndex * topK], bskParams, copyPadFloatParams);
        DataCopyPad(topkIdxLocal, topkIdxGT_[tokenIndex * topK], bskParams1, copyPadint32Params);
        DataCopyPad(sendTokenIdxLocal, sendTokenIdxGT_[tokenIndex * topK], bskParams1, copyPadint32Params);
        SyncFunc<AscendC::HardEvent::MTE2_S>();
        ReadTokenAndWeightedSum(tokenIndex, tarRankId);
    }
}

template <TypeClass>
ZBAL_KERNEL void CombineNormal<TypeFunc>::HandleAllRankToken()
{
    tpipe_->Reset();
    uint32_t matrixAlignLen = Ceil(epRankSize * 2 * sizeof(int32_t), UB_ALIGN) * UB_ALIGN;
    tpipe_->InitBuffer(balanceMatrixBuf_, matrixAlignLen);
    tpipe_->InitBuffer(xOutBuf_, h32AlignRecvXLen_);                          // 14KB
    tpipe_->InitBuffer(tokenFloatBuf_, h32AlignFloatLen_);                    // 28KB
    tpipe_->InitBuffer(weightedMulBuf_, h256AlignFloatLen_);                  // 28KB
    tpipe_->InitBuffer(sumFloatBuf_, h32AlignFloatLen_);                      // 28KB
    tpipe_->InitBuffer(weightedSumQueue_, BUFFER_NUM, h32AlignRecvXLen_);  // 2 * 14KB = 28KB
    tpipe_->InitBuffer(topkWeightsBuf_, k32AlignFloatLen_);                   // 32b
    tpipe_->InitBuffer(sendTokenIdxBuf_, k32AlignLen_);                       // 32b
    tpipe_->InitBuffer(topkIdxBuf_, k32AlignLen_);                            // 32b
    // moeExpertNum最大为512，tensor大小为 64*512*4=128kb
    uint32_t recvCountAlignLen_ = Ceil(epRankSize * moeExpertNum * sizeof(int32_t), UB_ALIGN) * UB_ALIGN;
    tpipe_->InitBuffer(allRecvCountBuf_, recvCountAlignLen_);

    balanceMatrixLocal = balanceMatrixBuf_.Get<int32_t>();
    topkWeightsLocal = topkWeightsBuf_.Get<float>();
    tokenFloatLocal = tokenFloatBuf_.Get<float>();
    weightedMulBufLocal = weightedMulBuf_.Get<float>();
    sumFloatBufLocal = sumFloatBuf_.Get<float>();
    sendTokenIdxLocal = sendTokenIdxBuf_.Get<int32_t>();
    allRecvCountLocal = allRecvCountBuf_.Get<int32_t>();
    topkIdxLocal = topkIdxBuf_.Get<int32_t>();

    const DataCopyPadExtParams<int32_t> copyPadParams{false, 0U, 0U, 0U};
    const DataCopyExtParams matrixParams{1U, static_cast<uint32_t>(epRankSize * 2 * sizeof(int32_t)), 0U, 0U, 0U};
    int32_t offset = epRankSize * 2;
    // 每卡获取自己需要处理的token序列
    DataCopyPad(balanceMatrixLocal, balanceMatrixGT_[epRankId * offset], matrixParams, copyPadParams);
    SyncFunc<AscendC::HardEvent::MTE2_S>();
    for (int32_t i = 0; i < epRankSize; ++i) {
        int32_t tarRankId = (epRankId + i) % epRankSize;

        int32_t startId = balanceMatrixLocal(tarRankId * 2);
        int32_t endId = balanceMatrixLocal(tarRankId * 2 + 1);
        int32_t tokenCnt = (endId - startId + 1);
        if (startId == -1 || tokenCnt <= 0) {
            continue;
        }
        ReadAndWriteForTargetRank(startId, endId, tokenCnt, tarRankId);
    }
}

template <TypeClass>
ZBAL_KERNEL void CombineNormal<TypeFunc>::Process()
{
    if ASCEND_IS_AIV {  // 全aiv处理
        ResetMetaState();
        PutShareAddr();
        SetSyncFlag(FLAG);  // 全卡同步，确保对称地址都放到了meta空间
        WaitSyncFlag(FLAG);

        GetShareAddr();
        if (!isEnableBalance_) {
            ReadTokenFromRemote();
        } else {
            HandleAllRankToken();
        }
        SetSyncFlag(STATE);  // 全卡同步，确保数据已经获取完
        WaitSyncFlag(STATE);
    }
}

}  // namespace MoeCombineNormal
#endif