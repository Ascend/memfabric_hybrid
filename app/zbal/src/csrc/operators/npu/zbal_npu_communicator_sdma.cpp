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
#include "zbal_npu_communicator_sdma.h"
#include "zbal_npu_operators.h"

namespace zbal {
namespace operators {

int32_t NpuCommunicatorSDMA::Scatter(const void *sendBuff, void *recvBuff, uint64_t data_count,
                                     zbal_datatype_t dataType, uint16_t root, aclrtStream stream) noexcept
{
    return ZBALOpScatterSDMA(sendBuff, recvBuff, data_count, dataType, root, stream,
                             const_cast<CommGroupInfo &>(GetMetaInfo()));
}

} // namespace operators
} // namespace zbal
