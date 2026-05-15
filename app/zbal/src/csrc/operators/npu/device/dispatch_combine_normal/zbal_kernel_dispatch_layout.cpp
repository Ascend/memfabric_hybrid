/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
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
#include "zbal_kernel_dispatch_layout.h"

using namespace AscendC;

extern "C" __global__ __aicore__ void dispatch_layout(uint64_t fftsAddr, GM_ADDR topkIdx, uint32_t numTokens,
                                                      uint32_t numExperts, uint32_t numTopk, uint32_t numRanks,
                                                      uint32_t rank, GM_ADDR numTokensPerRank,
                                                      GM_ADDR numTokensPerExpert, GM_ADDR isTokenInRank,
                                                      GM_ADDR sendTokenIdx, GM_ADDR notifySendData)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);
    AscendC::SetSyncBaseAddr(fftsAddr);
    AscendC::TPipe pipe;
    MoeDispatchLayout::DispatchLayout<int32_t> op;
    op.Init(topkIdx, numTokens, numExperts, numTopk, numRanks, rank, numTokensPerRank, numTokensPerExpert,
            isTokenInRank, sendTokenIdx, notifySendData, &pipe);
    op.Process();
}

int32_t ZBALOpDispatchLayout(const zbal_tensor_info_t *topkIndex, int64_t tokens, int64_t expertNum, int64_t topkNum,
                             const zbal_tensor_info_t *tokensPerRank, const zbal_tensor_info_t *tokensPerExpert,
                             const zbal_tensor_info_t *isTokenInRank, const zbal_tensor_info_t *sendTokensIndex,
                             const zbal_tensor_info_t *notifySendData, aclrtStream stream,
                             const CommGroupInfo &groupInfo, int64_t flags)
{
    uint32_t blockDim = 0;
    auto ret = aclrtGetResInCurrentThread(ACL_RT_DEV_RES_VECTOR_CORE, &blockDim);
    if (ret != 0) {
        printf("ZBALOpDispatchLayout failed as blockDim get failed, blockDim:%d\n", blockDim);
        return ret;
    }
    uint32_t rank = static_cast<uint32_t>(groupInfo.myGroupRank);
    uint32_t numRanks = static_cast<uint32_t>(groupInfo.groupSize);
    uint32_t numTokens = static_cast<uint32_t>(tokens);
    uint32_t numExperts = static_cast<uint32_t>(expertNum);
    uint32_t numTopk = static_cast<uint32_t>(topkNum);
    uint64_t fftsAddr = groupInfo.fftsConfig;

    GM_ADDR topkIndexAddr = reinterpret_cast<uint8_t *>(topkIndex->data);
    GM_ADDR tokensPerRankAddr = reinterpret_cast<uint8_t *>(tokensPerRank->data);
    GM_ADDR tokensPerExpertAddr = reinterpret_cast<uint8_t *>(tokensPerExpert->data);
    GM_ADDR isTokenInRankAddr = reinterpret_cast<uint8_t *>(isTokenInRank->data);
    GM_ADDR sendTokensIndexAddr = reinterpret_cast<uint8_t *>(sendTokensIndex->data);
    GM_ADDR notifySendDataAddr = reinterpret_cast<uint8_t *>(notifySendData->data);

    // launch kernel
    dispatch_layout<<<blockDim, nullptr, stream>>>(fftsAddr, topkIndexAddr, numTokens, numExperts, numTopk, numRanks,
                                                   rank, tokensPerRankAddr, tokensPerExpertAddr, isTokenInRankAddr,
                                                   sendTokensIndexAddr, notifySendDataAddr);

    return 0;
}