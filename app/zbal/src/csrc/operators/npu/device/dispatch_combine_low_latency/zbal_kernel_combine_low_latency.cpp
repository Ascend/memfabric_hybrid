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
#include "zbal_kernel_combine_low_latency.h"

using namespace AscendC;

extern "C" __global__ __aicore__ void combine_low_latency(int64_t fftsAddr, GM_ADDR metaAddr, GM_ADDR expandX,
                                                          GM_ADDR expertIds, GM_ADDR expandIdx, GM_ADDR epSendCount,
                                                          GM_ADDR scales, GM_ADDR XOut, uint32_t rank,
                                                          uint32_t numExperts, uint32_t bs, uint32_t hidden,
                                                          uint32_t topK, uint32_t srcDataType, uint32_t dstDataType)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);
    AscendC::SetSyncBaseAddr(fftsAddr);
    AscendC::TPipe pipe;

    if (srcDataType == ZBAL_DATA_TYPE_BFP16) {
        MoeCombineLowLatency::CombineLowLatency<bfloat16_t, bfloat16_t, int32_t, true, false> op;
        op.Init(metaAddr, expandX, expertIds, expandIdx, epSendCount, scales, XOut, rank, numExperts, bs, hidden, topK,
                &pipe);
        op.Process();
        return;
    } else if (srcDataType == ZBAL_DATA_TYPE_FP16) {
        MoeCombineLowLatency::CombineLowLatency<float16_t, float16_t, int32_t, true, false> op;
        op.Init(metaAddr, expandX, expertIds, expandIdx, epSendCount, scales, XOut, rank, numExperts, bs, hidden, topK,
                &pipe);
        op.Process();
        return;
    }
}

int32_t ZBALOpCombineLowLatency(const zbal_tensor_info_t *expandX, const zbal_tensor_info_t *expertIds,
                                const zbal_tensor_info_t *expandIdx, const zbal_tensor_info_t *epSendCount,
                                const zbal_tensor_info_t *scales, const zbal_tensor_info_t *XOut, int64_t moeExpertNum,
                                aclrtStream stream, const CommGroupInfo &groupInfo, int64_t flags)
{
    uint32_t blockDim = 0;
    auto ret = aclrtGetResInCurrentThread(ACL_RT_DEV_RES_VECTOR_CORE, &blockDim);
    if (ret != 0) {
        printf("ZBALOpCombineLowLatency failed as blockDim get failed, blockDim:%d\n", blockDim);
        return ret;
    }
    uint32_t rank = static_cast<uint32_t>(groupInfo.myGroupRank);
    uint32_t numExperts = static_cast<uint32_t>(moeExpertNum);
    uint32_t hidden = static_cast<uint32_t>(expandX->shape[1]);
    uint32_t bs = static_cast<uint32_t>(expertIds->shape[0]);
    uint32_t topK = static_cast<uint32_t>(expertIds->shape[1]);
    uint64_t fftsAddr = groupInfo.fftsConfig;
    GM_ADDR metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);

    zbal_datatype_t srcDataType = static_cast<zbal_datatype_t>(XOut->dataType);
    zbal_datatype_t dstDataType = static_cast<zbal_datatype_t>(expandX->dataType);

    GM_ADDR expandXAddr = reinterpret_cast<uint8_t *>(expandX->data);
    GM_ADDR expertIdsAddr = reinterpret_cast<uint8_t *>(expertIds->data);
    GM_ADDR expandIdxAddr = reinterpret_cast<uint8_t *>(expandIdx->data);
    GM_ADDR epSendCountsAddr = reinterpret_cast<uint8_t *>(epSendCount->data);
    GM_ADDR scalesAddr = reinterpret_cast<uint8_t *>(scales->data);
    GM_ADDR XOutAddr = reinterpret_cast<uint8_t *>(XOut->data);

    // launch kernel
    combine_low_latency<<<blockDim, nullptr, stream>>>(fftsAddr, metaAddr, expandXAddr, expertIdsAddr, expandIdxAddr,
                                                       epSendCountsAddr, scalesAddr, XOutAddr, rank, numExperts, bs,
                                                       hidden, topK, srcDataType, dstDataType);
    return 0;
}