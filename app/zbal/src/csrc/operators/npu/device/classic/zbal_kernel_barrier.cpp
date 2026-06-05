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
#include "zbal_def.h"
#include "zbal_kernel_utils.h"
#include "zbal_kernel_trace.h"
#include "zbal_kernel_base.h"

class ZBALBarrierKernel : public ZBALBaseKernel {
public:
    ZBAL_KERNEL ZBALBarrierKernel() {}

    ZBAL_KERNEL void Init(GM_ADDR metaGM, uint64_t waitSymbol)
    {
#ifdef __DAV_C220_VEC__
        this->comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaGM);
        this->myGroupRank = comm->myGroupRank;
        this->groupSize = comm->groupSize;
        this->memSize = comm->localDeviceMemSize;
        this->worldRanks = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);
#endif
    }

    ZBAL_KERNEL void Process()
    {
#ifdef __DAV_C220_VEC__
        ZBAL_PROF_START(comm, ZBAL_PROF_BARRIER);

        BarrierAll();

        ZBAL_PROF_STOP(comm, ZBAL_PROF_BARRIER);
#endif
    }
};

extern "C" __global__ __aicore__ void ZBALBarrierInner(GM_ADDR metaAddr, uint64_t waitSymbol)
{
    ZBALBarrierKernel op;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);

    op.Init(metaAddr, waitSymbol);
    op.Process();
}

int32_t ZBALOpBarrier(aclrtStream stream, CommGroupInfo &groupInfo)
{
    static uint32_t blockDim = 0;
    if (blockDim == 0) {
        auto ret = aclrtGetResInCurrentThread(ACL_RT_DEV_RES_VECTOR_CORE, &blockDim);
        if (ret != 0) {
            printf("ZBALOpAlltoAllV get block dim failed, blockDim:%d\n", ret);
            return ret;
        }
    }

    uint8_t *metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);
    uint64_t waitSymbol = ++groupInfo.waitSymbol;

    ZBALBarrierInner<<<blockDim, nullptr, stream>>>(metaAddr, waitSymbol);
    return 0;
}