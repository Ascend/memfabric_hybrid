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
#ifndef ZBAL_KERNEL_COMBINE_LOWLATENCY_H
#define ZBAL_KERNEL_COMBINE_LOWLATENCY_H

#include "kernel_operator.h"
#include "zbal_def.h"
#include "zbal_kernel_utils.h"
#include "zbal_kernel_comm_args.h"

using namespace AscendC;
using namespace Moe;

constexpr int ZBAL_META_OFF = 334;

namespace MoeCombineLowLatency {
constexpr uint8_t BUFFER_NUM = 2; // 多buf
constexpr uint64_t COMBINE_STATUS_OFFSET = 20UL * 1024UL;
constexpr uint32_t STATE_OFFSET = 32U; // 状态空间偏移地址
constexpr uint8_t EP_DOMAIN = 0;
constexpr uint8_t TP_DOMAIN = 1;
constexpr uint32_t ADDR_UINT64_ALIGN = 8;
constexpr uint32_t ALIGNED_LEN = 256U; // blockReduceMax中，最多支持连续256字节数据参与计算
constexpr float SCALE_PARAM = 127.0;   // 计算量化参数所需的缩放倍数
constexpr uint64_t WIN_ADDR_ALIGN = 512UL;
constexpr uint64_t RANK_META_INFO_OFFSET = 1024UL * 1024UL;
constexpr uint32_t UB_ALIGN = 32U;

template<AscendC::HardEvent event>
__aicore__ inline void SyncFunc()
{
    int32_t eventID = static_cast<int32_t>(GetTPipePtr()->FetchEventID(event));
    AscendC::SetFlag<event>(eventID);
    AscendC::WaitFlag<event>(eventID);
}

#define TemplateTypeClass \
    typename ExpandXType, typename XType, typename ExpandIdxType, bool IsNeedReduceScatter, bool IsInt8Quant
#define TemplateTypeFunc ExpandXType, XType, ExpandIdxType, IsNeedReduceScatter, IsInt8Quant

template<TemplateTypeClass>
class CombineLowLatency {
public:
    __aicore__ inline CombineLowLatency(){};
    __aicore__ inline void Init(GM_ADDR metaAddr, GM_ADDR expandX, GM_ADDR expertIds, GM_ADDR expandIdx,
                                GM_ADDR epSendCount, GM_ADDR expertScales, GM_ADDR XOut, uint32_t rank,
                                uint32_t numExperts, uint32_t bs, uint32_t hidden, uint32_t topK, TPipe *pipe);
    __aicore__ inline void Process();

private:
    __aicore__ inline void PutShareAddr();
    __aicore__ inline void GetShareAddr();
    __aicore__ inline void SetSyncFlag(int metaType);
    __aicore__ inline void WaitSyncFlag(int metaType);
    __aicore__ inline void InputToDstOutput();
    __aicore__ inline void SplitToCore(uint32_t curSendCnt, uint32_t curUseAivNum, uint32_t &startTokenId,
                                       uint32_t &endTokenId, uint32_t &sendTokenNum);
    __aicore__ inline void InitInputAndOutput(GM_ADDR expandX, GM_ADDR expertIds, GM_ADDR expandIdx,
                                              GM_ADDR epSendCount, GM_ADDR expertScales, GM_ADDR XOut);
    __aicore__ inline void InitInt8Quant();

    // get metaInfo address
    __aicore__ inline GM_ADDR GetMetaAddrByRankId(const int32_t rankId, const int metaType)
    {
        auto ptr = zbal_ptr((__gm__ uint64_t *)(gva_gm), epRankId_, rankId, localMemSize_, peerRanks);

        switch (metaType) {
            case STATE: // 存放通信结束的state, 12KB
                return (GM_ADDR)(ptr) + addrOffset_ + ZBAL_META_OFF * KB_SIZE;
            case ADDR: // 存放交换的共享地址
                // 128K + 334K + 20K + 5K
                return (GM_ADDR)(ptr) + addrOffset_ + ZBAL_META_OFF * KB_SIZE + COMBINE_STATUS_OFFSET;
            case FLAG: // 存放第一次清理state空间后的同步flag, 12KB
                return (GM_ADDR)(ptr) + flagOffset_;
            default:
                return (GM_ADDR)(ptr);
        }
    }

    __aicore__ inline uint32_t MIN(uint32_t x, uint32_t y)
    {
        return (x < y) ? x : y;
    }

    TPipe *tpipe_{nullptr};
    GlobalTensor<ExpandXType> expandXGT_;
    GlobalTensor<bool> xActiveMaskGM_;
    GlobalTensor<int32_t> expertIdsGM_;
    GlobalTensor<int32_t> expandIdxGM_;
    GlobalTensor<ExpandIdxType> epSendCountGM_;
    GlobalTensor<ExpandIdxType> tpSendCountGM_;
    GlobalTensor<ExpandIdxType> elasticInfoGM_;
    GlobalTensor<float> expertScalesGT_;
    GlobalTensor<XType> srcWinGMTensor;
    GlobalTensor<XType> XOutGT_;
    GlobalTensor<XType> rankWindow_; // 用于存对端window的变量
    GlobalTensor<XType> tpRankWindow_;
    GlobalTensor<XType> rowTmpGlobal_;
    GlobalTensor<ExpandXType> oriXGM_;
    GlobalTensor<ExpandXType> constExpertAlpha1GM_;
    GlobalTensor<ExpandXType> constExpertAlpha2GM_;
    GlobalTensor<ExpandXType> constExpertVGM_;
    GlobalTensor<uint32_t> selfDataStatusGMTensor_;
    GlobalTensor<uint64_t> remoteMetaAddrGt;
    GlobalTensor<uint64_t> localMetaAddrGt;
    GlobalTensor<uint64_t> localAllAddrGt;

    GM_ADDR stateGM_;
    GM_ADDR statusDataSpaceGm_;
    GM_ADDR shareAddrSpaceGm_;
    // shared addrs
    GM_ADDR gva_gm;
    GM_ADDR metaInfo_gva_gm;
    __gm__ CommGroupInfo *comm;
    __gm__ uint16_t *peerRanks;
    uint64_t localMemSize_{0};
    uint64_t addrOffset_{0};
    uint64_t metaSize_{0};
    uint64_t flagOffset_{0};
    uint32_t shareAddrNum{1};
    uint64_t shareExpandXAddrs[MAX_RANK_SIZE];
    GM_ADDR expandXGM_;

    LocalTensor<XType> winTpSendCountTensor_;
    LocalTensor<ExpandXType> gmTpSendCountTensor_;
    LocalTensor<XType> outTensor_;
    LocalTensor<float> tokenReuceTensor_;
    LocalTensor<float> weightedSumTensor_;
    LocalTensor<float> sumTokenTensor_;
    LocalTensor<float> winTpSendCountFloatTensor_;
    LocalTensor<float> gmTpSendCountFloatTensor_;
    LocalTensor<int32_t> elasticInfoTensor_;
    LocalTensor<bool> maskStrideTensor_;
    LocalTensor<bool> maskGenerateTensor_;
    LocalTensor<uint32_t> dataStateLocalTensor_;
    LocalTensor<float> stateResetTensor_;
    LocalTensor<int32_t> globalSendCountLocal_;
    LocalTensor<uint64_t> expandXShareAddrLt_;

    // 框架侧确保数据上限， 相乘不会越界，因此统一采用uin32_t进行处理
    uint32_t axisBS_{0};
    uint32_t axisH_{0};
    uint32_t axisK_{0};
    uint32_t aivNum_{0};
    uint32_t coreNum_{0};
    uint32_t epWorldSize_{0};
    uint32_t epWorldSizeOriginal_{0};
    uint32_t tpWorldSize_{0};
    uint32_t epRankId_{0};
    uint32_t epRankIdOriginal_{0};
    uint32_t tpRankId_{0};
    uint32_t aivId_{0}; // aiv id
    uint32_t sharedExpertNum_{0};
    uint32_t sharedExpertRankNum_{0}; // 共享专家卡数
    uint32_t moeExpertRankNum_{0};    // moe专家卡数，等于epWorldSize_ - sharedExpertRankNum_
    uint32_t moeExpertPerRankNum_{0}; // 每张卡部署的moe专家数
    uint32_t moeSendNum_{0};          // moeExpertPerRankNum_ * epWorldSize_
    uint32_t zeroExpertNum_{0};
    uint32_t copyExpertNum_{0};
    uint32_t constExpertNum_{0};
    uint32_t moeExpertNum_{0};
    uint32_t globalBS_{0};
    uint32_t tpStateOffsetOnWin_{0};
    uint32_t bsKNum_{0};
    uint32_t sendCntNum_{0};
    uint32_t dataState_{0};
    uint32_t stateOffset_{0};
    uint64_t activeMaskBsCnt_{0};
    uint64_t winDataSizeOffset_{0};
    uint64_t winStatusOffset_{0};
    uint64_t totalWinSize_{0};
    uint32_t selfSendCnt_{0};
    uint32_t tpRemoteSendCnt_{0};
    uint32_t activeMaskAlignSize_{0};
    uint32_t hExpandXTypeSize_{0};
    uint32_t hAlign32Size_{0};
    uint32_t hFloatAlign32Size_{0};
    uint32_t hFloatAlign256Size_{0};
    uint32_t hExpandXAlign32Size_{0};
    uint32_t hAlignWinSize_{0};
    uint32_t hAlignWinCnt_{0};
    uint32_t tokenScaleCnt_{0};
    uint32_t scaleNumAlignSize_{0};
    uint32_t flagRcvCount_{0};
    uint32_t axisBsAlignSize_{0};

    TQueBind<QuePosition::VECIN, QuePosition::VECOUT, 1> moeQueue_;
    TQue<QuePosition::VECIN, 1> moeSumQueue_;
    TQueBind<QuePosition::VECIN, QuePosition::VECOUT, 1> gmTpSendCountQueue_;
    TQue<QuePosition::VECIN, 1> gmTpSendCountInQueue_;
    TQue<QuePosition::VECIN, 1> winTpSendCountInQueue_;
    TQue<QuePosition::VECOUT, 1> xOutQueue_;

    TBuf<> statusBuf;
    TBuf<> waitStatusBuf;
    TBuf<> gatherMaskOutBuf;
    TBuf<> statusSumBuf;
    TBuf<> recvAddrBuf_;

    TBuf<> sendCountBuf_;
    TBuf<> readStateBuf_;
    TBuf<> expertScalesBuf_;
    TBuf<> expertIdsBuf_;
    TBuf<> rowTmpFloatBuf_;
    TBuf<> sumFloatBuf_;
    TBuf<> tokenSumBuf_;
    TBuf<> weightedSumBuf_;
    TBuf<> xOutBuf_;
    TBuf<> addrBuf_;
    TBuf<> mulBuf_;
    TBuf<> indexCountsBuf_;
    TBuf<> winTpSendCountFloatBuf_;
    TBuf<> gmTpSendCountFloatBuf_;
    TBuf<> tokenBuf_;
    TBuf<> xActMaskTBuf_;
    TBuf<> xActMaskCastTBuf_;
    TBuf<> tokenTargetTBuf_;
    TBuf<> validBsIndexTBuf_;
    TBuf<> xActMaskSumTBuf_;
    TBuf<> stateBuf_;
    TBuf<> stateResetBuf_;
    TBuf<> expertMaskBuf_;
    TBuf<> elasticInfoBuf_;
    bool isInputTokenMaskFlag_ = false;
    bool isInputExpertMaskFlag_ = false;
    bool hasSharedExpertX_ = false;
    bool hasElasticInfoFlag_ = false;
    bool isScalingDownFlag_ = false;
    bool isShareExpertRankFlag_ = false;
    bool enableSpecialExpert_ = false;

    // int8量化
    TBuf<> xAbsBuf_;
    TBuf<> xMaxBuf_;
    TBuf<> xScaleMulBuf_;

    LocalTensor<int8_t> castLocalTensor_;
    LocalTensor<half> fp16CastTensor_;
    LocalTensor<float> absFloatTensor_;
    LocalTensor<float> reduceMaxFloatTensor_;
    LocalTensor<XType> scaleDivTensor_;
    LocalTensor<float> scaleDivFloatTensor_;
    LocalTensor<float> scaleDupLocalTensor_;
    LocalTensor<XType> sendLocalTensor_;
    LocalTensor<half> tokenTargetTensor_;
    LocalTensor<int32_t> validBsIndexTensor_;
    LocalTensor<bool> expertMaskTensor_;
    LocalTensor<float> expertScalesLocal_;
    LocalTensor<float> rowTmpFloatLocal_;
    LocalTensor<float> mulBufLocal_;
    LocalTensor<float> sumFloatBufLocal_;

    uint32_t mask_{0};
    uint32_t repeatNum_{0};
    uint32_t scaleNum_{0};
    float scaleValFloat_;
    uint32_t addrUint64AlignLen_{0};
    uint32_t addrAllAlignLen_{0};
    uint32_t rankNumPerBlock{0};
    uint32_t curBlockStartRankId{0};
    uint32_t curBlockEndRankId{0};
};

template<TemplateTypeClass>
__aicore__ inline void CombineLowLatency<TemplateTypeFunc>::SetSyncFlag(int metaType)
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
        auto ptr = GetMetaAddrByRankId(i, metaType) + epRankId_ * STATE_OFFSET;
        gmRemoteStatusGt.SetGlobalBuffer((__gm__ int32_t *)(ptr));
        DataCopy<int32_t>(gmRemoteStatusGt, statusTensor[(i - curBlockStartRankId) * 8], 8UL);
    }
    SyncFunc<AscendC::HardEvent::MTE3_S>();
}

template<TemplateTypeClass>
__aicore__ inline void CombineLowLatency<TemplateTypeFunc>::WaitSyncFlag(int metaType)
{
    if (rankNumPerBlock == 0U) {
        SyncAll<true>();
        return;
    }
    uint32_t waitStatusBufSize = (((rankNumPerBlock * UB_ALIGN) > 256) ? (rankNumPerBlock * UB_ALIGN) : 256);
    tpipe_->InitBuffer(waitStatusBuf, waitStatusBufSize); // ranks/48 * 32B = 1 * 32B
    uint32_t maskAlign = Ceil(epWorldSize_ * sizeof(float), UB_ALIGN) * UB_ALIGN;
    tpipe_->InitBuffer(gatherMaskOutBuf, maskAlign); // rankSize * 4B
    tpipe_->InitBuffer(statusSumBuf, UB_ALIGN);      // 32B

    LocalTensor<float> gatherMaskOutTensor = gatherMaskOutBuf.Get<float>();
    LocalTensor<float> statusSumOutTensor = statusSumBuf.Get<float>(UB_ALIGN);
    LocalTensor<float> statusFp32Tensor = waitStatusBuf.Get<float>();
    GlobalTensor<float> statusFp32TensorGT;
    auto ptr = GetMetaAddrByRankId(epRankId_, metaType);
    statusFp32TensorGT.SetGlobalBuffer((__gm__ float *)(ptr));
    uint32_t mask = 1;
    float compareTarget = static_cast<float>(1.0) * rankNumPerBlock;
    float sumOfFlag = static_cast<float>(-1.0);
    // use approximated targetSumFlag
    float minTarget = compareTarget - (float)0.5;
    float maxTarget = compareTarget + (float)0.5;

    DataCopyParams intriParams{static_cast<uint16_t>(rankNumPerBlock), 1, 0, 0};

    SyncFunc<AscendC::HardEvent::S_V>();
    while ((sumOfFlag < minTarget) || (sumOfFlag > maxTarget)) {
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

template<TemplateTypeClass>
__aicore__ inline void CombineLowLatency<TemplateTypeFunc>::PutShareAddr()
{
    if (aivId_ != 0) {
        return;
    }
    addrUint64AlignLen_ = Ceil(sizeof(uint64_t), UB_ALIGN) * UB_ALIGN;
    tpipe_->InitBuffer(addrBuf_, addrUint64AlignLen_);

    // store shareAddr separately
    LocalTensor<uint64_t> addrTensor_ = addrBuf_.Get<uint64_t>();
    uint64_t expandXAddr = reinterpret_cast<__gm__ uint64_t>(expandXGM_);
    addrTensor_(0) = expandXAddr;
    SyncFunc<AscendC::HardEvent::S_MTE3>();
    SyncFunc<AscendC::HardEvent::MTE2_MTE3>();

    // 写入到本rank的地址交换区
    AscendC::GlobalTensor<uint64_t> metaAddrGt;
    shareAddrSpaceGm_ = GetMetaAddrByRankId(epRankId_, ADDR);
    DataCopyExtParams copyParams{1, static_cast<uint32_t>(sizeof(uint64_t)), 0, 0, 0};
    metaAddrGt.SetGlobalBuffer((__gm__ uint64_t *)(shareAddrSpaceGm_ + epRankId_ * ADDR_UINT64_ALIGN));
    DataCopyPad(metaAddrGt, addrTensor_, copyParams);
}

template<TemplateTypeClass>
__aicore__ inline void CombineLowLatency<TemplateTypeFunc>::GetShareAddr()
{
    if (rankNumPerBlock == 0U) {
        SyncAll<true>();
        return;
    }
    shareAddrSpaceGm_ = GetMetaAddrByRankId(epRankId_, ADDR);

    for (uint32_t targetRankId = curBlockStartRankId; targetRankId < curBlockEndRankId; targetRankId++) {
        // 获取其他rank的addr，有1个shareAddr
        auto addrPtr = GetMetaAddrByRankId(targetRankId, ADDR);
        remoteMetaAddrGt.SetGlobalBuffer((__gm__ uint64_t *)(addrPtr + targetRankId * ADDR_UINT64_ALIGN));
        localMetaAddrGt.SetGlobalBuffer((__gm__ uint64_t *)(shareAddrSpaceGm_ + targetRankId * ADDR_UINT64_ALIGN));
        CpGM2GM(localMetaAddrGt, remoteMetaAddrGt, 1);
    }

    SyncAll<true>();
}

template<TemplateTypeClass>
__aicore__ inline void CombineLowLatency<TemplateTypeFunc>::InitInputAndOutput(GM_ADDR expandX, GM_ADDR expertIds,
                                                                               GM_ADDR expandIdx, GM_ADDR epSendCount,
                                                                               GM_ADDR expertScales, GM_ADDR XOut)
{
    expandXGM_ = expandX;
    expandXGT_.SetGlobalBuffer((__gm__ ExpandXType *)expandX);
    expertIdsGM_.SetGlobalBuffer((__gm__ ExpandIdxType *)expertIds);
    expandIdxGM_.SetGlobalBuffer((__gm__ int32_t *)expandIdx);
    epSendCountGM_.SetGlobalBuffer((__gm__ int32_t *)epSendCount);
    expertScalesGT_.SetGlobalBuffer((__gm__ float *)expertScales);
    XOutGT_.SetGlobalBuffer((__gm__ XType *)XOut);
}

template<TemplateTypeClass>
__aicore__ inline void CombineLowLatency<TemplateTypeFunc>::InitInt8Quant()
{
    scaleValFloat_ = static_cast<float>(1.0f / SCALE_PARAM);
    uint32_t scaleGranu = static_cast<uint32_t>(UB_ALIGN / sizeof(float)); // 计算每个block得到的reducemax结果数量
    scaleNum_ = (hExpandXAlign32Size_ / sizeof(ExpandXType)) / scaleGranu; // 得到有效scale的个数
    repeatNum_ = static_cast<uint32_t>(hFloatAlign256Size_ /
                                       ALIGNED_LEN); // BlockReduceMax 与 Brcb的重复迭代次数，每次256b参与计算
    mask_ = static_cast<uint32_t>(ALIGNED_LEN / sizeof(float));
    tokenScaleCnt_ = hAlign32Size_ / sizeof(ExpandXType) + scaleNum_; // int8_align + scale有效个数
}

template<TemplateTypeClass>
__aicore__ inline void
CombineLowLatency<TemplateTypeFunc>::Init(GM_ADDR metaAddr, GM_ADDR expandX, GM_ADDR expertIds, GM_ADDR expandIdx,
                                          GM_ADDR epSendCount, GM_ADDR expertScales, GM_ADDR XOut, uint32_t rank,
                                          uint32_t numExperts, uint32_t bs, uint32_t hidden, uint32_t topK, TPipe *pipe)
{
    tpipe_ = pipe;
    aivId_ = GetBlockIdx();
    coreNum_ = GetBlockNum();

    // init attributes
    axisBS_ = bs;
    axisH_ = hidden;
    axisK_ = topK;
    aivNum_ = coreNum_;
    globalBS_ = axisBS_ * epWorldSize_;
    epWorldSizeOriginal_ = comm->groupSize;
    epWorldSize_ = comm->groupSize;
    epRankId_ = rank;
    epRankIdOriginal_ = rank;

    // / ******************* / ********************/

    gva_gm = (GM_ADDR)metaAddr;
    comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaAddr);
    peerRanks = (__gm__ uint16_t *)comm->peerGroupRank2WorldRank;
    localMemSize_ = comm->localDeviceMemSize;
    // Change addrOffset Calculation
    GM_ADDR meta_addr_gm = reinterpret_cast<__gm__ uint8_t *>(comm->myAddressExchangeGva);
    addrOffset_ = (meta_addr_gm - gva_gm);

    metaSize_ = addrOffset_ + comm->sizeForExchangeAddress;
    flagOffset_ = Ceil(metaSize_ - 5 * KB_SIZE, UB_ALIGN) * UB_ALIGN;
    metaInfo_gva_gm = (GM_ADDR)(metaAddr) + addrOffset_;

    InitInputAndOutput(expandX, expertIds, expandIdx, epSendCount, expertScales, XOut);
    uint32_t sharedExpertRankNum = 0;

    // Disable ShareExp at the moment
    sharedExpertRankNum_ = 0;
    sharedExpertNum_ = 0;
    moeExpertNum_ = numExperts;
    moeExpertRankNum_ = epWorldSize_ - sharedExpertRankNum_;
    moeExpertPerRankNum_ = moeExpertNum_ / moeExpertRankNum_;
    bsKNum_ = axisBS_ * axisK_;
    moeSendNum_ = epWorldSize_ * moeExpertPerRankNum_;
    if (epRankId_ < sharedExpertRankNum) {
        isShareExpertRankFlag_ = true;
    }

    uint32_t hFloatSize = axisH_ * static_cast<uint32_t>(sizeof(float));
    hAlign32Size_ = Ceil(axisH_, UB_ALIGN) * UB_ALIGN;
    hFloatAlign32Size_ = Ceil(hFloatSize, UB_ALIGN) * UB_ALIGN;
    hFloatAlign256Size_ = Ceil(hFloatSize, ALIGNED_LEN) * ALIGNED_LEN;
    hExpandXTypeSize_ = axisH_ * sizeof(ExpandXType);
    hExpandXAlign32Size_ = Ceil(hExpandXTypeSize_, UB_ALIGN) * UB_ALIGN;
    hAlignWinSize_ = Ceil(hExpandXTypeSize_, WIN_ADDR_ALIGN) * WIN_ADDR_ALIGN;
    hAlignWinCnt_ = hAlignWinSize_ / sizeof(ExpandXType);

    if constexpr (IsInt8Quant) {
        InitInt8Quant();
    }
    PipeBarrier<PIPE_ALL>();

    if (isShareExpertRankFlag_) {
        DataCacheCleanAndInvalid<ExpandIdxType, CacheLine::SINGLE_CACHE_LINE, DcciDst::CACHELINE_OUT>(
            epSendCountGM_[epWorldSize_ - 1]);
        selfSendCnt_ = epSendCountGM_(epWorldSize_ - 1);
    } else {
        DataCacheCleanAndInvalid<ExpandIdxType, CacheLine::SINGLE_CACHE_LINE, DcciDst::CACHELINE_OUT>(
            epSendCountGM_[moeSendNum_ - 1]);
        selfSendCnt_ = epSendCountGM_(moeSendNum_ - 1);
    }
    tpipe_->InitBuffer(moeQueue_, BUFFER_NUM, hExpandXAlign32Size_);
    addrUint64AlignLen_ = Ceil(sizeof(uint64_t), UB_ALIGN) * UB_ALIGN;
    tpipe_->InitBuffer(addrBuf_, addrUint64AlignLen_);
    flagRcvCount_ = axisK_ + sharedExpertNum_;

    SplitToCore(epWorldSize_, coreNum_, curBlockStartRankId, curBlockEndRankId, rankNumPerBlock);
    shareAddrSpaceGm_ = GetMetaAddrByRankId(epRankId_, ADDR);
}

template<TemplateTypeClass>
__aicore__ inline void CombineLowLatency<TemplateTypeFunc>::SplitToCore(uint32_t curSendCnt, uint32_t curUseAivNum,
                                                                        uint32_t &startTokenId, uint32_t &endTokenId,
                                                                        uint32_t &sendTokenNum)
{
    sendTokenNum = curSendCnt / curUseAivNum;               // 每个aiv需要发送的token数(也可以是其他数)
    uint32_t remainderTokenNum = curSendCnt % curUseAivNum; // 余数
    startTokenId = sendTokenNum * aivId_;                   // 每个aiv发送时的起始rankid
    if (aivId_ < remainderTokenNum) {                       // 前remainderRankNum个aiv需要多发1个卡的数据
        sendTokenNum += 1;
        startTokenId += aivId_;
    } else {
        startTokenId += remainderTokenNum;
    }
    endTokenId = startTokenId + sendTokenNum;
}

template<TemplateTypeClass>
__aicore__ inline void CombineLowLatency<TemplateTypeFunc>::InputToDstOutput()
{
    if (axisBS_ == 0U) {
        return;
    }

    uint32_t startTokenId, endTokenId, sendTokenNum; // 每个aiv发送时的起始rankid
    SplitToCore(axisBS_, aivNum_, startTokenId, endTokenId, sendTokenNum);
    if (sendTokenNum == 0U) {
        return;
    }

    tpipe_->Reset();
    uint32_t sendCountAlignLen_ = Ceil(epWorldSize_ * moeExpertNum_ * sizeof(int32_t), UB_ALIGN) * UB_ALIGN;
    tpipe_->InitBuffer(sendCountBuf_, sendCountAlignLen_);
    globalSendCountLocal_ = sendCountBuf_.Get<int32_t>();

    uint32_t scaleAlignLen_ = Ceil(sendTokenNum * axisK_ * sizeof(int32_t), UB_ALIGN) * UB_ALIGN;
    tpipe_->InitBuffer(expertScalesBuf_, scaleAlignLen_);
    tpipe_->InitBuffer(indexCountsBuf_, scaleAlignLen_);

    // get scale per token
    DataCopyExtParams bskParams{1U, static_cast<uint32_t>(sendTokenNum * axisK_ * sizeof(uint32_t)), 0U, 0U, 0U};
    const DataCopyPadExtParams<float> copyPadFloatParams{false, 0U, 0U, 0U};
    expertScalesLocal_ = expertScalesBuf_.Get<float>();
    DataCopyPad(expertScalesLocal_, expertScalesGT_[startTokenId * axisK_], bskParams, copyPadFloatParams);

    // get per token remote read small offset
    LocalTensor<int32_t> expandIdxLocal_ = indexCountsBuf_.Get<int32_t>();
    DataCopyPadExtParams<int32_t> copyPadint32Params{false, 0U, 0U, 0U};
    DataCopyPad(expandIdxLocal_, expandIdxGM_[startTokenId * axisK_], bskParams, copyPadint32Params);

    // get per token remote read big offset
    DataCopyExtParams countParams{1U, static_cast<uint32_t>(epWorldSize_ * moeExpertNum_ * sizeof(int32_t)), 0U, 0U,
                                  0U};
    DataCopyPad(globalSendCountLocal_, epSendCountGM_, countParams, copyPadint32Params);

    // get shareAddr locally
    addrAllAlignLen_ = Ceil(sizeof(uint64_t) * epWorldSize_, ADDR_UINT64_ALIGN) * ADDR_UINT64_ALIGN;
    tpipe_->InitBuffer(recvAddrBuf_, addrAllAlignLen_);
    expandXShareAddrLt_ = recvAddrBuf_.Get<uint64_t>();
    DataCopyExtParams addrCopyParams = {1U, static_cast<uint32_t>(addrAllAlignLen_), 0, 0, 0};
    DataCopyPadExtParams<uint64_t> addrCopyExtParams{false, 0U, 0U, 0U};
    localAllAddrGt.SetGlobalBuffer((__gm__ uint64_t *)(shareAddrSpaceGm_));
    // 从metaInfo获取共享地址
    DataCopyPad(expandXShareAddrLt_, localAllAddrGt, addrCopyParams, addrCopyExtParams);

    PipeBarrier<PIPE_ALL>();

    uint32_t expertIdsAlignLen_ = Ceil(axisK_ * sizeof(int32_t), UB_ALIGN) * UB_ALIGN;
    tpipe_->InitBuffer(expertIdsBuf_, expertIdsAlignLen_);
    DataCopyExtParams kParams{1U, static_cast<uint32_t>(axisK_ * sizeof(ExpandIdxType)), 0U, 0U, 0U};
    DataCopyPadExtParams<ExpandIdxType> copyPadParams{false, 0U, 0U, 0U};
    LocalTensor<int32_t> expertIdsTensor_ = expertIdsBuf_.Get<int32_t>();

    DataCopyExtParams xOutCopyParams{1U, static_cast<uint32_t>(hExpandXTypeSize_), 0U, 0U, 0U};
    DataCopyPadExtParams<ExpandXType> copyPadExtParams{false, 0U, 0U, 0U};

    tpipe_->InitBuffer(xOutBuf_, hExpandXAlign32Size_);
    tpipe_->InitBuffer(moeSumQueue_, BUFFER_NUM, hExpandXAlign32Size_);
    tpipe_->InitBuffer(tokenSumBuf_, hFloatAlign32Size_);
    tpipe_->InitBuffer(weightedSumBuf_, hFloatAlign256Size_);
    tpipe_->InitBuffer(sumFloatBuf_, hFloatAlign32Size_);
    tokenReuceTensor_ = tokenSumBuf_.Get<float>();
    weightedSumTensor_ = weightedSumBuf_.Get<float>();
    sumTokenTensor_ = sumFloatBuf_.Get<float>();

    for (uint32_t tokenIndex = startTokenId; tokenIndex < endTokenId; tokenIndex++) {
        uint32_t localTokenId = tokenIndex - startTokenId;
        DataCopyPad(expertIdsTensor_, expertIdsGM_[tokenIndex * axisK_], kParams, copyPadParams);
        PipeBarrier<PIPE_ALL>();

        Duplicate(sumTokenTensor_, static_cast<float>(0), axisH_);

        for (uint32_t topkId = 0U; topkId < axisK_; topkId++) {
            int32_t dstExpertId = expertIdsTensor_.GetValue(topkId);
            int32_t srcRankId = dstExpertId / moeExpertPerRankNum_;
            uint32_t offsetIdx = dstExpertId * epWorldSize_ + epRankId_;
            uint32_t col = offsetIdx % moeExpertNum_;
            int32_t remoteBase = (col == 0) ? 0 : globalSendCountLocal_(offsetIdx - 1); // count 转 offset
            int32_t remoteOffset = expandIdxLocal_(localTokenId * axisK_ + topkId);
            float scale = expertScalesLocal_.GetValue(localTokenId * axisK_ + topkId);
            auto srcPtr = expandXShareAddrLt_.GetValue(srcRankId);
            srcWinGMTensor.SetGlobalBuffer(
                (__gm__ ExpandXType *)(srcPtr + hExpandXTypeSize_ * (remoteBase + remoteOffset)));

            LocalTensor<ExpandXType> tmpToken = moeSumQueue_.AllocTensor<ExpandXType>();
            // Read From Remote
            DataCopyPad(tmpToken, srcWinGMTensor, xOutCopyParams, copyPadExtParams);
            moeSumQueue_.EnQue(tmpToken);
            tmpToken = moeSumQueue_.DeQue<ExpandXType>();
            Cast(tokenReuceTensor_, tmpToken, AscendC::RoundMode::CAST_NONE, axisH_);
            PipeBarrier<PIPE_V>();
            AscendC::Muls(weightedSumTensor_, tokenReuceTensor_, scale, axisH_);
            PipeBarrier<PIPE_V>();
            AscendC::Add(sumTokenTensor_, sumTokenTensor_, weightedSumTensor_, axisH_);
            moeSumQueue_.FreeTensor<ExpandXType>(tmpToken);
            PipeBarrier<PIPE_V>();
        }
        PipeBarrier<PIPE_V>();
        LocalTensor<ExpandXType> xOutLocal = xOutBuf_.Get<ExpandXType>();
        Cast(xOutLocal, sumTokenTensor_, AscendC::RoundMode::CAST_RINT, axisH_);
        SyncFunc<AscendC::HardEvent::V_MTE3>();
        DataCopyPad(XOutGT_[tokenIndex * axisH_], xOutLocal, xOutCopyParams);
    }
}

template<TemplateTypeClass>
__aicore__ inline void CombineLowLatency<TemplateTypeFunc>::Process()
{
    if ASCEND_IS_AIV { // 全aiv处理
        PutShareAddr();
        SetSyncFlag(FLAG); // 全卡同步，确保对称地址都放到了meta空间
        WaitSyncFlag(FLAG);
        GetShareAddr();
        InputToDstOutput();
        if (aivId_ == 0) {
            PRINTF("[------------ Leaving Combine -----------] of Core %d on Rank %d\n", aivId_, epRankId_);
        }
    }
}

} // namespace MoeCombineLowLatency
#endif // ZBAL_KERNEL_DISPATCH_LOWLATENCY_H