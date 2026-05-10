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
#include <cstdio>
#include "zbal_kernel_dispatch_normal.h"

using namespace AscendC;

extern "C" __global__ __aicore__
void dispatch_normal(uint64_t fftsAddr, GM_ADDR metaAddr, GM_ADDR srcTokens, GM_ADDR topkIndex, GM_ADDR sendTokensIndex,
                     GM_ADDR putOffset, GM_ADDR balanceMatrix, uint32_t rank, uint32_t numExperts, uint32_t bs,
                     uint32_t hidden, uint32_t topK, uint32_t quantMode, bool enableBalance,
                     GM_ADDR destTokens, GM_ADDR destScale, uint32_t srcDataType, uint32_t dstDataType)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);
    AscendC::SetSyncBaseAddr(fftsAddr);
    AscendC::TPipe pipe;
    if (dstDataType == ZBAL_DATA_TYPE_BFP16 || dstDataType == ZBAL_DATA_TYPE_FP16) {
        if (srcDataType == ZBAL_DATA_TYPE_BFP16 && quantMode == NO_QUANT) {
            MoeDispatchNormal::DispatchNormal<bfloat16_t, bfloat16_t, false> op;
            op.Init(metaAddr, srcTokens, topkIndex, sendTokensIndex, putOffset, balanceMatrix, rank,
                numExperts, bs, hidden, topK, enableBalance, destTokens, destScale, &pipe);
            op.Process();
            return;
        } else if (srcDataType == ZBAL_DATA_TYPE_FP16 && quantMode == NO_QUANT) {
            MoeDispatchNormal::DispatchNormal<float16_t, float16_t, false> op;
            op.Init(metaAddr, srcTokens, topkIndex, sendTokensIndex, putOffset, balanceMatrix, rank,
                numExperts, bs, hidden, topK, enableBalance, destTokens, destScale, &pipe);
            op.Process();
            return;
        }
    } else if (dstDataType == ZBAL_DATA_TYPE_INT8) { // QUANT_BF16_2_INT8
        if (srcDataType == ZBAL_DATA_TYPE_BFP16 && quantMode == QUANT_BF16_2_INT8) {
            MoeDispatchNormal::DispatchNormal<bfloat16_t, int8_t, true> op;
            op.Init(metaAddr, srcTokens, topkIndex, sendTokensIndex, putOffset, balanceMatrix, rank,
                numExperts, bs, hidden, topK, enableBalance, destTokens, destScale, &pipe);
            op.Process();
            return;
        } else if (srcDataType == ZBAL_DATA_TYPE_FP16 && quantMode == QUANT_BF16_2_INT8) {
            MoeDispatchNormal::DispatchNormal<float16_t, int8_t, true> op;
            op.Init(metaAddr, srcTokens, topkIndex, sendTokensIndex, putOffset, balanceMatrix, rank,
                numExperts, bs, hidden, topK, enableBalance, destTokens, destScale, &pipe);
            op.Process();
            return;
        }
    }
}

int32_t ZBALOpDispatchNormal(const zbal_tensor_info_t *srcTokens, const zbal_tensor_info_t *topkIndex,
                             const zbal_tensor_info_t *sendTokensIndex, const zbal_tensor_info_t *pushTargetOffset,
                             const zbal_tensor_info_t *balanceMatrix, int64_t expertNum, zbal_quant_mode_t quantMode,
                             const zbal_tensor_info_t *destTokens, const zbal_tensor_info_t *destScale,
                             bool enableBalance, aclrtStream stream, const CommGroupInfo &groupInfo, int64_t flags)
{
    uint32_t blockDim = 0;
    auto ret = aclrtGetResInCurrentThread(ACL_RT_DEV_RES_VECTOR_CORE, &blockDim);
    if (ret != 0) {
        printf("ZBALOpDispatchNormal failed as blockDim get failed, blockDim:%d\n", blockDim);
        return ret;
    }
    uint32_t rank = static_cast<uint32_t>(groupInfo.myGroupRank);
    uint32_t numExperts = static_cast<uint32_t>(expertNum);
    uint32_t bs = static_cast<uint32_t>(srcTokens->shape[0]);
    uint32_t hidden = static_cast<uint32_t>(srcTokens->shape[1]);
    uint32_t topK = static_cast<uint32_t>(topkIndex->shape[1]);
    uint64_t fftsAddr = groupInfo.fftsConfig;
    GM_ADDR metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);

    GM_ADDR srcTokensAddr = reinterpret_cast<uint8_t *>(srcTokens->data);
    GM_ADDR topkIndexAddr = reinterpret_cast<uint8_t *>(topkIndex->data);
    GM_ADDR sendTokensIndexAddr = reinterpret_cast<uint8_t *>(sendTokensIndex->data);
    GM_ADDR putOffsetAddr = reinterpret_cast<uint8_t *>(pushTargetOffset->data);
    GM_ADDR balanceMatrixAddr = reinterpret_cast<uint8_t *>(balanceMatrix->data);
    GM_ADDR destTokensAddr = reinterpret_cast<uint8_t *>(destTokens->data);
    GM_ADDR destScaleAddr = reinterpret_cast<uint8_t *>(destScale->data);

    zbal_datatype_t srcDataType = static_cast<zbal_datatype_t>(srcTokens->dataType);
    zbal_datatype_t dstDataType = static_cast<zbal_datatype_t>(destTokens->dataType);

    // launch kernel
    dispatch_normal<<<blockDim, nullptr, stream>>>(fftsAddr, metaAddr, srcTokensAddr, topkIndexAddr,
        sendTokensIndexAddr, putOffsetAddr, balanceMatrixAddr, rank, numExperts, bs, hidden, topK, quantMode,
        enableBalance, destTokensAddr, destScaleAddr, srcDataType, dstDataType);

    return 0;
}