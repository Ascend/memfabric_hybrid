/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#include "acc_includes.h"
#include "acc_tcp_request_context.h"

namespace ock {
namespace acc {
Result AccTcpRequestContext::Reply(int16_t result, const AccDataBufferPtr &d) const
{
    ASSERT_RETURN(d.Get() != nullptr, ACC_INVALID_PARAM);
    ASSERT_RETURN(link_.Get() != nullptr, ACC_LINK_ERROR);
    if (UNLIKELY(!link_->Established())) {
        LOG_ERROR("Failed to send reply message with message type " << header_.type << ", seqlo " << header_.seqNo
                                                                    << " as the link is broken");
        return ACC_LINK_ERROR;
    }
    AccMsgHeader replyHeader(header_.type, result, d->DataLen(), header_.seqNo);
    return link_->EnqueueAndModifyEpoll(replyHeader, d, nullptr);
}
}  // namespace acc
}  // namespace ock