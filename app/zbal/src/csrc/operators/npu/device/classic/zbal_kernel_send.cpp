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

template<typename T>
class ZBALSendKernel : public ZBALBaseKernel {
public:
    ZBAL_KERNEL ZBALSendKernel() {}

    ZBAL_KERNEL void Init(GM_ADDR sendBuf, GM_ADDR metaAddr, size_t sendCount, uint32_t peer)
    {
        this->sendBuf = sendBuf;
        this->peer = peer;
        this->elements = sendCount;
        this->comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaAddr);
        this->rank = comm->myGroupRank;
        this->myGroupRank = comm->myGroupRank;
        this->groupSize = comm->groupSize;
        this->addrOffset = groupSize * ZBAL_FLAG_SIZE;

        this->memSize = comm->localDeviceMemSize;
        this->worldRanks = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);

        // |------input------|------flag------|------ack------|
        this->exchangeAddr = reinterpret_cast<__gm__ uint64_t *>(comm->myAddressExchangeGva);
        this->exchangeFlag = exchangeAddr + addrOffset;
        this->exchangeAck = exchangeFlag + addrOffset;
    }

    ZBAL_KERNEL void Process()
    {
#ifdef __DAV_C220_VEC__
        ZBAL_PROF_START(comm, ZBAL_PROF_SEND_KERNEL_ALL);
        uint64_t waitSymbol = 1024;
        uint64_t dataAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(sendBuf));
        auto ptr = ZbalPtr(exchangeAddr, peer);
        auto flagPtr = ZbalPtr(exchangeFlag, peer);
        ZBALSetFlag(ptr, dataAddr, rank);
        AscendC::PipeBarrier<PIPE_ALL>();
        ZBALSetFlag(flagPtr, waitSymbol, rank);
        AscendC::PipeBarrier<PIPE_ALL>();

        ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
        ZBALWaitFlag(exchangeAck, waitSymbol, peer);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);

        ZBALSetFlag(exchangeAck, 0, peer);
        AscendC::PipeBarrier<PIPE_ALL>();
        ZBAL_PROF_STOP(comm, ZBAL_PROF_SEND_KERNEL_ALL);
#endif
    }

private:
    uint32_t rank;
    uint32_t peer;
    uint32_t elements;
    uint32_t addrOffset;
    GM_ADDR sendBuf;
    __gm__ uint64_t *exchangeAddr;
    __gm__ uint64_t *exchangeFlag;
    __gm__ uint64_t *exchangeAck;
};

extern "C" __global__ __aicore__ void ZBALSendInner(GM_ADDR sendBuf, size_t sendCount, uint32_t dataTypeNum,
                                                    GM_ADDR metaAddr, uint32_t peer)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);

    auto comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaAddr);
    AscendC::SetSyncBaseAddr(comm->fftsConfig);

    zbal_datatype_t dataType = static_cast<zbal_datatype_t>(dataTypeNum);
    switch (dataType) {
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT8: {
            ZBALSendKernel<int8_t> op;
            op.Init(sendBuf, metaAddr, sendCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT16: {
            ZBALSendKernel<int16_t> op;
            op.Init(sendBuf, metaAddr, sendCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT32: {
            ZBALSendKernel<int32_t> op;
            op.Init(sendBuf, metaAddr, sendCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP16: {
            ZBALSendKernel<float16_t> op;
            op.Init(sendBuf, metaAddr, sendCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP32: {
            ZBALSendKernel<float> op;
            op.Init(sendBuf, metaAddr, sendCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT64: {
            ZBALSendKernel<int64_t> op;
            op.Init(sendBuf, metaAddr, sendCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT64: {
            ZBALSendKernel<uint64_t> op;
            op.Init(sendBuf, metaAddr, sendCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT8: {
            ZBALSendKernel<uint8_t> op;
            op.Init(sendBuf, metaAddr, sendCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT16: {
            ZBALSendKernel<uint16_t> op;
            op.Init(sendBuf, metaAddr, sendCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT32: {
            ZBALSendKernel<uint32_t> op;
            op.Init(sendBuf, metaAddr, sendCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP64: {
            ZBALSendKernel<float64_t> op;
            op.Init(sendBuf, metaAddr, sendCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_BFP16: {
            ZBALSendKernel<bfloat16_t> op;
            op.Init(sendBuf, metaAddr, sendCount, peer);
            op.Process();
            break;
        }
        default:
            return;
    }
}

int32_t ZBALOpSend(const void *sendBuff, size_t sendCount, zbal_datatype_t dataType, uint32_t peer, aclrtStream stream,
                   CommGroupInfo &groupInfo)
{
    uint32_t blockDim = 1;
    uint32_t dataTypeNum = static_cast<uint32_t>(dataType);
    uint8_t *metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);
    uint8_t *sendBuf = reinterpret_cast<uint8_t *>(const_cast<void *>(sendBuff));

    ZBALSendInner<<<blockDim, nullptr, stream>>>(sendBuf, sendCount, dataTypeNum, metaAddr, peer);

    return 0;
}