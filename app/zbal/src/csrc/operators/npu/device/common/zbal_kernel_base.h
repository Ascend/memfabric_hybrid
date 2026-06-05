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

#ifndef ZBAL_KERNEL_BASE_H
#define ZBAL_KERNEL_BASE_H

#include <acl/acl_rt.h>
#include "kernel_operator.h"
#include "zbal_def.h"
#include "zbal_defines.h"
#include "zbal_kernel_utils.h"
#include "zbal_kernel_trace.h"
#include "zbal_kernel_sdma_data_op.h"

using namespace zbal;

class ZBALBaseKernel {
public:
    ZBAL_KERNEL ZBALBaseKernel() {};

    ZBAL_KERNEL void Init()
    {
        pipe.InitBuffer(bindQueue, 1, UB_DMA_MAX_SIZE);
    }

    ZBAL_KERNEL __gm__ void *ZbalPtr(__gm__ void *ptr, int dstPe)
    {
        int worldDstPe = static_cast<int>(*((__gm__ uint16_t *)(worldRanks + dstPe)));
        int worldCurPe = static_cast<int>(*((__gm__ uint16_t *)(worldRanks + myGroupRank)));
        uint64_t curPtr = reinterpret_cast<uint64_t>(ptr);
        uint64_t dstPtr = curPtr + (worldDstPe - worldCurPe) * memSize;
        return reinterpret_cast<__gm__ void *>(dstPtr);
    }

    ZBAL_KERNEL void BarrierAll(bool flag = true, bool stat = true)
    {
        AscendC::SyncAll<true>();
        const uint64_t barrierMagic = 1024;
        int64_t aivIndex = AscendC::GetBlockIdx();
        int64_t aivNum = AscendC::GetBlockNum();
        uint16_t startRank = groupSize + 1;
        uint16_t endRank = groupSize;
        __gm__ uint64_t *flagAddr = reinterpret_cast<__gm__ uint64_t *>(comm->myParamDataGva);
        __gm__ uint64_t *statAddr =
            reinterpret_cast<__gm__ uint64_t *>(comm->myParamDataGva + zbal::ZBAL_OPERATE_PARAM_SIZE / 2);
        if (groupSize <= aivNum) {
            if (aivIndex < groupSize) {
                startRank = aivIndex;
                endRank = startRank + 1;
            }
        } else {
            uint16_t avg = groupSize / aivNum;
            uint16_t remain = groupSize % aivNum;
            startRank = aivIndex * avg;
            if (aivIndex < remain) {
                avg += 1;
                startRank += aivIndex;
            } else {
                startRank += remain;
            }
            endRank = startRank + avg;
        }

        if (flag) {
            // flag
            for (uint16_t rank = startRank; rank < endRank; rank++) {
                AscendC::PipeBarrier<PIPE_ALL>();
                auto ptr = ZbalPtr(flagAddr, rank);
                ZBALSetFlag(ptr, barrierMagic, myGroupRank);
            }
            for (uint16_t rank = startRank; rank < endRank; rank++) {
                AscendC::PipeBarrier<PIPE_ALL>();
                uint64_t readyFlag;
                do {
                    readyFlag = ZBALGetFlag(flagAddr, rank);
                } while (readyFlag != barrierMagic);

                AscendC::PipeBarrier<PIPE_ALL>();
                ZBALSetFlag(flagAddr, 0, rank);
            }
        }

        if (stat) {
            // stat
            for (uint16_t rank = startRank; rank < endRank; rank++) {
                AscendC::PipeBarrier<PIPE_ALL>();
                auto ptr = ZbalPtr(statAddr, rank);
                ZBALSetFlag(ptr, barrierMagic, myGroupRank);
            }
            for (uint16_t rank = startRank; rank < endRank; rank++) {
                AscendC::PipeBarrier<PIPE_ALL>();
                uint64_t readyFlag;
                do {
                    readyFlag = ZBALGetFlag(statAddr, rank);
                } while (readyFlag != barrierMagic);

                AscendC::PipeBarrier<PIPE_ALL>();
                ZBALSetFlag(statAddr, 0, rank);
            }
        }

        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::SyncAll<true>();
    }

    ZBAL_KERNEL void ClearExchange(__gm__ uint64_t *exchangeMeta, uint32_t size)
    {
        uint32_t copyUbNum = UB_DMA_MAX_SIZE / sizeof(uint64_t);

        for (uint32_t offset = 0; offset < size; offset += copyUbNum) {
            uint32_t chunkSize = (offset + copyUbNum <= size) ? copyUbNum : (size - offset);

            AscendC::LocalTensor<uint64_t> localTensor = bindQueue.AllocTensor<uint64_t>();
            AscendC::LocalTensor<uint32_t> localTensorI32 = localTensor.ReinterpretCast<uint32_t>();
            AscendC::Duplicate<uint32_t>(localTensorI32, 0, copyUbNum * ZBAL_TYPE_SIZE_TWO);
            AscendC::PipeBarrier<PIPE_V>();

            GlobalTensor<uint64_t> globalBuf;
            globalBuf.SetGlobalBuffer(exchangeMeta + offset, chunkSize);
            AscendC::DataCopyExtParams copyParams(1, chunkSize * sizeof(uint64_t), 0, 0, 0);

            AscendC::DataCopyPad(globalBuf, localTensor, copyParams);
            SyncFunc<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID0);

            bindQueue.FreeTensor(localTensor);
        }
    }

protected:
    template<typename T>
    ZBAL_KERNEL void CpGM2GM(AscendC::GlobalTensor<T> inputTensor, AscendC::GlobalTensor<T> outputTensor,
                             uint32_t elemNum, bool atomic = false, uint32_t atomicOp = 0)
    {
        if (dataOpType == ZBAL_DATA_OP_DEVICE_SDMA) {
            CpGM2GMSDMA(inputTensor, outputTensor, elemNum, atomic, atomicOp);
        } else {
            CpGM2GMMTE(inputTensor, outputTensor, elemNum, atomic, atomicOp);
        }
    }

    template<typename T>
    ZBAL_KERNEL void CpGM2GMMTE(AscendC::GlobalTensor<T> inputTensor, AscendC::GlobalTensor<T> outputTensor,
                                uint32_t elemNum, bool atomic, uint32_t atomicOp)
    {
        if (atomic) {
            SetAtomicOpMTE<T>(atomicOp);
        }

        AscendC::DataCopyPadExtParams<T> padParams;
        uint32_t leftCopySize = elemNum * sizeof(T);
        uint32_t times = 0;
        uint32_t preCopyNum = UB_DMA_MAX_SIZE / sizeof(T);
        do {
            uint32_t curCopySize = (leftCopySize > UB_DMA_MAX_SIZE) ? UB_DMA_MAX_SIZE : leftCopySize;
            AscendC::LocalTensor<T> xLocal = bindQueue.AllocTensor<T>();
            AscendC::DataCopyExtParams dataCopyParams(1, curCopySize, 0, 0, 0);
            AscendC::DataCopyPad(xLocal, inputTensor[times * preCopyNum], dataCopyParams, padParams);
            bindQueue.EnQue(xLocal);
            xLocal = bindQueue.DeQue<T>();
            AscendC::DataCopyPad(outputTensor[times * preCopyNum], xLocal, dataCopyParams);
            bindQueue.FreeTensor(xLocal);
            leftCopySize = (leftCopySize > UB_DMA_MAX_SIZE) ? leftCopySize - UB_DMA_MAX_SIZE : 0;
            times++;
        } while (leftCopySize > 0);

        if (atomic) {
            AscendC::SetAtomicNone();
        }

        AscendC::PipeBarrier<PIPE_ALL>();
    }

    template<typename T>
    ZBAL_KERNEL void CpGM2GMSDMA(AscendC::GlobalTensor<T> inputTensor, AscendC::GlobalTensor<T> outputTensor,
                                 uint64_t elemNum, bool atomic, uint32_t atomicOp)
    {
        uint8_t opCode = 0;
        if (atomic) {
            opCode = SetAtomicOpSDMA<T>(atomicOp);
        }

        AscendC::LocalTensor<T> localTensor = bindQueue.AllocTensor<T>();
        zbal_sdma_get_nbi(outputTensor, inputTensor, localTensor, elemNum, EVENT_ID0, opCode);
        zbal_sdma_quiet(localTensor, EVENT_ID0);
        bindQueue.FreeTensor(localTensor);
    }

private:
    template<typename T>
    ZBAL_KERNEL void SetAtomicOpMTE(uint32_t atomicOp)
    {
        switch (atomicOp) {
            case ZBAL_REDUCE_SUM:
                AscendC::SetAtomicAdd<T>();
                break;
            case ZBAL_REDUCE_MAX:
                AscendC::SetAtomicMax<T>();
                break;
            case ZBAL_REDUCE_MIN:
                AscendC::SetAtomicMin<T>();
                break;
            default:
                AscendC::SetAtomicNone();
                break;
        }
    }

    template<typename T>
    ZBAL_KERNEL uint8_t SetAtomicOpSDMA(uint32_t atomicOp)
    {
        uint8_t dataTypeOffset = 4;
        uint8_t reduceOpMask = 0x0f;

        uint8_t dataType = 0;
        uint8_t reduceOp = static_cast<uint8_t>(atomicOp & reduceOpMask);
        if constexpr (std::is_same_v<T, int8_t>) {
            dataType = ZBAL_DATA_TYPE_INT8;
        } else if constexpr (std::is_same_v<T, int16_t>) {
            dataType = ZBAL_DATA_TYPE_INT16;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            dataType = ZBAL_DATA_TYPE_INT32;
        } else if constexpr (std::is_same_v<T, float16_t>) {
            dataType = ZBAL_DATA_TYPE_FP16;
        } else if constexpr (std::is_same_v<T, float32_t>) {
            dataType = ZBAL_DATA_TYPE_FP32;
        } else if constexpr (std::is_same_v<T, bfloat16_t>) {
            dataType = ZBAL_DATA_TYPE_BFP16;
        }
        dataType = dataType << dataTypeOffset;

        return dataType | reduceOp;
    }

protected:
    AscendC::TPipe pipe;
    AscendC::TQueBind<AscendC::TPosition::VECIN, AscendC::TPosition::VECOUT, 1> bindQueue;
    uint32_t dataOpType;
    uint16_t groupSize;
    uint16_t myGroupRank;
    uint64_t memSize;
    __gm__ uint16_t *worldRanks;
    __gm__ CommGroupInfo *comm;
};

#endif