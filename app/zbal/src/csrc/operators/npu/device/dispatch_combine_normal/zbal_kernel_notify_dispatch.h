/*

Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
ZBAL is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
     http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
*/
#ifndef ZBAL_KERNEL_NOTIFY_DISPATCH_H
#define ZBAL_KERNEL_NOTIFY_DISPATCH_H

#include <climits>
#include "kernel_operator.h"
#include "zbal_def.h"
#include "zbal_kernel_utils.h"
#include "zbal_kernel_base.h"
#include "zbal_kernel_constant.h"

namespace MoeNotifyDispatch {
using namespace AscendC;
using namespace Moe;
using namespace zbal;

template<AscendC::HardEvent event>
ZBAL_KERNEL void SyncFunc()
{
    int32_t eventID = static_cast<int32_t>(GetTPipePtr()->FetchEventID(event));
    AscendC::SetFlag<event>(eventID);
    AscendC::WaitFlag<event>(eventID);
}

constexpr uint64_t NOTIFY_STATUS_OFFSET = 20UL * 1024UL;
constexpr uint32_t UB_FLAG_SIZE = 8U * 1024U;
constexpr int32_t REBALANCE_MIN_TOKEN_NUM = 128;
constexpr uint64_t ZBAL_NUM2 = 2;
constexpr uint64_t ZBAL_NUM3 = 3;
constexpr uint64_t ZBAL_NUM8 = 8;

template<typename T>
class NotifyDispatch : public ZBALBaseKernel {
public:
    ZBAL_KERNEL NotifyDispatch() {};

    ZBAL_KERNEL void Init(GM_ADDR metaAddr, GM_ADDR tokenPerExpert, int64_t sendCount, uint32_t numTopk, uint32_t rank,
                          GM_ADDR recvData, GM_ADDR totalRecvTokens, GM_ADDR recvTokensPerExpert, GM_ADDR putOffset,
                          GM_ADDR balanceMatrix, float factorHigh, float factorLow, TPipe *pipe)
    {
        blockIdx_ = GetBlockIdx();
        blockNum_ = GetBlockNum();
        epRankId_ = rank;
        len = sendCount;
        numExperts = len / sendPerGroup; // len为 num_tokens_per_expert长度，即专家数
        topkNum_ = numTopk;
        factorHigh = factorHigh;
        factorLow = factorLow;
        pipe_ = pipe;

        gva_gm = (GM_ADDR)metaAddr;
        comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaAddr);
        memSize = comm->localDeviceMemSize;
    myGroupRank = comm->myGroupRank;
    groupSize = comm->groupSize;
        GM_ADDR meta_addr_gm = reinterpret_cast<__gm__ uint8_t *>(comm->myAddressExchangeGva);
        addrOffset_ = (meta_addr_gm - gva_gm);
        metaSize_ = addrOffset_ + comm->sizeForExchangeAddress;
        stateOffset_ = metaSize_ - NOTIFY_STATUS_OFFSET;
        flagOffset_ = metaSize_ - META_FLAG_R_OFFSET;
        epWorldSize_ = comm->groupSize;
        worldRanks = (__gm__ uint16_t *)comm->peerGroupRank2WorldRank;
        assert(comm->sizeForExchangeAddress >= META_FLAG_R_OFFSET * ZBAL_NUM2,
               "The group meta size for exchange is %lluKB, the min value should be %lluKB. \
            addrOffset_:%d, epRankId:%d, epWorldSize:%d, moeExpertNum:%d, shareAddrNum:%d\n",
               comm->sizeForExchangeAddress / KB_SIZE, META_FLAG_R_OFFSET * ZBAL_NUM2 / KB_SIZE, addrOffset_, epRankId_,
               epWorldSize_, numExperts, shareAddrNum);

        tokenPerExpertData_ = tokenPerExpert;
        totalRecvTokens_ = totalRecvTokens;
        recvTokensPerExpert_ = recvTokensPerExpert;

        tokenPerExpertDataInput = (__gm__ int32_t *)tokenPerExpert;
        tokenPerExpertDataInputGt.SetGlobalBuffer((__gm__ int32_t *)tokenPerExpertDataInput);
        recvDataOutput = (__gm__ T *)recvData;
        recvDataOutputGt.SetGlobalBuffer((__gm__ T *)recvDataOutput);
        recvDataGt_.SetGlobalBuffer((__gm__ int32_t *)recvDataOutput);
        recvCntGt.SetGlobalBuffer((__gm__ int32_t *)putOffset);
        balanceMatrixGt.SetGlobalBuffer((__gm__ int32_t *)balanceMatrix); // shape:[epWorldSize_, epWorldSize_*2]

        addrUint64AlignLen_ = Ceil(shareAddrNum * sizeof(uint64_t), UB_ALIGN) * UB_ALIGN;
        recvDataAlignLen_ = Ceil(numExperts * epWorldSize_ * sizeof(int32_t), UB_ALIGN) * UB_ALIGN;
        tokenPerExpertDataAlignLen_ = Ceil(numExperts * sizeof(int32_t), UB_ALIGN) * UB_ALIGN;
        allRecvCountDataAlignLen_ = Ceil(numExperts * epWorldSize_ * sizeof(int32_t), UB_ALIGN) * UB_ALIGN;
        matrixDataAlignLen_ = Ceil(epWorldSize_ * epWorldSize_ * ZBAL_NUM2 * sizeof(int32_t), UB_ALIGN) * UB_ALIGN;

        pipe_->InitBuffer(tBuf, UB_FLAG_SIZE);
        pipe_->InitBuffer(addrBuf_, addrUint64AlignLen_);
        SplitCoreCal(epWorldSize_, rankNumPerBlock, curBlockStartRankId, curBlockEndRankId);
    }

    ZBAL_KERNEL void Process()
    {
        ResetMetaState();
        PutShareAddr();
        SetSyncFlag(FLAG); // 全卡同步，确保对称地址都放到了meta空间
        WaitSyncFlag(FLAG);

        GetShareAddr();
        AllGatherSendData(); // allgather 每个rank的sendCount
        SetSyncFlag(STATE);  // 全卡同步，确保数据已经获取完
        WaitSyncFlag(STATE);

        ReloadRecvData();
        BuildRecvCount();
        BuildMaxBsAndBalanceMatrix();
        BuildTotalRecvTokens();
        BuildRecvTokenPerExp();
        SyncAll<true>();
    }

private:
    int64_t blockIdx_; // Index of the current aicore
    int64_t blockNum_; // Total number of aicores for the current epRankId_
    uint32_t rankNumPerBlock;
    uint32_t curBlockStartRankId;
    uint32_t curBlockEndRankId;

    int64_t len;
    int epRankId_;
    int epWorldSize_;
    int topkNum_;
    int numExperts;
    int sendPerGroup = 1;

    GlobalTensor<int> tokenPerExpertDataInputGt;
    GlobalTensor<T> recvDataOutputGt;
    GlobalTensor<int32_t> recvDataGt_;
    GlobalTensor<int32_t> recvCntGt;
    GlobalTensor<int32_t> balanceMatrixGt;

    LocalTensor<int32_t> sendCountTensor_;
    LocalTensor<int32_t> sendOffsetTensor;
    LocalTensor<int32_t> recvDataTensor_;
    uint32_t addrUint64AlignLen_{0};
    uint32_t sendDataAlignLen_{0};
    uint32_t tokenPerExpertDataAlignLen_{0};
    uint32_t allRecvCountDataAlignLen_{0};
    uint32_t recvDataAlignLen_{0};
    uint32_t sendDataOffsetAlignLen{0};
    uint32_t matrixDataAlignLen_{0};

    TPipe *pipe_{nullptr};
    TBuf<QuePosition::VECCALC> tBuf;
    TBuf<> addrBuf_;
    TBuf<> statusBuf_;
    TBuf<> waitStatusBuf_;
    TBuf<> gatherMaskOutBuf_;
    TBuf<> statusSumBuf_;
    TBuf<> tokenPerExpertDataBuf_;
    TBuf<> sendCountBuf_;
    TBuf<> recvDataBuf_;
    TBuf<> localRecvDataBuf_;
    TBuf<> tmpBuf_;
    TBuf<> tmpBuf2_;
    TBuf<> tmpBuf3_;
    TBuf<> tmpBuf4_;
    TBuf<> matrixBuf_;

    __gm__ int *tokenPerExpertDataInput;
    __gm__ T *recvDataOutput;
    __gm__ int32_t *allRecvCountOutput_;
    GM_ADDR tokenPerExpertData_;
    GM_ADDR totalRecvTokens_;
    GM_ADDR allRecvCount_;
    GM_ADDR maxBs_;
    GM_ADDR recvTokensPerExpert_;
    GM_ADDR recvData_;

    GM_ADDR balanceMatrix_;
    float factorHigh{1.2};
    float factorLow{1.0};
    int32_t bsPerRank[ZBAL_MAX_RANK_SIZE];       // 记录每个rank上的bs, size=epWorldSize_
    int32_t processCapacity[ZBAL_MAX_RANK_SIZE]; // 记录每个rank的剩余处理能力, size=epWorldSize_

    GM_ADDR gva_gm;
    uint64_t metaSize_ = 0;
    uint64_t addrOffset_ = 0;
    uint64_t stateOffset_ = 0;
    uint64_t flagOffset_ = 0;
    // List of asymmetric output addresses (tokenPerExpertData_)
    uint64_t shareTokenPerExpertAddrs[ZBAL_MAX_RANK_SIZE];
    uint32_t shareAddrNum{1};

    ZBAL_KERNEL void SplitCoreCal(uint32_t totalNum, uint32_t &perCoreNum, uint32_t &startIdx, uint32_t &endIdx)
    {
        perCoreNum = totalNum / blockNum_;
        uint32_t remainderRankNum = totalNum % blockNum_;

        startIdx = perCoreNum * blockIdx_;
        if (blockIdx_ < remainderRankNum) {
            perCoreNum++;
            startIdx += blockIdx_;
        } else {
            startIdx += remainderRankNum;
        }
        endIdx = startIdx + perCoreNum;
    }

    ZBAL_KERNEL GM_ADDR GetMetaAddrByRankId(const int32_t rankId, const int metaType)
    {
        auto ptr = ZbalPtr(gva_gm, rankId);
        // sizeForExchangeAddress: |- addr -|- flag -|- state -|
        //                               -50kb    -20kb      -0kb
        switch (metaType) {
            case STATE: // 存放通信结束的state, 12KB
                return (GM_ADDR)(ptr) + stateOffset_;
            case ADDR: // 存放交换的共享地址
                return (GM_ADDR)(ptr) + addrOffset_;
            case FLAG: // 存放第一次清理state空间后的同步flag, 12KB
                return (GM_ADDR)(ptr) + flagOffset_;
            default:
                return (GM_ADDR)(ptr);
        }
    }

    ZBAL_KERNEL void ResetMetaState()
    {
        if (rankNumPerBlock == 0U) {
            return;
        }
        uint32_t waitStatusBufSize = (((rankNumPerBlock * UB_ALIGN) > 256) ? (rankNumPerBlock * UB_ALIGN) : 256);
        pipe_->InitBuffer(waitStatusBuf_, waitStatusBufSize); // ranks/48 * 32B = 1 * 32B

        GlobalTensor<float> statusFp32TensorGT;
        auto ptr = GetMetaAddrByRankId(epRankId_, STATE);
        statusFp32TensorGT.SetGlobalBuffer((__gm__ float *)(ptr));

        DataCopyParams intriOutParams{static_cast<uint16_t>(rankNumPerBlock), 1, 0, 0};
        uint64_t duplicateMask[2] = {0x101010101010101, 0};
        LocalTensor<int32_t> cleanStateTensor = waitStatusBuf_.Get<int32_t>();
        SyncFunc<AscendC::HardEvent::S_V>();
        Duplicate<int32_t>(cleanStateTensor, 0, duplicateMask, Ceil(rankNumPerBlock, ZBAL_NUM8), 1, ZBAL_NUM8);
        SyncFunc<AscendC::HardEvent::V_MTE3>();
        DataCopy(statusFp32TensorGT[curBlockStartRankId * STATE_OFFSET / sizeof(float)],
                 cleanStateTensor.ReinterpretCast<float>(), intriOutParams);
        SyncFunc<AscendC::HardEvent::MTE3_S>();
    }

    ZBAL_KERNEL void SetSyncFlag(int metaType)
    {
        if (rankNumPerBlock == 0U) {
            SyncAll<true>();
            return;
        }
        uint32_t statusCntAlign = Ceil(rankNumPerBlock, ZBAL_NUM8) * ZBAL_NUM8;
        pipe_->InitBuffer(statusBuf_, statusCntAlign * UB_ALIGN);
        LocalTensor statusTensor = statusBuf_.Get<int32_t>();
        Duplicate<int32_t>(statusTensor, 0, rankNumPerBlock * ZBAL_NUM8);
        uint64_t mask[2] = {0x101010101010101, 0};
        PipeBarrier<PIPE_V>();
        Duplicate<int32_t>(statusTensor, 0x3F800000, mask, statusCntAlign / ZBAL_NUM8, 1, ZBAL_NUM8);
        PipeBarrier<PIPE_ALL>();
        SyncAll<true>();

        AscendC::GlobalTensor<int32_t> gmRemoteStatusGt;
        for (uint32_t i = curBlockStartRankId; i < curBlockEndRankId; i++) {
            auto ptr = GetMetaAddrByRankId(i, metaType) + epRankId_ * STATE_OFFSET;
            gmRemoteStatusGt.SetGlobalBuffer((__gm__ int32_t *)(ptr));
            DataCopy<int32_t>(gmRemoteStatusGt, statusTensor[(i - curBlockStartRankId) * ZBAL_NUM8], ZBAL_NUM8);
        }
        SyncFunc<AscendC::HardEvent::MTE3_S>();
    }

    ZBAL_KERNEL void WaitSyncFlag(int metaType)
    {
        if (rankNumPerBlock == 0U) {
            SyncAll<true>();
            return;
        }
        uint32_t waitStatusBufSize = (((rankNumPerBlock * UB_ALIGN) > 256) ? (rankNumPerBlock * UB_ALIGN) : 256);
        pipe_->InitBuffer(waitStatusBuf_, waitStatusBufSize); // ranks/48 * 32B = 1 * 32B
        uint32_t maskAlign = Ceil(epWorldSize_ * sizeof(float), UB_ALIGN) * UB_ALIGN;
        pipe_->InitBuffer(gatherMaskOutBuf_, maskAlign); // rankSize * 4B
        pipe_->InitBuffer(statusSumBuf_, UB_ALIGN);      // 32B

        LocalTensor<float> gatherMaskOutTensor = gatherMaskOutBuf_.Get<float>();
        LocalTensor<float> statusSumOutTensor = statusSumBuf_.Get<float>(UB_ALIGN);
        LocalTensor<float> statusFp32Tensor = waitStatusBuf_.Get<float>();
        GlobalTensor<float> statusFp32TensorGT;
        auto ptr = GetMetaAddrByRankId(epRankId_, metaType);
        statusFp32TensorGT.SetGlobalBuffer((__gm__ float *)(ptr));
        uint32_t mask = 1;
        float compareTarget = static_cast<float>(1.0) * rankNumPerBlock;
        float sumOfFlag = static_cast<float>(-1.0);
        DataCopyParams intriParams{static_cast<uint16_t>(rankNumPerBlock), 1, 0, 0};

        SyncFunc<AscendC::HardEvent::S_V>();
        uint64_t timeoutCheckStart = static_cast<uint64_t>(GetSystemCycle());
        uint64_t timeoutCheckEnd;
        uint64_t timeoutCheckDuration;
        while (sumOfFlag != compareTarget) {
            DataCopy(statusFp32Tensor, statusFp32TensorGT[curBlockStartRankId * STATE_OFFSET / sizeof(float)],
                     intriParams);
            SyncFunc<AscendC::HardEvent::MTE2_V>();
            ReduceSum(statusSumOutTensor, statusFp32Tensor, gatherMaskOutTensor, mask, rankNumPerBlock, 1);
            SyncFunc<AscendC::HardEvent::V_S>();
            sumOfFlag = statusSumOutTensor.GetValue(0);
            if (metaType == 0) {
                timeoutCheckEnd = static_cast<uint64_t>(GetSystemCycle());
                timeoutCheckDuration = (timeoutCheckEnd - timeoutCheckStart) / CYCLE_TO_TIME;
                assert(timeoutCheckDuration < TIMEOUT_DETECTION_THRESHOLD);
            }
        }

        // 清标记位
        SyncFunc<AscendC::HardEvent::MTE3_S>();
        DataCopyParams intriOutParams{static_cast<uint16_t>(rankNumPerBlock), 1, 0, 0};
        uint64_t duplicateMask[2] = {0x101010101010101, 0};
        LocalTensor<int32_t> cleanStateTensor = waitStatusBuf_.Get<int32_t>();
        SyncFunc<AscendC::HardEvent::S_V>();
        Duplicate<int32_t>(cleanStateTensor, 0, duplicateMask, Ceil(rankNumPerBlock, ZBAL_NUM8), 1, ZBAL_NUM8);
        SyncFunc<AscendC::HardEvent::V_MTE3>();
        DataCopy(statusFp32TensorGT[curBlockStartRankId * STATE_OFFSET / sizeof(float)],
                 cleanStateTensor.ReinterpretCast<float>(), intriOutParams);
        SyncFunc<AscendC::HardEvent::MTE3_S>();

        SyncAll<true>();
    }

    ZBAL_KERNEL void AllGatherSendData()
    {
        if (rankNumPerBlock == 0U) {
            return;
        }
        pipe_->InitBuffer(tokenPerExpertDataBuf_, tokenPerExpertDataAlignLen_);
        LocalTensor sendDataLt = tokenPerExpertDataBuf_.Get<int32_t>();
        DataCopyExtParams copyParams(1, static_cast<uint32_t>(numExperts * sizeof(int32_t)), 0, 0, 0);
        DataCopyPadExtParams<int32_t> padParams{false, 0U, 0U, 0U};

        GlobalTensor<int32_t> gmRemoteDataGt;
        for (uint32_t targetRankId = curBlockStartRankId; targetRankId < curBlockEndRankId; targetRankId++) {
            auto ptr = shareTokenPerExpertAddrs[targetRankId];
            gmRemoteDataGt.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ptr));

            DataCopyPad(sendDataLt, gmRemoteDataGt, copyParams, padParams);
            SyncFunc<AscendC::HardEvent::MTE2_MTE3>();
            DataCopyPad(recvDataGt_[targetRankId * numExperts], sendDataLt, copyParams);
            SyncFunc<AscendC::HardEvent::MTE3_MTE2>();
        }
    }

    ZBAL_KERNEL void ReloadRecvData()
    {
        pipe_->Reset();
        pipe_->InitBuffer(recvDataBuf_, recvDataAlignLen_);

        recvDataTensor_ = recvDataBuf_.Get<int32_t>();
        DataCopyExtParams recvDataParams = {1U, static_cast<uint32_t>(numExperts * epWorldSize_ * sizeof(int32_t)), 0,
                                            0, 0};
        DataCopyPadExtParams<int32_t> DataCopyPadExtParams{false, 0U, 0U, 0U};
        SyncFunc<AscendC::HardEvent::MTE3_MTE2>();
        DataCopyPad(recvDataTensor_, recvDataGt_, recvDataParams, DataCopyPadExtParams);
        PipeBarrier<PIPE_ALL>();
    }

    ZBAL_KERNEL void ReorderRecvDataOutput(int32_t rankId, LocalTensor<int32_t> &transLt, bool isCumSum = false)
    {
        uint32_t moeExpertPerRankNum = numExperts / epWorldSize_;
        uint32_t startExpId = rankId * moeExpertPerRankNum;
        uint32_t endExpId = rankId * moeExpertPerRankNum + moeExpertPerRankNum;

        SyncFunc<AscendC::HardEvent::V_S>();
        SyncFunc<AscendC::HardEvent::MTE2_S>();
        // 对recv_data进行转置
        int32_t prefixSum = 0; // 每卡求前缀和，调整为偏移，起始偏移从0开始
        for (uint32_t expId = startExpId; expId < endExpId; ++expId) {
            for (uint32_t srcRank = 0; srcRank < epWorldSize_; ++srcRank) {
                uint32_t index = (expId - startExpId) * epWorldSize_ + srcRank;
                uint32_t pairIdx = srcRank * numExperts + expId;

                int32_t curRecvCount = recvDataTensor_(pairIdx);
                transLt(index) = isCumSum ? prefixSum : curRecvCount; // 根据是否需要前缀和进行填充
                prefixSum += curRecvCount;
            }
        }
        PipeBarrier<PIPE_ALL>();
        SyncFunc<AscendC::HardEvent::S_MTE2>();
    }

    ZBAL_KERNEL void BuildMaxBsAndBalanceMatrix()
    {
        if (blockIdx_ != (blockNum_ - ZBAL_NUM3)) {
            return;
        }
        // 需要recvData
        pipe_->InitBuffer(localRecvDataBuf_, Ceil(numExperts * sizeof(int32_t), UB_ALIGN) * UB_ALIGN);
        pipe_->InitBuffer(tmpBuf_, Ceil(numExperts * sizeof(int32_t), UB_ALIGN) * UB_ALIGN);
        pipe_->InitBuffer(tmpBuf2_, Ceil(numExperts * sizeof(float), UB_ALIGN) * UB_ALIGN);
        pipe_->InitBuffer(tmpBuf3_, Ceil(numExperts * sizeof(float), UB_ALIGN) * UB_ALIGN);
        pipe_->InitBuffer(tmpBuf4_, Ceil(numExperts * sizeof(float), UB_ALIGN) * UB_ALIGN);

        DataCopyExtParams copyParams = {1U, static_cast<uint32_t>(numExperts * sizeof(int32_t)), 0, 0, 0};
        DataCopyPadExtParams<int32_t> copyPadExtParams{false, 0U, 0U, 0U};

        LocalTensor<int32_t> tokenPerExpertDataLt = localRecvDataBuf_.Get<int32_t>();
        LocalTensor<int32_t> maxBsLt = tmpBuf_.Get<int32_t>();
        LocalTensor<float> floatExpTokenCntLt = tmpBuf2_.Get<float>();
        LocalTensor<float> floatExpTokenSumCntLt = tmpBuf3_.Get<float>();
        LocalTensor<float> sharedTmpBuffer = tmpBuf4_.Get<float>();

        int32_t maxBsNum = 0;
        int32_t totalTokens = 0;
        for (uint32_t srcRankId = 0; srcRankId < epWorldSize_; srcRankId++) {
            DataCopy(tokenPerExpertDataLt, recvDataTensor_[numExperts * srcRankId], numExperts);
            PipeBarrier<PIPE_ALL>();
            SyncFunc<AscendC::HardEvent::MTE2_V>();

            Cast(floatExpTokenCntLt, tokenPerExpertDataLt, RoundMode::CAST_NONE, numExperts);
            PipeBarrier<PIPE_V>();
            ReduceSum(floatExpTokenSumCntLt, floatExpTokenCntLt, sharedTmpBuffer, numExperts);
            SyncFunc<AscendC::HardEvent::V_S>();

            int32_t curRankBsNum = static_cast<int32_t>(floatExpTokenSumCntLt(0)) / topkNum_;
            maxBsNum = curRankBsNum > maxBsNum ? curRankBsNum : maxBsNum;
            totalTokens += curRankBsNum;
            bsPerRank[srcRankId] = curRankBsNum;
            PipeBarrier<PIPE_V>();
        }
        PipeBarrier<PIPE_V>();
        BuildBalanceMatrix(totalTokens);
    }

    ZBAL_KERNEL void BuildBalanceMatrix(int32_t totalTokens)
    {
        pipe_->InitBuffer(matrixBuf_, matrixDataAlignLen_);
        // 跨rank分配token处理范围
        int32_t avgTokens = static_cast<int32_t>(totalTokens / epWorldSize_); // 小于128时，每卡仅处理自己的
        int32_t capacity = 0;
        for (uint32_t i = 0; i < epWorldSize_; ++i) {
            if (bsPerRank[i] > avgTokens) {
                capacity = ScalarCast<float, int32_t, RoundMode::CAST_CEIL>(avgTokens * factorHigh);
            } else {
                capacity = ScalarCast<float, int32_t, RoundMode::CAST_CEIL>(avgTokens * factorLow);
            }
            processCapacity[i] = capacity;
        }
        LocalTensor<int32_t> balanceMatrixLt = matrixBuf_.Get<int32_t>();
        Duplicate<int32_t>(balanceMatrixLt, -1, matrixDataAlignLen_);
        PipeBarrier<PIPE_V>();

        // 每个rank先处理自己的token
        bool isNeedBalance = avgTokens > REBALANCE_MIN_TOKEN_NUM;
        int32_t offset = epWorldSize_ * ZBAL_NUM2; // 一行的个数
        int32_t processCount = 0;
        for (uint32_t i = 0; i < epWorldSize_; ++i) {
            if (bsPerRank[i] > 0) {
                if (!isNeedBalance) {
                    processCount = bsPerRank[i]; // 本卡处理完自己的所有token
                } else {
                    processCount = bsPerRank[i] > processCapacity[i] ? processCapacity[i] : bsPerRank[i];
                }
                balanceMatrixLt(i * offset + i * ZBAL_NUM2) = 0;
                balanceMatrixLt(i * offset + i * ZBAL_NUM2 + 1) = processCount - 1;
                processCapacity[i] -= processCount;
            }
        }
        // 处理其他rank的剩余token
        for (uint32_t tarRankId = 0; tarRankId < epWorldSize_; ++tarRankId) {
            if (!isNeedBalance || bsPerRank[tarRankId] == 0) {
                continue;
            }
            // 计算目标rank上已经处理了多少token
            int32_t processedCnt = 0;
            for (int32_t srcRankId = 0; srcRankId < epWorldSize_; ++srcRankId) {
                if (balanceMatrixLt.GetValue(srcRankId * offset + tarRankId * ZBAL_NUM2) != -1) {
                    int32_t startId = balanceMatrixLt.GetValue(srcRankId * offset + tarRankId * ZBAL_NUM2);
                    int32_t endId = balanceMatrixLt.GetValue(srcRankId * offset + tarRankId * ZBAL_NUM2 + 1);
                    processedCnt += (endId - startId + 1);
                }
            }
            int32_t remainTokens = bsPerRank[tarRankId] - processedCnt;
            if (remainTokens <= 0) {
                continue;
            }
            // 寻找有处理能力的rank分担
            int32_t curStart = processedCnt;
            for (int32_t helpRank = 0; helpRank < epWorldSize_; ++helpRank) {
                if (helpRank == tarRankId) {
                    continue;
                }
                if (processCapacity[helpRank] > 0 && remainTokens > 0) {
                    int32_t allocCnt =
                        processCapacity[helpRank] > remainTokens ? remainTokens : processCapacity[helpRank];
                    int32_t startId = curStart;
                    int32_t endId = startId + allocCnt - 1;

                    balanceMatrixLt(helpRank * offset + tarRankId * ZBAL_NUM2) = startId;
                    balanceMatrixLt(helpRank * offset + tarRankId * ZBAL_NUM2 + 1) = endId;

                    processCapacity[helpRank] -= allocCnt;
                    remainTokens -= allocCnt;
                    curStart += allocCnt;
                }
            }
        }
        PipeBarrier<PIPE_ALL>();
        SyncFunc<AscendC::HardEvent::S_MTE3>();
        DataCopyExtParams params{1, static_cast<uint32_t>(epWorldSize_ * epWorldSize_ * 2 * sizeof(int32_t)), 0, 0, 0};
        DataCopyPad(balanceMatrixGt, balanceMatrixLt, params);
    }

    ZBAL_KERNEL void BuildRecvCount()
    {
        uint32_t maxUseCoreNum = blockNum_;
        uint32_t perCoreNum = epWorldSize_ / maxUseCoreNum;
        uint32_t remainderRankNum = epWorldSize_ % maxUseCoreNum;

        uint32_t startRankId = perCoreNum * blockIdx_;
        if (blockIdx_ < remainderRankNum) {
            perCoreNum += 1;
            startRankId += blockIdx_;
        } else {
            startRankId += remainderRankNum;
        }
        uint32_t endRankId = startRankId + perCoreNum;
        if (perCoreNum == 0U || blockIdx_ >= maxUseCoreNum) {
            return;
        }

        pipe_->InitBuffer(sendCountBuf_, Ceil(numExperts * sizeof(int32_t), UB_ALIGN) * UB_ALIGN);
        LocalTensor<int32_t> recvTokenLt = sendCountBuf_.Get<int32_t>();

        for (uint32_t rank = startRankId; rank < endRankId; ++rank) {
            // 每卡求前缀和
            ReorderRecvDataOutput(rank, recvTokenLt, true); // localExpNum * ranks

            SyncFunc<AscendC::HardEvent::MTE2_MTE3>();
            DataCopyExtParams copyParams{1, static_cast<uint32_t>(numExperts * sizeof(int32_t)), 0, 0, 0};
            DataCopyPad(recvCntGt[rank * numExperts], recvTokenLt, copyParams);
        }
    }

    ZBAL_KERNEL void BuildTotalRecvTokens()
    {
        if (blockIdx_ != (blockNum_ - ZBAL_NUM2)) {
            return;
        }
        // 需要recvData, 转置后取当前rank的部分
        pipe_->InitBuffer(localRecvDataBuf_, Ceil(numExperts * sizeof(int32_t), UB_ALIGN) * UB_ALIGN);
        pipe_->InitBuffer(tmpBuf_, Ceil(1 * sizeof(int32_t), UB_ALIGN) * UB_ALIGN);
        pipe_->InitBuffer(tmpBuf2_, Ceil(numExperts * sizeof(float), UB_ALIGN) * UB_ALIGN);
        pipe_->InitBuffer(tmpBuf3_, Ceil(numExperts * sizeof(float), UB_ALIGN) * UB_ALIGN);
        pipe_->InitBuffer(tmpBuf4_, Ceil(numExperts * sizeof(float), UB_ALIGN) * UB_ALIGN);

        LocalTensor<int32_t> recvTokenLt = localRecvDataBuf_.Get<int32_t>();
        LocalTensor<int32_t> totalCntLt = tmpBuf_.Get<int32_t>();
        LocalTensor<float> floatExpTokenCntLt = tmpBuf2_.Get<float>();
        LocalTensor<float> floatExpTokenSumCntLt = tmpBuf3_.Get<float>();
        LocalTensor<float> sharedTmpBuffer = tmpBuf4_.Get<float>();

        // 只需要计算当前rank接收的token数
        ReorderRecvDataOutput(epRankId_, recvTokenLt, false); // localExpNum * ranks
        SyncFunc<AscendC::HardEvent::MTE2_V>();
        Cast(floatExpTokenCntLt, recvTokenLt, RoundMode::CAST_NONE, numExperts);
        PipeBarrier<PIPE_V>();
        ReduceSum(floatExpTokenSumCntLt, floatExpTokenCntLt, sharedTmpBuffer, numExperts);
        SyncFunc<AscendC::HardEvent::V_S>();
        int32_t recvCnt = static_cast<int32_t>(floatExpTokenSumCntLt.GetValue(0));
        PipeBarrier<PIPE_ALL>();

        // 拷贝到outputGT
        GlobalTensor<int32_t> totalCntGt;
        totalCntGt.SetGlobalBuffer((__gm__ int32_t *)totalRecvTokens_);
        totalCntGt.SetValue(0, recvCnt);
        DataCacheCleanAndInvalid<int32_t, CacheLine::SINGLE_CACHE_LINE, DcciDst::CACHELINE_OUT>(totalCntGt);
    }

    ZBAL_KERNEL void BuildRecvTokenPerExp()
    {
        if (blockIdx_ != (blockNum_ - 1)) {
            return;
        }
        // 需要recvData, 转置后取当前rank的部分
        uint32_t moeExpertPerRankNum = numExperts / epWorldSize_;
        pipe_->InitBuffer(localRecvDataBuf_, Ceil(numExperts * sizeof(int32_t), UB_ALIGN) * UB_ALIGN);
        pipe_->InitBuffer(tmpBuf_, Ceil(moeExpertPerRankNum * sizeof(int64_t), UB_ALIGN) * UB_ALIGN);

        LocalTensor<int32_t> recvTokenLt = localRecvDataBuf_.Get<int32_t>();
        ReorderRecvDataOutput(epRankId_, recvTokenLt, false); // localExpNum * ranks
        SyncFunc<AscendC::HardEvent::MTE2_S>();

        LocalTensor<int64_t> tmpTensor = tmpBuf_.Get<int64_t>();
        for (uint32_t expId = 0; expId < moeExpertPerRankNum; ++expId) {
            int64_t localRecvCount = 0;
            for (uint32_t srcRank = 0; srcRank < epWorldSize_; ++srcRank) {
                uint32_t index = expId * epWorldSize_ + srcRank;
                localRecvCount += recvTokenLt(index);
            }
            tmpTensor(expId) = localRecvCount;
        }
        PipeBarrier<PIPE_ALL>();
        SyncFunc<AscendC::HardEvent::S_MTE2>();
        GlobalTensor<int64_t> recvTokenPerExpGt;
        recvTokenPerExpGt.SetGlobalBuffer((__gm__ int64_t *)recvTokensPerExpert_);
        DataCopyExtParams copyParams{1, static_cast<uint32_t>(moeExpertPerRankNum * sizeof(int64_t)), 0, 0, 0};
        SyncFunc<AscendC::HardEvent::MTE2_MTE3>();
        DataCopyPad(recvTokenPerExpGt, tmpTensor, copyParams);
    }

    ZBAL_KERNEL void PutShareAddr()
    {
        if (blockIdx_ != 0) {
            return;
        }
        LocalTensor<uint64_t> addrTensor_ = addrBuf_.Get<uint64_t>();
        uint64_t tokenPerExpertAddr = reinterpret_cast<__gm__ uint64_t>(tokenPerExpertData_);
        addrTensor_(0) = tokenPerExpertAddr;
        SyncFunc<AscendC::HardEvent::S_MTE3>();
        SyncFunc<AscendC::HardEvent::MTE2_MTE3>();

        AscendC::GlobalTensor<uint64_t> metaDataGt;
        DataCopyExtParams copyParams{1, static_cast<uint32_t>(shareAddrNum * sizeof(uint64_t)), 0, 0, 0};
        GM_ADDR remote_meta = GetMetaAddrByRankId(epRankId_, ADDR);
        metaDataGt.SetGlobalBuffer((__gm__ uint64_t *)(remote_meta));
        DataCopyPad(metaDataGt, addrTensor_, copyParams);
    }

    ZBAL_KERNEL void GetShareAddr()
    {
        LocalTensor<uint64_t> addrTensor_ = addrBuf_.Get<uint64_t>();
        DataCopyExtParams copyParams = {1U, static_cast<uint32_t>(addrUint64AlignLen_), 0, 0, 0};
        DataCopyPadExtParams<uint64_t> copyExtParams{false, 0U, 0U, 0U};

        // 将共享地址保存
        for (uint32_t i = 0; i < epWorldSize_; i++) {
            GM_ADDR meta_addr = GetMetaAddrByRankId(i, ADDR);
            AscendC::GlobalTensor<uint64_t> shareAddrGt;
            shareAddrGt.SetGlobalBuffer((__gm__ uint64_t *)(meta_addr));

            SyncFunc<AscendC::HardEvent::MTE3_MTE2>();
            DataCopyPad(addrTensor_, shareAddrGt, copyParams, copyExtParams);
            SyncFunc<AscendC::HardEvent::MTE2_S>();
            shareTokenPerExpertAddrs[i] = addrTensor_(0);
        }
    }
};

} // namespace MoeNotifyDispatch
#endif // ZBAL_KERNEL_NOTIFY_DISPATCH_H