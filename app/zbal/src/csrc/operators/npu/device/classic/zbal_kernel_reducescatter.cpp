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

#include "zbal_kernel_base.h"

struct RsParallelStrategy {
    bool aivNumLtGroupSize{false};
    uint32_t startRank{0};
    uint32_t endRank{0};
    uint32_t startNotifyRank{0};
    uint32_t endNotifyRank{0};
    uint32_t corePerRank{0};
    uint32_t coreRankIdx{0};
};

template<typename T>
class ZBALReduceScatterKernel : public ZBALBaseKernel {
public:
    ZBAL_KERNEL ZBALReduceScatterKernel() {}

    ZBAL_KERNEL void Init(GM_ADDR input, GM_ADDR output, GM_ADDR metaAddr, size_t elements, uint32_t reduceOp,
                          uint64_t flagMagic)
    {
        this->aivNum = AscendC::GetBlockNum();
        this->aivIndex = AscendC::GetBlockIdx();
        this->input = input;
        this->output = output;
        this->comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaAddr);
        this->rank = comm->myGroupRank;
        this->groupSize = comm->groupSize;
        this->reduceOp = reduceOp;
        this->elements = elements;
        this->addrOffset = groupSize * ZBAL_FLAG_SIZE;
        this->flagMagic = flagMagic;
        this->localDeviceMemSize = comm->localDeviceMemSize;
        // |------input------|------flag------|
        this->exchangeAddr = reinterpret_cast<__gm__ uint64_t *>(comm->myAddressExchangeGva);
        this->exchangeFlag = exchangeAddr + addrOffset;
        this->peerGroupRank2WorldRank = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);

        InitParallelStrategy();

        ZBALBaseKernel::Init(comm->dataOpType);
    }

    ZBAL_KERNEL void InitParallelStrategy()
    {
        meta.aivNumLtGroupSize = aivNum < groupSize;
        // 核数小于集群数场景，前remain个核负责avg+1张卡的数据搬运，其他核负责avg张卡的数据搬运
        if (meta.aivNumLtGroupSize) {
            uint16_t avg = groupSize / aivNum;
            uint16_t remain = groupSize % aivNum;
            uint16_t extra = aivIndex < remain;
            meta.startRank = aivIndex * avg + (extra ? aivIndex : remain);
            meta.endRank = meta.startRank + avg + extra;
            meta.startNotifyRank = meta.startRank;
            meta.endNotifyRank = meta.endRank;
        } else {
            // 核数大于集群数场景，前remain个组负责一张卡的数据搬运，每个组avg+1个核并行搬运，其他组avg个核并行搬运一张卡
            uint16_t avg = aivNum / groupSize;
            uint16_t remain = aivNum % groupSize;
            uint16_t extra = avg + 1;

            bool extraPart = aivIndex < remain * extra;
            if (extraPart) {
                meta.corePerRank = extra;
                meta.coreRankIdx = aivIndex % extra;
                meta.startRank = aivIndex / extra;
            } else {
                uint32_t offset = aivIndex - remain * extra;
                meta.corePerRank = avg;
                meta.coreRankIdx = offset % avg;
                meta.startRank = remain + offset / avg;
            }
            meta.endRank = meta.startRank + 1;
            if (aivIndex < groupSize) {
                meta.startNotifyRank = aivIndex;
                meta.endNotifyRank = aivIndex + 1;
            }
        }
    }

    ZBAL_KERNEL void Process()
    {
#ifdef __DAV_C220_VEC__
        ZBAL_PROF_START(comm, ZBAL_PROF_REDUCESCATTER_KERNEL_ALL);

        // step1. 所有核先参与本地搬运
        uint32_t numPerCoreLocal = elements / aivNum;
        uint32_t xOffsetLocal = rank * elements + aivIndex * numPerCoreLocal;
        uint32_t yOffsetLocal = aivIndex * numPerCoreLocal;
        if (aivIndex == aivNum - 1) {
            numPerCoreLocal = elements - (aivNum - 1) * numPerCoreLocal;
        }

        ZBAL_PROF_START(comm, ZBAL_PROF_REDUCESCATTER_LOCAL_COPY);
        inputGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(input) + xOffsetLocal, numPerCoreLocal);
        outputGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(output) + yOffsetLocal, numPerCoreLocal);
        CpGM2GM(inputGm, outputGm, numPerCoreLocal);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_REDUCESCATTER_LOCAL_COPY);
        AscendC::SyncAll<true>();

        // step2. 分核与交换地址
        InitDataAddrAndFlag();

        // step3. 分核与并行拷贝
        uint32_t xOffset = 0;
        uint32_t yOffset = 0;
        uint32_t numPerCore = 0;
        if (meta.aivNumLtGroupSize) {
            numPerCore = elements;
            xOffset = rank * elements;
            yOffset = 0;
        } else {
            numPerCore = elements / meta.corePerRank;
            xOffset = rank * elements + meta.coreRankIdx * numPerCore;
            yOffset = meta.coreRankIdx * numPerCore;
            if (meta.coreRankIdx == meta.corePerRank - 1) {
                numPerCore = elements - (meta.corePerRank - 1) * numPerCore;
            }
        }

        for (auto srcRank = meta.startRank; srcRank < meta.endRank; srcRank++) {
            ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
            ZBALWaitFlag(exchangeFlag, flagMagic, srcRank);
            ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);
            uint64_t inputAddr = ZBALGetFlag(exchangeAddr, srcRank);
            inputGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(inputAddr) + xOffset, numPerCore);
            outputGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(output) + yOffset, numPerCore);
            if (srcRank != rank) {
                ZBAL_PROF_START(comm, ZBAL_PROF_REDUCESCATTER_COPY);
                CpGM2GM(inputGm, outputGm, numPerCore, true, reduceOp);
                ZBAL_PROF_STOP(comm, ZBAL_PROF_REDUCESCATTER_COPY);
            }
        }

        BarrierAll(comm);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_REDUCESCATTER_KERNEL_ALL);
#endif
    }

private:
    ZBAL_KERNEL void InitDataAddrAndFlag()
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_EXCHANGE_ADDR);
        for (auto dsrRank = meta.startNotifyRank; dsrRank < meta.endNotifyRank; dsrRank++) {
            uint64_t dataAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(input));
            auto ptr = zbal_ptr(exchangeAddr, rank, dsrRank, localDeviceMemSize, peerGroupRank2WorldRank);
            auto flagPtr = zbal_ptr(exchangeFlag, rank, dsrRank, localDeviceMemSize, peerGroupRank2WorldRank);
            ZBALSetFlag(ptr, dataAddr, rank);
            AscendC::PipeBarrier<PIPE_ALL>();
            ZBALSetFlag(flagPtr, flagMagic, rank);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_EXCHANGE_ADDR);
    }

private:
    AscendC::GlobalTensor<T> inputGm;
    AscendC::GlobalTensor<T> outputGm;
    uint32_t aivNum;
    uint32_t aivIndex;
    uint32_t rank;
    uint32_t groupSize;
    uint32_t reduceOp;
    uint32_t elements;
    uint32_t addrOffset;
    uint64_t flagMagic;
    uint64_t localDeviceMemSize;
    RsParallelStrategy meta;
    __gm__ void *input;
    __gm__ void *output;
    __gm__ CommGroupInfo *comm;
    __gm__ uint64_t *exchangeAddr;
    __gm__ uint64_t *exchangeFlag;
    __gm__ uint16_t *peerGroupRank2WorldRank;
};

extern "C" __global__ __aicore__ void ZBALReduceScatterInner(GM_ADDR input, GM_ADDR output, size_t recvNumel,
                                                             uint32_t dataType, uint32_t reduceOp, GM_ADDR metaAddr,
                                                             uint64_t flagMagic)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);

    auto comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaAddr);
    AscendC::SetSyncBaseAddr(comm->fftsConfig);

    zbal_datatype_t zbalDataType = static_cast<zbal_datatype_t>(dataType);
    switch (zbalDataType) {
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT8: {
            ZBALReduceScatterKernel<int8_t> op;
            op.Init(input, output, metaAddr, recvNumel, reduceOp, flagMagic);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT16: {
            ZBALReduceScatterKernel<int16_t> op;
            op.Init(input, output, metaAddr, recvNumel, reduceOp, flagMagic);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT32: {
            ZBALReduceScatterKernel<int32_t> op;
            op.Init(input, output, metaAddr, recvNumel, reduceOp, flagMagic);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP32: {
            ZBALReduceScatterKernel<float> op;
            op.Init(input, output, metaAddr, recvNumel, reduceOp, flagMagic);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP16: {
            ZBALReduceScatterKernel<float16_t> op;
            op.Init(input, output, metaAddr, recvNumel, reduceOp, flagMagic);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_BFP16: {
            ZBALReduceScatterKernel<bfloat16_t> op;
            op.Init(input, output, metaAddr, recvNumel, reduceOp, flagMagic);
            op.Process();
            break;
        }
        default:
            return;
    }
}

int32_t ZBALOpReduceScatter(const void *inp, void *out, size_t recvNumel, zbal_datatype_t dataType, aclrtStream stream,
                            zbal_reduce_op_t reduceOp, CommGroupInfo &groupInfo)
{
    /* define the block dim */
    uint32_t blockDim = 32;
    uint32_t dataTypeNum = static_cast<uint32_t>(dataType);
    uint32_t reduceOpNum = static_cast<uint32_t>(reduceOp);

    uint8_t *metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);
    uint8_t *input = reinterpret_cast<uint8_t *>(const_cast<void *>(inp));
    uint8_t *output = reinterpret_cast<uint8_t *>(out);

    uint64_t flagMagic = ++groupInfo.waitSymbol;

    ZBALReduceScatterInner<<<blockDim, nullptr, stream>>>(input, output, recvNumel, dataTypeNum, reduceOpNum, metaAddr,
                                                          flagMagic);

    return 0;
}
