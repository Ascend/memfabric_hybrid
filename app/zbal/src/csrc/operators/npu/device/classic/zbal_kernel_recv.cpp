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
class ZBALRecvKernel : public ZBALBaseKernel {
public:
    ZBAL_KERNEL ZBALRecvKernel() {}

    ZBAL_KERNEL void Init(GM_ADDR recvBuf, GM_ADDR metaAddr, size_t recvCount, uint32_t peer)
    {
        this->recvBuf = recvBuf;
        this->peer = peer;
        this->elements = recvCount;
        this->comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaAddr);
        this->rank = comm->myGroupRank;
        this->groupSize = comm->groupSize;
        this->addrOffset = groupSize * ZBAL_FLAG_SIZE;

        this->localDeviceMemSize = comm->localDeviceMemSize;
        this->peerGroupRank2WorldRank = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);

        this->aivNum = AscendC::GetBlockNum();
        this->aivIndex = AscendC::GetBlockIdx();

        // |------input------|------flag------|------ack------|
        this->exchangeAddr = reinterpret_cast<__gm__ uint64_t *>(comm->myAddressExchangeGva);
        this->exchangeFlag = exchangeAddr + addrOffset;
        this->exchangeAck = exchangeFlag + addrOffset;

        ZBALBaseKernel::Init(comm->dataOpType);
    }

    ZBAL_KERNEL void Process()
    {
#ifdef __DAV_C220_VEC__
        ZBAL_PROF_START(comm, ZBAL_PROF_RECV_KERNEL_ALL);
        uint64_t waitSymbol = 1024;
        uint32_t numPerCore = elements / aivNum;
        uint32_t offset = aivIndex * numPerCore;
        if (aivIndex == aivNum - 1) {
            numPerCore = elements - (aivNum - 1) * numPerCore;
        }

        ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
        ZBALWaitFlag(exchangeFlag, waitSymbol, peer);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);

        uint64_t sendBuf = ZBALGetFlag(exchangeAddr, peer);
        sendGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(sendBuf) + offset, numPerCore);
        recvGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(recvBuf) + offset, numPerCore);

        CpGM2GM(sendGm, recvGm, numPerCore);
        AscendC::SyncAll<true>();

        if (aivIndex == 0) {
            ZBALSetFlag(exchangeFlag, 0, peer);
            AscendC::PipeBarrier<PIPE_ALL>();

            auto ackPtr = zbal_ptr(exchangeAck, rank, peer, localDeviceMemSize, peerGroupRank2WorldRank);
            ZBALSetFlag(ackPtr, waitSymbol, rank);
            AscendC::PipeBarrier<PIPE_ALL>();
        }

        AscendC::SyncAll<true>();
        ZBAL_PROF_STOP(comm, ZBAL_PROF_RECV_KERNEL_ALL);
#endif
    }

private:
    AscendC::GlobalTensor<T> sendGm;
    AscendC::GlobalTensor<T> recvGm;
    uint32_t aivNum;
    uint32_t aivIndex;
    uint32_t rank;
    uint32_t peer;
    uint32_t groupSize;
    uint32_t elements;
    uint32_t addrOffset;
    uint64_t localDeviceMemSize;
    GM_ADDR recvBuf;
    __gm__ CommGroupInfo *comm;
    __gm__ uint64_t *exchangeAddr;
    __gm__ uint64_t *exchangeFlag;
    __gm__ uint64_t *exchangeAck;
    __gm__ uint16_t *peerGroupRank2WorldRank;
};

extern "C" __global__ __aicore__ void ZBALRecvInner(GM_ADDR recvBuf, size_t recvCount, uint32_t dataTypeNum,
                                                    GM_ADDR metaAddr, uint32_t peer)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);

    auto comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaAddr);
    AscendC::SetSyncBaseAddr(comm->fftsConfig);

    zbal_datatype_t dataType = static_cast<zbal_datatype_t>(dataTypeNum);
    switch (dataType) {
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT8: {
            ZBALRecvKernel<int8_t> op;
            op.Init(recvBuf, metaAddr, recvCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT16: {
            ZBALRecvKernel<int16_t> op;
            op.Init(recvBuf, metaAddr, recvCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT32: {
            ZBALRecvKernel<int32_t> op;
            op.Init(recvBuf, metaAddr, recvCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP16: {
            ZBALRecvKernel<float16_t> op;
            op.Init(recvBuf, metaAddr, recvCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP32: {
            ZBALRecvKernel<float> op;
            op.Init(recvBuf, metaAddr, recvCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT64: {
            ZBALRecvKernel<int64_t> op;
            op.Init(recvBuf, metaAddr, recvCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT64: {
            ZBALRecvKernel<uint64_t> op;
            op.Init(recvBuf, metaAddr, recvCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT8: {
            ZBALRecvKernel<uint8_t> op;
            op.Init(recvBuf, metaAddr, recvCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT16: {
            ZBALRecvKernel<uint16_t> op;
            op.Init(recvBuf, metaAddr, recvCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT32: {
            ZBALRecvKernel<uint32_t> op;
            op.Init(recvBuf, metaAddr, recvCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP64: {
            ZBALRecvKernel<float64_t> op;
            op.Init(recvBuf, metaAddr, recvCount, peer);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_BFP16: {
            ZBALRecvKernel<bfloat16_t> op;
            op.Init(recvBuf, metaAddr, recvCount, peer);
            op.Process();
            break;
        }
        default:
            return;
    }
}

int32_t ZBALOpRecv(const void *recvBuff, size_t recvCount, zbal_datatype_t dataType, uint32_t peer, aclrtStream stream,
                   CommGroupInfo &groupInfo)
{
    uint32_t blockDim = ZBALOpGetAivBlockDim(groupInfo, recvCount, dataType);
    uint32_t dataTypeNum = static_cast<uint32_t>(dataType);
    uint8_t *metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);
    uint8_t *recvBuf = reinterpret_cast<uint8_t *>(const_cast<void *>(recvBuff));

    ZBALRecvInner<<<blockDim, nullptr, stream>>>(recvBuf, recvCount, dataTypeNum, metaAddr, peer);

    return 0;
}