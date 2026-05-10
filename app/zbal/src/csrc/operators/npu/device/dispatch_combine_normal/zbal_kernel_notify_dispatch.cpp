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
#include "zbal_kernel_notify_dispatch.h"

using namespace AscendC;

extern "C" __global__ __aicore__
void notify_dispatch(uint64_t fftsAddr, GM_ADDR metaAddr, GM_ADDR tokenPerExpert, int64_t sendCount, uint32_t numTopk,
                     uint32_t rank, GM_ADDR recvData, GM_ADDR totalRecvTokens, GM_ADDR recvTokensPerExpert,
                     GM_ADDR putOffset, GM_ADDR balanceMatrix, float factorHigh, float factorLow)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);
    AscendC::SetSyncBaseAddr(fftsAddr);
    AscendC::TPipe pipe;
    MoeNotifyDispatch::NotifyDispatch<int32_t> op;
    op.Init(metaAddr, tokenPerExpert, sendCount, numTopk, rank, recvData, totalRecvTokens,
        recvTokensPerExpert, putOffset, balanceMatrix, factorHigh, factorLow, &pipe);
    op.Process();
}

int32_t ZBALOpNotifyDispatch(const zbal_tensor_info_t *sendTokensPerExpert, int64_t sendCount,
                             int64_t topKNum, const zbal_tensor_info_t *recvBuff,
                             const zbal_tensor_info_t *totalRecvTokens, const zbal_tensor_info_t *recvTokensPerExpert,
                             const zbal_tensor_info_t *pushTargetOffset, const zbal_tensor_info_t *balanceMatrix,
                             float factorHigh, float factorLow, aclrtStream stream, const CommGroupInfo &groupInfo,
                             int64_t flags)
{
    uint32_t blockDim = 0;
    auto ret = aclrtGetResInCurrentThread(ACL_RT_DEV_RES_VECTOR_CORE, &blockDim);
    if (ret != 0) {
        printf("ZBALOpNotifyDispatch failed as blockDim get failed, blockDim:%d\n", blockDim);
        return ret;
    }
    uint32_t rank = static_cast<uint32_t>(groupInfo.myGroupRank);
    uint32_t numTopk = static_cast<uint32_t>(topKNum);
    uint64_t fftsAddr = groupInfo.fftsConfig;
    GM_ADDR metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);

    GM_ADDR tokenPerExpertAddr = reinterpret_cast<uint8_t *>(sendTokensPerExpert->data);
    GM_ADDR recvDataAddr = reinterpret_cast<uint8_t *>(recvBuff->data);
    GM_ADDR totalRecvTokensAddr = reinterpret_cast<uint8_t *>(totalRecvTokens->data);
    GM_ADDR recvTokensPerExpertAddr = reinterpret_cast<uint8_t *>(recvTokensPerExpert->data);
    GM_ADDR putOffsetAddr = reinterpret_cast<uint8_t *>(pushTargetOffset->data);
    GM_ADDR balanceMatrixAddr = reinterpret_cast<uint8_t *>(balanceMatrix->data);

    // launch kernel
    notify_dispatch<<<blockDim, nullptr, stream>>>(fftsAddr, metaAddr, tokenPerExpertAddr, sendCount, numTopk, rank,
        recvDataAddr, totalRecvTokensAddr, recvTokensPerExpertAddr, putOffsetAddr,
        balanceMatrixAddr, factorHigh, factorLow);

    return 0;
}