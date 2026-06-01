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
#include "hybm_logger.h"
#include "hybm_ptracer.h"
#include "receiver_side_queue.h"
namespace ock {
namespace mf {
namespace transport {
namespace device {
ReceiverSideQueue::ReceiverSideQueue(uint32_t threadCount,
                                     std::unordered_map<uint16_t, RecvPhProcess> processors) noexcept
    : recvQueue_{"ind_recv"}, recvThreadCount_{threadCount}, phraseProcessors_{std::move(processors)}
{}

ReceiverSideQueue::~ReceiverSideQueue() noexcept
{
    Stop();
}

bool ReceiverSideQueue::Start(const std::shared_ptr<ThreadContext> &ctx) noexcept
{
    if (started_) {
        BM_LOG_WARN("SenderSideQueue already started");
        return true;
    }

    if (!recvQueue_.Start()) {
        started_ = false;
        return false;
    }

    started_ = true;
    for (auto i = 0U; i < recvThreadCount_; i++) {
        threads_.emplace_back([this, ctx](const int index) { ReceiverThreadProcess(index, ctx); }, i);
    }

    return true;
}

void ReceiverSideQueue::Stop() noexcept
{
    if (!started_) {
        return;
    }

    started_ = false;
    recvQueue_.Stop();
    for (auto &th : threads_) {
        if (th.joinable()) {
            th.join();
        }
    }
    threads_.clear();
}

void ReceiverSideQueue::ReceiverThreadProcess(int index, const std::shared_ptr<ThreadContext> &ctx)
{
    QueueMessage request;
    QueueMessage response;
    auto thName = std::string("ind_recv_").append(std::to_string(index));
    pthread_setname_np(pthread_self(), thName.c_str());
    BM_LOG_INFO("start thread for " << thName);

    if (ctx != nullptr) {
        auto ret = ctx->ThreadStartup();
        if (ret != 0) {
            BM_LOG_ERROR("receiver thread(" << index << ") context startup failed: " << ret);
            ctx->ThreadShutdown();
            return;
        }
    }

    while (started_) {
        if (!recvQueue_.DequeueMessage(request)) {
            continue;
        }

        BM_LOG_DEBUG("ReceiverSideQueue::Get Request:" << request.head);
        auto pos = phraseProcessors_.find(request.head.opCode);
        if (pos == phraseProcessors_.end()) {
            BM_LOG_ERROR("invalid request: " << response.head << " opcode invalid.");
            continue;
        }
        // 接收端 H2D
        if (request.head.opCode == 0) {
            TP_TRACE_RECORD(TP_INDIRECT_RECEIVER_PHASE_0_SCHE, (TP_CURRENT_TIME_NS - request.head.timestamp), 0);
        } else if (request.head.opCode == 1) {
            TP_TRACE_RECORD(TP_INDIRECT_RECEIVER_PHASE_1_SCHE, (TP_CURRENT_TIME_NS - request.head.timestamp), 0);
        }
        uint64_t timestamp = TP_CURRENT_TIME_NS;
        auto ret = pos->second(request, response);
        uint64_t diffNs = TP_CURRENT_TIME_NS - timestamp;
        if (request.head.opCode == 0) {
            TP_TRACE_RECORD(TP_INDIRECT_RECEIVER_PHASE_0, diffNs, ret);
        } else if (request.head.opCode == 1) {
            TP_TRACE_RECORD(TP_INDIRECT_RECEIVER_PHASE_1, diffNs, ret);
        }

        if (ret != 0) {
            BM_LOG_ERROR("process request: " << response.head << " failed: " << ret);
        }
        // 接收端 写回 hbm buffer的地址
        response.head.timestamp = TP_CURRENT_TIME_NS;
        recvQueue_.EnqueueMessage(std::move(response));
    }
    if (ctx != nullptr) {
        ctx->ThreadShutdown();
    }
    BM_LOG_INFO("end thread for " << thName);
}

} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock