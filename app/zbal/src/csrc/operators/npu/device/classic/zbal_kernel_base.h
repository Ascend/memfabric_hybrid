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

    ZBAL_KERNEL void Init(uint32_t type = ZBAL_DATA_OP_MTE)
    {
        dataOpType = type;
        pipe.InitBuffer(bindQueue, 1, UB_DMA_MAX_SIZE);
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
};

#endif