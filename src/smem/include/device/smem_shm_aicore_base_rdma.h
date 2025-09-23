/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef __MEMFABRIC_SMEM_AI_CORE_BASE_RDMA_H__
#define __MEMFABRIC_SMEM_AI_CORE_BASE_RDMA_H__

#include "smem_shm_aicore_base_define.h"
#include "smem_shm_aicore_base_meta.h"

constexpr uint64_t DATA_CACHE_LINE_SIZE = 64;
constexpr uint32_t NUM_CQE_PER_POLL_CQ = 100;

SMEM_SHM_INLINE_AICORE void cacheWriteThrough(__gm__ uint8_t* sourceAddr, uint64_t length) {
    __gm__ uint8_t* start = (__gm__ uint8_t*)((uint64_t)sourceAddr / DATA_CACHE_LINE_SIZE * DATA_CACHE_LINE_SIZE);
    __gm__ uint8_t* end =
        (__gm__ uint8_t*)(
            ((uint64_t)sourceAddr + length) / DATA_CACHE_LINE_SIZE * DATA_CACHE_LINE_SIZE
        );
    AscendC::GlobalTensor<uint8_t> global;
    global.SetGlobalBuffer(start);
    for (uint64_t i = 0; i <= end - start; i+= DATA_CACHE_LINE_SIZE) {
        AscendC::DataCacheCleanAndInvalid<uint8_t,
            AscendC::CacheLine::SINGLE_CACHE_LINE, AscendC::DcciDst::CACHELINE_OUT>(global[i]);
    }
}

enum class OPCODE : uint32_t {
    OP_SEND = 0,
    OP_SEND_WITH_INV,
    OP_SEND_WITH_IMM,
    OP_RDMA_WRITE,
    OP_RDMA_WRITE_WITH_IMM,
    OP_RDMA_READ
};

struct AIVRDMAInfo {
    uint32_t qpNum; // number of QP per connection
    uint64_t sqPtr; // pointer to send queue address array of size [PE_NUM][qpNum]
    uint64_t rqPtr; // pointer to receive queue address array of size [PE_NUM][qpNum]
    uint64_t scqPtr; // pointer to send completion queue address array of size [PE_NUM][qpNum]
    uint64_t rcqPtr; // pointer to receive completion queue address array of size [PE_NUM][qpNum]
    uint64_t memPtr; // pointer to memory region array of size [MAX_PE_NUM]
};

struct memInfo {
    uint64_t size; // size of the memory region
    uint64_t addr; // start address of the memory region
    uint32_t lkey; // local key of the memory region
    uint32_t rkey; // remote key of the memory region
};

enum class DBMode : int32_t { INVALID_DB = -1, HW_DB = 0, SW_DB };

struct WQCtx {
    uint32_t wqn; // work queue number
    uint64_t bufAddr; // start address of ring buffer
    uint32_t wqeSize; // size of each WQE
    uint32_t depth; // depth of ring buffer
    uint64_t headAddr; // work queue head (Producer Index) address
    uint64_t tailAddr; // work queue tail (Consumer Index) address
    DBMode dbMode;
    uint64_t dbAddr; // doorbell address
    uint32_t sl; // service level
};

struct CQCtx {
    uint32_t cqn; // completion queue number
    uint64_t bufAddr; // start address of ring buffer
    uint32_t cqeSize; // size of each CQE
    uint32_t depth; // depth of ring buffer
    uint64_t headAddr; // work queue head (Producer Index) address
    uint64_t tailAddr; // work queue tail (Consumer Index) address
    DBMode dbMode;
    uint64_t dbAddr; // doorbell address
};

struct wqeCtx {
    uint32_t byte4;
    uint32_t msgLen;
    uint32_t immtdata;
    uint32_t byte16;
    uint32_t byte20;
    uint32_t rkey;
    uint64_t va;
};

struct segCtx {
    uint32_t len;
    uint32_t lkey;
    uint64_t addr;
};

struct cqeCtx {
    uint32_t byte4;
    uint32_t immtdata;
    uint32_t byte12;
    uint32_t byte16;
    uint32_t byteCnt;
    uint32_t smac;
    uint32_t byte28;
    uint32_t byte32;
};

struct HybmDeviceMeta {
    uint32_t entityId;
    uint32_t rankId;
    uint32_t rankSize;
    uint32_t extraContextSize;
    uint64_t symmetricSize;
    uint64_t qpInfoAddress;
    uint64_t reserved[12];  // total 128B, equal HYBM_DEVICE_PRE_META_SIZE
};

/**
 * @brief RDMA Poll Completion Queue (CQ) function. Return status: 0 means success, non-zero means error.
 *
 * @param remoteRankId           [in] destination rank ID
 * @param qpIdx                  [in] QP index in multi-QP scenario (default 0 for single QP)
 * @param idx                    [in] expect completion queue consumer index after polling
 * @param ubLocal64              [in] temporary UB local tensor of uint64_t used as workspace
 * @param ubLocal32              [in] temporary UB local tensor of uint32_t used as workspace
 */

SMEM_SHM_INLINE_AICORE uint32_t smem_shm_roce_poll_cq(uint32_t remoteRankId, uint32_t qpIdx, uint32_t idx,
                                                      AscendC::LocalTensor<uint64_t> ubLocal64,
                                                      AscendC::LocalTensor<uint32_t> ubLocal32)
{
    __gm__ HybmDeviceMeta* metaPtr = (__gm__ HybmDeviceMeta*)(SMEM_SHM_DEVICE_META_ADDR +
                                                                SMEM_SHM_DEVICE_GLOBAL_META_SIZE);
    __gm__ AIVRDMAInfo* RDMAInfo = (__gm__ AIVRDMAInfo*)(metaPtr->qpInfoAddress);
    uint32_t qpNum = RDMAInfo->qpNum;
    __gm__ CQCtx* cqCtxEntry = (__gm__ CQCtx*)(RDMAInfo->scqPtr + (remoteRankId * qpNum + qpIdx) * sizeof(CQCtx));
    auto cqBaseAddr = cqCtxEntry->bufAddr;
    auto cqeSize = cqCtxEntry->cqeSize;
    auto depth = cqCtxEntry->depth;
    auto curHardwareTailAddr = cqCtxEntry->tailAddr;
    cacheWriteThrough((__gm__ uint8_t*)curHardwareTailAddr, 8);
    uint32_t curTail = *(__gm__ uint32_t*)(curHardwareTailAddr);

    AscendC::DataCopyExtParams copyParamsTail{1, 1 * sizeof(uint32_t), 0, 0, 0};
    const uint8_t bitShift = 7U;
    while (curTail != idx) {
        __gm__ cqeCtx* cqeAddr = (__gm__ cqeCtx*)(cqBaseAddr + cqeSize * (curTail & (depth - 1)));
        uint32_t cqeByte4 = *(__gm__ uint32_t*)cqeAddr;
        while (((cqeByte4 & (1 << bitShift)) != 0) == ((curTail & depth) != 0)) {
            int64_t tmp = AscendC::GetSystemCycle();
            cacheWriteThrough((__gm__ uint8_t*)cqeAddr, 32);
            cqeByte4 = *(__gm__ uint32_t*)cqeAddr;
        }
        curTail++;
        uint32_t wqn = cqeAddr->byte16 & 0xFFFFFF;
        
        // Check CQE status
        uint32_t status = (cqeAddr->byte4 >> 8) & 0xFF;
        if (status) {
            return status;
        }
    }

    // Update CQ tail
    ubLocal32.SetValue(0, (uint32_t)curTail);
    AscendC::GlobalTensor<uint32_t> TailGlobalTensor;
    TailGlobalTensor.SetGlobalBuffer((__gm__ uint32_t*)curHardwareTailAddr);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopyPad(TailGlobalTensor, ubLocal32, copyParamsTail);
    AscendC::PipeBarrier<PIPE_ALL>();
    cacheWriteThrough((__gm__ uint8_t*)curHardwareTailAddr, 8);

    // Ring CQ Doorbell
    auto cqDBAddr = cqCtxEntry->dbAddr;
    if (cqCtxEntry->dbMode == DBMode::SW_DB) {
        ubLocal32.SetValue(0, (uint32_t)(curTail & 0xFFFFFF));
        AscendC::GlobalTensor<uint32_t> CQDBGlobalTensor;
        CQDBGlobalTensor.SetGlobalBuffer((__gm__ uint32_t*)cqDBAddr);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopyPad(CQDBGlobalTensor, ubLocal32, copyParamsTail);
        AscendC::PipeBarrier<PIPE_ALL>();
        cacheWriteThrough((__gm__ uint8_t*)cqDBAddr, 8);
    } else if (cqCtxEntry->dbMode == DBMode::HW_DB) {
        uint64_t doorBellInfo = 0;
        doorBellInfo |= cqCtxEntry->cqn; // [0:23] DB_TAG = qp_num
        doorBellInfo |= 3 << 24; // [24:27] DB_CMD = HNS_ROCE_V2_CQ_DB_PTR(3)
        doorBellInfo |= (uint64_t)(curTail & 0xFFFFFF) << 32; // [32:55] DB_CQ_CI = cq.tail
        doorBellInfo |= (uint64_t)1 << 56; // [56:56] DB_CQ_CMD_SN = 1
        ubLocal64.SetValue(0, doorBellInfo);
        AscendC::GlobalTensor<uint64_t> DBGlobalTensor;
        DBGlobalTensor.SetGlobalBuffer((__gm__ uint64_t*)cqDBAddr);
        AscendC::DataCopyExtParams copyParams{1, 1 * sizeof(uint64_t), 0, 0, 0};
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopyPad(DBGlobalTensor, ubLocal64, copyParams);
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    // Update WQ tail
    __gm__ WQCtx* wqCtxEntry = (__gm__ WQCtx*)(RDMAInfo->sqPtr + (remoteRankId * qpNum + qpIdx) * sizeof(WQCtx));
    auto curWQTailAddr = wqCtxEntry->tailAddr;
    cacheWriteThrough((__gm__ uint8_t*)curWQTailAddr, 8);
    uint32_t curWQTail = *(__gm__ uint32_t*)(curWQTailAddr);
    ubLocal32.SetValue(0, curTail);
    AscendC::GlobalTensor<uint32_t> WQTailGlobalTensor;
    WQTailGlobalTensor.SetGlobalBuffer((__gm__ uint32_t*)curWQTailAddr);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopyPad(WQTailGlobalTensor, ubLocal32, copyParamsTail);
    AscendC::PipeBarrier<PIPE_ALL>();
    cacheWriteThrough((__gm__ uint8_t*)curWQTailAddr, 8);
    return 0;
}

/**
 * @brief AIV direct RDMA helper function for post send, prepare WQE and ring doorbell.
 *
 * @param remoteAddr             [in] address in remote HBM
 * @param localAddr              [in] address in lcoal HBM
 * @param destRankId             [in] destination rank ID
 * @param qpIdx                  [in] QP index in multi-QP scenario (default 0 for single QP)
 * @param opcode                 [in] rdma opcode in OPCODE enum class
 * @param messageLen             [in] message length in Bytes
 * @param ubLocal64              [in] temporary UB local tensor of uint64_t used as workspace
 * @param ubLocal32              [in] temporary UB local tensor of uint32_t used as workspace
 */

SMEM_SHM_INLINE_AICORE void smem_shm_rdma_post_send(__gm__ uint8_t* remoteAddr, __gm__ uint8_t* localAddr,
                                                    uint32_t destRankId, uint32_t qpIdx,
                                                    OPCODE opcode, uint64_t messageLen,
                                                    AscendC::LocalTensor<uint64_t> ubLocal64,
                                                    AscendC::LocalTensor<uint32_t> ubLocal32)
{
    __gm__ HybmDeviceMeta* metaPtr = (__gm__ HybmDeviceMeta*)(SMEM_SHM_DEVICE_META_ADDR +
                                                                SMEM_SHM_DEVICE_GLOBAL_META_SIZE);
    __gm__ AIVRDMAInfo* RDMAInfo = (__gm__ AIVRDMAInfo*)(metaPtr->qpInfoAddress);
    uint32_t qpNum = RDMAInfo->qpNum;
    __gm__ WQCtx* qpCtxEntry = (__gm__ WQCtx*)(RDMAInfo->sqPtr + (destRankId * qpNum + qpIdx) * sizeof(WQCtx));
    auto memInfoTable = RDMAInfo->memPtr;
    auto sqBaseAddr = qpCtxEntry->bufAddr;
    auto wqeSize = qpCtxEntry->wqeSize;
    auto curHardwareHeadAddr = qpCtxEntry->headAddr;
    cacheWriteThrough((__gm__ uint8_t*)curHardwareHeadAddr, 8);
    uint32_t curHead = *(__gm__ uint32_t*)(curHardwareHeadAddr);
    auto curHardwareTailAddr = qpCtxEntry->tailAddr;
    auto depth = qpCtxEntry->depth;
    auto shift = 13;
    AscendC::PipeBarrier<PIPE_ALL>();

    // Poll CQ if send queue is full
    cacheWriteThrough((__gm__ uint8_t*)curHardwareTailAddr, 8);
    if ((curHead + 10) % depth == (*(__gm__ uint32_t*)(curHardwareTailAddr)) % depth) {
        smem_shm_roce_poll_cq(destRankId, qpIdx, *(__gm__ uint32_t*)(curHardwareTailAddr) +
            NUM_CQE_PER_POLL_CQ, ubLocal64, ubLocal32);
    }

    // Write WQE to HBM
    __gm__ uint8_t* wqeAddr = (__gm__ uint8_t*)(sqBaseAddr + wqeSize * (curHead % depth));
    uint64_t ownBit = (curHead >> shift) & 0x1;
    uint32_t byte4 = (uint32_t)opcode & 0x1F;       // [0:4] opcode
    byte4 |= ((~ownBit) << 7) & (1 << 7); // [7] owner_bit
    byte4 |= 1 << 8;                      // [8] IBV_SEND_SINGNALED
    *(__gm__ uint32_t*)(wqeAddr) = byte4; // control set by local parameter, see above lines
    *(__gm__ uint32_t*)(wqeAddr + 4) = messageLen; // message size in bytes
    *(__gm__ uint32_t*)(wqeAddr + 8) = 0; // immtdata is always 0 till we provide poll CQ flow in AIV
    *(__gm__ uint32_t*)(wqeAddr + 12) = 1 << 24; // [120:127] num_sge = 1
    *(__gm__ uint32_t*)(wqeAddr + 16) = 0; // [128:151] start_sge_index = 0
    __gm__ memInfo* remoteMemInfo = (__gm__ memInfo*)(memInfoTable + sizeof(memInfo) * destRankId);
    *(__gm__ uint32_t*)(wqeAddr + 20) = remoteMemInfo->rkey; // rkey
    *(__gm__ uint64_t*)(wqeAddr + 24) = (uint64_t)remoteAddr; // remote VA

    // Write SGE to HBM
    __gm__ uint8_t* sgeAddr = wqeAddr + sizeof(wqeCtx);
    *(__gm__ uint32_t*)(sgeAddr) = messageLen; // message size in bytes
    __gm__ memInfo* localMemInfo = (__gm__ memInfo*)(memInfoTable + sizeof(memInfo) * smem_shm_get_global_rank(0));
    *(__gm__ uint32_t*)(sgeAddr + 4) = localMemInfo->lkey; // lkey
    *(__gm__ uint64_t*)(sgeAddr + 8) = (uint64_t)localAddr; // local VA

    // WQE & SGE cache flush
    cacheWriteThrough(wqeAddr, sizeof(wqeCtx) + sizeof(segCtx));
    AscendC::PipeBarrier<PIPE_ALL>();
    curHead++;

    uint64_t doorBellInfo = 0;
    doorBellInfo |= qpCtxEntry->wqn; // [0:23] DB_TAG = qp_num
    doorBellInfo |= 0 << 24; // [24:27] DB_CMD = HNS_ROCE_V2_SQ_DB(0)
    doorBellInfo |= ((uint64_t)curHead % 65536) << 32; // [32:47] DB_PI = sq.head
    doorBellInfo |= (uint64_t)(qpCtxEntry->sl) << 48; // [48:50] DB_SL = qp.sl

    __gm__ uint64_t* doorBellAddr = (__gm__ uint64_t*)(qpCtxEntry->dbAddr);
    AscendC::PipeBarrier<PIPE_ALL>();

    ubLocal64.SetValue(0, doorBellInfo);
    AscendC::GlobalTensor<uint64_t> DBGlobalTensor;
    DBGlobalTensor.SetGlobalBuffer(doorBellAddr);
    AscendC::DataCopyExtParams copyParams{1, 1 * sizeof(uint64_t), 0, 0, 0};
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopyPad(DBGlobalTensor, ubLocal64, copyParams);
    AscendC::PipeBarrier<PIPE_ALL>();

    ubLocal32.SetValue(0, (uint32_t)curHead);
    AscendC::GlobalTensor<uint32_t> HeadGlobalTensor;
    HeadGlobalTensor.SetGlobalBuffer((__gm__ uint32_t*)curHardwareHeadAddr);
    AscendC::DataCopyExtParams copyParamsHead{1, 1 * sizeof(uint32_t), 0, 0, 0};
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopyPad(HeadGlobalTensor, ubLocal32, copyParamsHead);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * @brief Asynchronous RDMA Write function.
 *
 * @param srcDmaAddr             [in] source address in local HBM
 * @param destDmaAddr            [in] destination address in remote HBM
 * @param destRankId             [in] destination rank ID
 * @param qpIdx                  [in] QP index in multi-QP scenario (default 0 for single QP)
 * @param messageLen             [in] message length in Bytes
 * @param ubLocal64              [in] temporary UB local tensor of uint64_t used as workspace
 * @param ubLocal32              [in] temporary UB local tensor of uint32_t used as workspace
 */

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_roce_write(__gm__ T* srcDmaAddr, __gm__ T* destDmaAddr, uint32_t destRankId,
                                                uint32_t qpIdx, uint64_t messageLen,
                                                AscendC::LocalTensor<uint64_t> ubLocal64,
                                                AscendC::LocalTensor<uint32_t> ubLocal32)
{
    smem_shm_rdma_post_send(destDmaAddr, srcDmaAddr, destRankId, qpIdx, OPCODE::OP_RDMA_WRITE,
                            messageLen, ubLocal64, ubLocal32);
}

/**
 * @brief Asynchronous RDMA READ function.
 *
 * @param srcDmaAddr             [in] source address in remote HBM
 * @param destDmaAddr            [in] destination address in local HBM
 * @param srcRankId              [in] destination rank ID
 * @param qpIdx                  [in] QP index in multi-QP scenario (default 0 for single QP)
 * @param messageLen             [in] message length in Bytes
 * @param ubLocal64              [in] temporary UB local tensor of uint64_t used as workspace
 * @param ubLocal32              [in] temporary UB local tensor of uint32_t used as workspace
 */

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_roce_read(__gm__ T* srcDmaAddr, __gm__ T* destDmaAddr, uint32_t srcRankId,
                                                uint32_t qpIdx, uint64_t messageLen,
                                                AscendC::LocalTensor<uint64_t> ubLocal64,
                                                AscendC::LocalTensor<uint32_t> ubLocal32)
{
    smem_shm_rdma_post_send(srcDmaAddr, destDmaAddr, srcRankId, qpIdx, OPCODE::OP_RDMA_READ,
                            messageLen, ubLocal64, ubLocal32);
}

/**
 * @brief RDMA Quiet function. This synchronous function ensures all previous RDMA WQEs are
 *        completed (data has arrived at the destination NIC).
 *
 * @param remoteRankId           [in] destination rank ID
 * @param qpIdx                  [in] QP index in multi-QP scenario (default 0 for single QP)
 * @param ubLocal64              [in] temporary UB local tensor of uint64_t used as workspace
 * @param ubLocal32              [in] temporary UB local tensor of uint32_t used as workspace
 */

SMEM_SHM_INLINE_AICORE void smem_shm_roce_quiet(uint32_t remoteRankId, uint32_t qpIdx,
                                                AscendC::LocalTensor<uint64_t> ubLocal64,
                                                AscendC::LocalTensor<uint32_t> ubLocal32)
{
    __gm__ HybmDeviceMeta* metaPtr = (__gm__ HybmDeviceMeta*)(SMEM_SHM_DEVICE_META_ADDR +
                                                                SMEM_SHM_DEVICE_GLOBAL_META_SIZE);
    __gm__ AIVRDMAInfo* RDMAInfo = (__gm__ AIVRDMAInfo*)(metaPtr->qpInfoAddress);
    uint32_t qpNum = RDMAInfo->qpNum;
    __gm__ WQCtx* qpCtxEntry = (__gm__ WQCtx*)(RDMAInfo->sqPtr + (remoteRankId * qpNum + qpIdx) * sizeof(WQCtx));
    auto curHardwareHeadAddr = qpCtxEntry->headAddr;
    cacheWriteThrough((__gm__ uint8_t*)curHardwareHeadAddr, 8);
    uint32_t curHead = *(__gm__ uint32_t*)(curHardwareHeadAddr);
    smem_shm_roce_poll_cq(remoteRankId, qpIdx, curHead, ubLocal64, ubLocal32);
}

SMEM_SHM_INLINE_AICORE void smem_shm_roce_qpinfo_test(__gm__ uint8_t* gva, uint32_t destRankId, uint32_t qpIdx)
{
    __gm__ HybmDeviceMeta* metaPtr = (__gm__ HybmDeviceMeta*)(SMEM_SHM_DEVICE_META_ADDR +
                                                                SMEM_SHM_DEVICE_GLOBAL_META_SIZE);
    __gm__ AIVRDMAInfo* RDMAInfo = (__gm__ AIVRDMAInfo*)(metaPtr->qpInfoAddress);
    *(__gm__ uint64_t*)(gva) = (uint64_t)RDMAInfo;
    uint32_t qpNum = RDMAInfo->qpNum;
    *(__gm__ uint64_t*)(gva + 8) = (uint64_t)qpNum;
    __gm__ WQCtx* qpCtxEntry = (__gm__ WQCtx*)(RDMAInfo->sqPtr + (destRankId * qpNum + qpIdx) * sizeof(WQCtx));
    *(__gm__ uint64_t*)(gva + 16) = (uint64_t)qpCtxEntry;
    auto memInfoTable = RDMAInfo->memPtr;
    *(__gm__ uint64_t*)(gva + 24) = (uint64_t)memInfoTable;
    auto sqBaseAddr = qpCtxEntry->bufAddr;
    *(__gm__ uint64_t*)(gva + 32) = (uint64_t)sqBaseAddr;
    auto wqeSize = qpCtxEntry->wqeSize;
    *(__gm__ uint64_t*)(gva + 40) = (uint64_t)wqeSize;
    auto curHardwareHeadAddr = qpCtxEntry->headAddr;
    *(__gm__ uint64_t*)(gva + 48) = (uint64_t)curHardwareHeadAddr;
    cacheWriteThrough((__gm__ uint8_t*)curHardwareHeadAddr, 8);
    uint32_t curHead = *(__gm__ uint32_t*)(curHardwareHeadAddr);
    *(__gm__ uint64_t*)(gva + 56) = (uint64_t)curHead;
    auto curHardwareTailAddr = qpCtxEntry->tailAddr;
    *(__gm__ uint64_t*)(gva + 64) = (uint64_t)curHardwareTailAddr;
    auto depth = qpCtxEntry->depth;
    *(__gm__ uint64_t*)(gva + 72) = (uint64_t)depth;
    *(__gm__ uint64_t*)(gva + 80) = (uint64_t)(qpCtxEntry->sl);
    auto shift = 15;
    AscendC::PipeBarrier<PIPE_ALL>();

    // Write WQE to HBM
    __gm__ uint8_t* wqeAddr = (__gm__ uint8_t*)(sqBaseAddr + wqeSize * (curHead % depth));
    __gm__ memInfo* remoteMemInfo = (__gm__ memInfo*)(memInfoTable + sizeof(memInfo) * destRankId);
    *(__gm__ uint64_t*)(gva + 88) = (uint64_t)(remoteMemInfo->rkey);

    // Write SGE to HBM
    __gm__ memInfo* localMemInfo = (__gm__ memInfo*)(memInfoTable + sizeof(memInfo) * smem_shm_get_global_rank(0));
    *(__gm__ uint64_t*)(gva + 96) = (uint64_t)(localMemInfo->lkey);; // lkey

    __gm__ uint64_t* doorBellAddr = (__gm__ uint64_t*)(qpCtxEntry->dbAddr);
    *(__gm__ uint64_t*)(gva + 104) = (uint64_t)doorBellAddr;
    *(__gm__ uint64_t*)(gva + 112) = (uint64_t)gva;
    AscendC::PipeBarrier<PIPE_ALL>();
}

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_roce_pollcq_test(__gm__ T* srcDmaAddr, __gm__ T* destDmaAddr, uint32_t destRankId,
                                                    uint32_t qpIdx, uint64_t messageLen,
                                                    AscendC::LocalTensor<uint64_t> ubLocal64,
                                                    AscendC::LocalTensor<uint32_t> ubLocal32, __gm__ uint8_t* gva)
{
    smem_shm_rdma_post_send(destDmaAddr, srcDmaAddr, destRankId, qpIdx, OPCODE::OP_RDMA_WRITE,
                            messageLen, ubLocal64, ubLocal32);
    uint32_t idx = 1;
    __gm__ HybmDeviceMeta* metaPtr = (__gm__ HybmDeviceMeta*)(SMEM_SHM_DEVICE_META_ADDR +
                                                                SMEM_SHM_DEVICE_GLOBAL_META_SIZE);
    __gm__ AIVRDMAInfo* RDMAInfo = (__gm__ AIVRDMAInfo*)(metaPtr->qpInfoAddress);
    uint32_t qpNum = RDMAInfo->qpNum;
    __gm__ CQCtx* cqCtxEntry = (__gm__ CQCtx*)(RDMAInfo->scqPtr + (destRankId * qpNum + qpIdx) * sizeof(CQCtx));
    *(__gm__ uint64_t*)(gva) = (uint64_t)cqCtxEntry;
    auto cqBaseAddr = cqCtxEntry->bufAddr;
    auto cqeSize = cqCtxEntry->cqeSize;
    auto depth = cqCtxEntry->depth;
    *(__gm__ uint64_t*)(gva + 8) = (uint64_t)cqBaseAddr;
    *(__gm__ uint64_t*)(gva + 16) = (uint64_t)cqeSize;
    *(__gm__ uint64_t*)(gva + 24) = (uint64_t)depth;
    auto curHardwareTailAddr = cqCtxEntry->tailAddr;
    *(__gm__ uint64_t*)(gva + 32) = (uint64_t)curHardwareTailAddr;
    cacheWriteThrough((__gm__ uint8_t*)curHardwareTailAddr, 8);
    uint32_t curTail = *(__gm__ uint32_t*)(curHardwareTailAddr);
    *(__gm__ uint64_t*)(gva + 40) = (uint64_t)curTail;

    AscendC::DataCopyExtParams copyParamsTail{1, 1 * sizeof(uint32_t), 0, 0, 0};
    
    __gm__ cqeCtx* cqeAddr = (__gm__ cqeCtx*)(cqBaseAddr + cqeSize * (curTail & (depth - 1)));
    uint32_t cqeByte4 = *(__gm__ uint32_t*)cqeAddr;
    while (!(cqeByte4 & (1 << 7))) {
        int64_t tmp = AscendC::GetSystemCycle();
        cacheWriteThrough((__gm__ uint8_t*)cqeAddr, 32);
        cqeByte4 = *(__gm__ uint32_t*)cqeAddr;
    }
    *(__gm__ uint64_t*)(gva + 56) = (uint64_t)(cqeAddr->byte4);
    *(__gm__ uint64_t*)(gva + 64) = (uint64_t)(cqeAddr->immtdata);
    *(__gm__ uint64_t*)(gva + 72) = (uint64_t)(cqeAddr->byte12);
    *(__gm__ uint64_t*)(gva + 80) = (uint64_t)(cqeAddr->byte16);
    *(__gm__ uint64_t*)(gva + 88) = (uint64_t)(cqeAddr->byteCnt);
    *(__gm__ uint64_t*)(gva + 96) = (uint64_t)(cqeAddr->smac);
    curTail++;
    // Process each CQE, and update WQ tail
    uint32_t wqn = cqeAddr->byte16 & 0xFFFFFF;
    __gm__ WQCtx* wqCtxEntry = (__gm__ WQCtx*)(RDMAInfo->sqPtr + (destRankId * qpNum + qpIdx) * sizeof(WQCtx));
    *(__gm__ uint64_t*)(gva + 104) = (uint64_t)(wqCtxEntry->wqn == wqn);
    auto curWQTailAddr = wqCtxEntry->tailAddr;
    cacheWriteThrough((__gm__ uint8_t*)curWQTailAddr, 8);
    uint32_t curWQTail = *(__gm__ uint32_t*)(curWQTailAddr);
    ubLocal32.SetValue(0, curWQTail + 1);
    AscendC::GlobalTensor<uint32_t> WQTailGlobalTensor;
    WQTailGlobalTensor.SetGlobalBuffer((__gm__ uint32_t*)curWQTailAddr);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopyPad(WQTailGlobalTensor, ubLocal32, copyParamsTail);
    AscendC::PipeBarrier<PIPE_ALL>();
    cacheWriteThrough((__gm__ uint8_t*)curWQTailAddr, 8);
    
    // Check CQE status
    uint32_t status = (cqeAddr->byte4 >> 8) & 0xFF;
    *(__gm__ uint64_t*)(gva + 112) = status;
    if (status) {
        return;
    }

    // Update tail
    ubLocal32.SetValue(0, (uint32_t)curTail);
    AscendC::GlobalTensor<uint32_t> TailGlobalTensor;
    TailGlobalTensor.SetGlobalBuffer((__gm__ uint32_t*)curHardwareTailAddr);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopyPad(TailGlobalTensor, ubLocal32, copyParamsTail);
    AscendC::PipeBarrier<PIPE_ALL>();
    cacheWriteThrough((__gm__ uint8_t*)curHardwareTailAddr, 8);

    // Ring CQ Doorbell
    auto cqDBAddr = cqCtxEntry->dbAddr;
    ubLocal32.SetValue(0, (uint32_t)(curTail & 0xFFFFFF));
    AscendC::GlobalTensor<uint32_t> CQDBGlobalTensor;
    CQDBGlobalTensor.SetGlobalBuffer((__gm__ uint32_t*)cqDBAddr);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopyPad(CQDBGlobalTensor, ubLocal32, copyParamsTail);
    AscendC::PipeBarrier<PIPE_ALL>();
    cacheWriteThrough((__gm__ uint8_t*)cqDBAddr, 8);
}

#endif // __MEMFABRIC_SMEM_AI_CORE_BASE_RDMA_H__