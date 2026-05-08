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
#include "zbal_kernel_allgather.h"

constexpr uint32_t SMALL_DATA_SIZE = 1024 * 7168;

extern "C" __global__ __aicore__ void ZBALAllGatherInner(GM_ADDR input, GM_ADDR output, size_t elements, int dataType,
                                                         GM_ADDR metaAddr, uint64_t waitSymbol)
{
    // TD: enable multi kernel dispatch dealing with different shape : if (elements <= SMALL_DATA_SIZE)
    if (false) {
        AllGatherSmallKernel op;
        zbal_datatype_t ZBAL_DATA_TYPE = static_cast<zbal_datatype_t>(dataType);
        KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);

        switch (ZBAL_DATA_TYPE)
        {
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT8:
                op.Init<int8_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<int8_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT16:
                op.Init<int16_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<int16_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT32:
                op.Init<int32_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<int32_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_FP16:
                op.Init<float16_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<float16_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_FP32:
                op.Init<float>(input, output, metaAddr, elements, waitSymbol);
                op.Process<float>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT64:
                op.Init<int64_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<int64_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT64:
                op.Init<uint64_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<uint64_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT8:
                op.Init<uint8_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<uint8_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT16:
                op.Init<uint16_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<uint16_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT32:
                op.Init<uint32_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<uint32_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_FP64:
                op.Init<float64_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<float64_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_BFP16:
                op.Init<bfloat16_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<bfloat16_t>();
                break;
            default:
                break;
        }
    } else {
        AllGatherBigKernel op;
        zbal_datatype_t ZBAL_DATA_TYPE = static_cast<zbal_datatype_t>(dataType);
        KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);

        switch (ZBAL_DATA_TYPE)
        {
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT8:
                op.Init<int8_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<int8_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT16:
                op.Init<int16_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<int16_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT32:
                op.Init<int32_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<int32_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_FP16:
                op.Init<float16_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<float16_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_FP32:
                op.Init<float>(input, output, metaAddr, elements, waitSymbol);
                op.Process<float>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT64:
                op.Init<int64_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<int64_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT64:
                op.Init<uint64_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<uint64_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT8:
                op.Init<uint8_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<uint8_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT16:
                op.Init<uint16_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<uint16_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT32:
                op.Init<uint32_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<uint32_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_FP64:
                op.Init<float64_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<float64_t>();
                break;
            case zbal_datatype_t::ZBAL_DATA_TYPE_BFP16:
                op.Init<bfloat16_t>(input, output, metaAddr, elements, waitSymbol);
                op.Process<bfloat16_t>();
                break;
            default:
                break;
        }
    }
}

int32_t ZBALOpAllGather(const void *sendBuff, void *recvBuff, size_t sendCount, zbal_datatype_t dataType,
                        aclrtStream stream, CommGroupInfo &groupInfo)
{
    uint32_t blockDim = ZBALOpGetAivBlockDim(groupInfo, sendCount, dataType);

    int dataTypeInt = static_cast<int>(dataType);
    uint8_t *metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);
    uint8_t *realSendBuff = reinterpret_cast<uint8_t *>(const_cast<void *>(sendBuff));
    uint8_t *realRecvBuff = reinterpret_cast<uint8_t *>(recvBuff);
    uint64_t waitSymbol = ++groupInfo.waitSymbol;

    ZBALAllGatherInner<<<blockDim, nullptr, stream>>>(realSendBuff, realRecvBuff, sendCount, dataTypeInt, metaAddr,
                                                      waitSymbol);
    return 0;
}