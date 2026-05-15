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
#include <acl/acl_rt.h>
#include "kernel_operator.h"
#include "tiling/platform/platform_ascendc.h"
#include "zbal_kernel_dispatch_low_latency.h"

using namespace AscendC;

extern "C" __global__ __aicore__ void
dispatch_low_latency(uint64_t fftsAddr, GM_ADDR metaAddr, GM_ADDR x, GM_ADDR expertIds, GM_ADDR expandXOut,
                     GM_ADDR dynamicScalesOut, GM_ADDR expandIdxOut, GM_ADDR expertTokenNumsOut,
                     GM_ADDR epSendCountsOut, GM_ADDR putOffset, GM_ADDR putOffsetStatus, uint32_t rank,
                     uint32_t numExperts, uint32_t bs, uint32_t hidden, uint32_t topK, uint32_t quantMode,
                     int64_t magicVal, uint32_t srcDataType, uint32_t dstDataType)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);
    AscendC::SetSyncBaseAddr(fftsAddr);
    AscendC::TPipe pipe;
    if (dstDataType == ZBAL_DATA_TYPE_BFP16 || dstDataType == ZBAL_DATA_TYPE_FP16) {
        if (srcDataType == ZBAL_DATA_TYPE_BFP16 && quantMode == NO_QUANT) {
            MoeDispatchLowLatency::DispatchLowLatency<bfloat16_t, bfloat16_t, false, false, false, false> op;
            op.Init(metaAddr, x, expertIds, expandXOut, dynamicScalesOut, expandIdxOut, expertTokenNumsOut,
                    epSendCountsOut, putOffset, putOffsetStatus, rank, numExperts, bs, hidden, topK, magicVal, &pipe);
            op.Process();
            return;
        } else if (srcDataType == ZBAL_DATA_TYPE_FP16 && quantMode == NO_QUANT) {
            MoeDispatchLowLatency::DispatchLowLatency<float16_t, float16_t, false, false, false, false> op;
            op.Init(metaAddr, x, expertIds, expandXOut, dynamicScalesOut, expandIdxOut, expertTokenNumsOut,
                    epSendCountsOut, putOffset, putOffsetStatus, rank, numExperts, bs, hidden, topK, magicVal, &pipe);
            op.Process();
            return;
        }
    } else if (dstDataType == ZBAL_DATA_TYPE_INT8) {
        if (srcDataType == ZBAL_DATA_TYPE_BFP16 && quantMode == QUANT_BF16_2_INT8) {
            MoeDispatchLowLatency::DispatchLowLatency<bfloat16_t, int8_t, false, true, false, false> op;
            op.Init(metaAddr, x, expertIds, expandXOut, dynamicScalesOut, expandIdxOut, expertTokenNumsOut,
                    epSendCountsOut, putOffset, putOffsetStatus, rank, numExperts, bs, hidden, topK, magicVal, &pipe);
            op.Process();
            return;
        } else if (srcDataType == ZBAL_DATA_TYPE_FP16 && quantMode == QUANT_BF16_2_INT8) {
            MoeDispatchLowLatency::DispatchLowLatency<float16_t, int8_t, false, true, false, false> op;
            op.Init(metaAddr, x, expertIds, expandXOut, dynamicScalesOut, expandIdxOut, expertTokenNumsOut,
                    epSendCountsOut, putOffset, putOffsetStatus, rank, numExperts, bs, hidden, topK, magicVal, &pipe);
            op.Process();
            return;
        }
    }
}

int32_t ZBALOpDispatchLowLatency(const zbal_tensor_info_t *x, const zbal_tensor_info_t *expertIds, int64_t moeExpertNum,
                                 int64_t sharedExpertNum, int64_t sharedExpertRankNum, int64_t quantMode,
                                 int64_t globalBs, int64_t magicVal, int64_t expertTokenNumsType,
                                 const zbal_tensor_info_t *expandXOut, const zbal_tensor_info_t *dynamicScalesOut,
                                 const zbal_tensor_info_t *expandIdxOut, const zbal_tensor_info_t *expertTokenNumsOut,
                                 const zbal_tensor_info_t *epRecvCountsOut, const zbal_tensor_info_t *putOffset,
                                 const zbal_tensor_info_t *putOffsetStatus, aclrtStream stream,
                                 const CommGroupInfo &groupInfo, int64_t flags)
{
    uint32_t blockDim = 0;
    auto ret = aclrtGetResInCurrentThread(ACL_RT_DEV_RES_VECTOR_CORE, &blockDim);
    if (ret != 0) {
        printf("ZBALOpDispatchLowLatency failed as blockDim get failed, blockDim:%d\n", blockDim);
        return ret;
    }
    uint32_t rank = static_cast<uint32_t>(groupInfo.myGroupRank);
    uint32_t numExperts = static_cast<uint32_t>(moeExpertNum);
    // uint32_t magicNum = static_cast<uint32_t>(magicVal);
    uint32_t bs = static_cast<uint32_t>(x->shape[0]);
    uint32_t hidden = static_cast<uint32_t>(x->shape[1]);
    uint32_t topK = static_cast<uint32_t>(expertIds->shape[1]);
    uint64_t fftsAddr = groupInfo.fftsConfig;
    GM_ADDR metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);

    zbal_datatype_t srcDataType = static_cast<zbal_datatype_t>(x->dataType);
    zbal_datatype_t dstDataType = static_cast<zbal_datatype_t>(expandXOut->dataType);

    GM_ADDR xAddr = reinterpret_cast<uint8_t *>(x->data);
    GM_ADDR expertIdsAddr = reinterpret_cast<uint8_t *>(expertIds->data);
    GM_ADDR expandXOutAddr = reinterpret_cast<uint8_t *>(expandXOut->data);
    GM_ADDR dynamicScalesOutAddr = reinterpret_cast<uint8_t *>(dynamicScalesOut->data);
    GM_ADDR expandIdxOutAddr = reinterpret_cast<uint8_t *>(expandIdxOut->data);
    GM_ADDR expertTokenNumsOutAddr = reinterpret_cast<uint8_t *>(expertTokenNumsOut->data);
    GM_ADDR epSendCountsOutAddr = reinterpret_cast<uint8_t *>(epRecvCountsOut->data);
    // temp tensors
    GM_ADDR putOffsetAddr = reinterpret_cast<uint8_t *>(putOffset->data);
    GM_ADDR putOffsetStatusAddr = reinterpret_cast<uint8_t *>(putOffsetStatus->data);

    // launch kernel
    dispatch_low_latency<<<blockDim, nullptr, stream>>>(
        fftsAddr, metaAddr, xAddr, expertIdsAddr, expandXOutAddr, dynamicScalesOutAddr, expandIdxOutAddr,
        expertTokenNumsOutAddr, epSendCountsOutAddr, putOffsetAddr, putOffsetStatusAddr, rank, numExperts, bs, hidden,
        topK, quantMode, magicVal, srcDataType, dstDataType);

    return 0;
}