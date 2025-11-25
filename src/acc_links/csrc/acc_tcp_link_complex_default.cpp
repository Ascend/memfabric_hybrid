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
#include "acc_tcp_link_complex_default.h"
#include "acc_tcp_worker.h"

namespace ock {
namespace acc {
Result AccTcpLinkComplexDefault::Initialize(uint16_t sendQueueCap, int32_t workIndex, AccTcpWorker *worker)
{
    ASSERT_RETURN(sendQueueCap < UNO_256, ACC_INVALID_PARAM);
    ASSERT_RETURN(worker != nullptr, ACC_INVALID_PARAM);

    queue_ = AccMakeRef<AccLinkedMessageQueue>(sendQueueCap);
    ASSERT_RETURN(queue_.Get() != nullptr, ACC_NEW_OBJECT_FAIL);

    data_ = AccMakeRef<AccDataBuffer>(UNO_1024);
    ASSERT_RETURN(data_.Get() != nullptr, ACC_NEW_OBJECT_FAIL);
    ASSERT_RETURN(data_->DataPtr() != nullptr, ACC_NEW_OBJECT_FAIL);

    header_ = AccMsgHeader();
    receiveState_ = AccLinkReceiveState();

    workerIndex_ = static_cast<uint32_t>(workIndex);
    worker_ = worker;
    worker_->IncreaseRef();
    established_ = true;

    return ACC_OK;
}

void AccTcpLinkComplexDefault::UnInitialize()
{
    queue_ = nullptr;
    data_ = nullptr;
    if (worker_ != nullptr) {
        worker_->DecreaseRef();
        worker_ = nullptr;
    }
}
Result AccTcpLinkComplexDefault::EnqueueAndModifyEpoll(const AccMsgHeader &h, const AccDataBufferPtr &d,
                                                       const AccDataBufferPtr &cbCtx)
{
    ASSERT_RETURN(worker_ != nullptr, ACC_ERROR);
    auto result = queue_->EnqueueBack(h, d, cbCtx);
    if (UNLIKELY(result != ACC_OK)) {
        LOG_WARN("Failed to enqueue message into link " << this->id_ << ", errorCode:" << result
                                                        << ", queue size:" << queue_->GetSize());
        return result;
    }

    return worker_->ModifyLink(this, POLLIN | POLLOUT | EPOLLET);
}
}  // namespace acc
}  // namespace ock