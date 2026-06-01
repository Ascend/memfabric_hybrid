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
#include "sender_side_queue.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
SenderSideQueue::SenderSideQueue(uint32_t threadCount, std::unordered_map<uint16_t, SendPhProcess> processors) noexcept
    : sendQueue_{"ind_send"}, sendThreadCount_{threadCount}, phraseProcessors_{std::move(processors)}
{}

SenderSideQueue::~SenderSideQueue() noexcept
{
    Stop();
}

bool SenderSideQueue::Start(const std::shared_ptr<ThreadContext> &ctx) noexcept
{
    if (started_) {
        BM_LOG_WARN("SenderSideQueue already started");
        return true;
    }

    if (!sendQueue_.Start()) {
        started_ = false;
        return false;
    }

    started_ = true;
    threadContext_ = ctx;
    for (auto i = 0U; i < sendThreadCount_; i++) {
        threads_.emplace_back([this, ctx](const int index) { SenderThreadProcess(index, ctx); }, i);
    }

    return true;
}

void SenderSideQueue::Stop() noexcept
{
    if (!started_) {
        return;
    }

    started_ = false;
    sendQueue_.Stop();
    for (auto &th : threads_) {
        if (th.joinable()) {
            th.join();
        }
    }
    threads_.clear();
}

int SenderSideQueue::BeginRequest(QueueMessage &&request, void *context) noexcept
{
    auto opCode = request.head.opCode;
    auto requestId = request.head.requestId;
    std::unique_lock<std::mutex> locker{contextMutex_};
    messageContext_.emplace(requestId, context);
    locker.unlock();

    if (!sendQueue_.EnqueueMessage(std::move(request))) {
        BM_LOG_ERROR("begin request message(id:" << requestId << ", opCode:" << opCode << ") failed.");
        std::unique_lock<std::mutex> releaseLocker{contextMutex_};
        messageContext_.erase(requestId);
        releaseLocker.unlock();
        return -1;
    }

    return 0;
}

void SenderSideQueue::SenderThreadProcess(int index, const std::shared_ptr<ThreadContext> &ctx)
{
    QueueMessage response;
    QueueMessage nextReq;
    bool finished = false;
    auto thName = std::string("ind_sender_").append(std::to_string(index));
    pthread_setname_np(pthread_self(), thName.c_str());
    BM_LOG_INFO("start thread for " << thName);
    if (ctx != nullptr) {
        auto ret = ctx->ThreadStartup();
        if (ret != 0) {
            BM_LOG_ERROR("sender thread(" << index << ") context startup failed: " << ret);
            ctx->ThreadShutdown();
            return;
        }
    }

    while (started_) {
        if (!sendQueue_.DequeueMessage(response)) {
            continue;
        }

        auto pos = phraseProcessors_.find(response.head.opCode);
        if (pos == phraseProcessors_.end()) {
            BM_LOG_ERROR("invalid response: " << response.head << " opcode invalid.");
            continue;
        }

        std::unique_lock<std::mutex> locker{contextMutex_};
        auto contextIt = messageContext_.find(response.head.requestId);
        if (contextIt == messageContext_.end()) {
            locker.unlock();
            BM_LOG_ERROR("invalid response: " << response.head << " requestId invalid.");
            continue;
        }
        auto context = contextIt->second;
        locker.unlock();
        uint64_t timestamp = TP_CURRENT_TIME_NS;
        auto ret = (pos->second)(response, nextReq, finished, context);
        uint64_t diffNs = TP_CURRENT_TIME_NS - timestamp;
        if (response.head.opCode == 0) {
            TP_TRACE_RECORD(TP_INDIRECT_SENDER_PHASE_0, diffNs, ret);
        } else if (response.head.opCode == 1) {
            TP_TRACE_RECORD(TP_INDIRECT_SENDER_PHASE_1, diffNs, ret);
        }
        if (ret != 0) {
            BM_LOG_ERROR("process response(id: " << response.head.requestId << ", opCode:" << response.head.opCode
                                                 << ") failed: " << ret);
        }

        BM_LOG_DEBUG("SenderThreadProcess Process Response(" << response.head.requestId << ") finished: " << finished);
        if (finished) {
            BM_LOG_DEBUG("finished remove request: " << response.head.requestId);
            std::unique_lock<std::mutex> finishLocker{contextMutex_};
            messageContext_.erase(response.head.requestId);
            continue;
        }

        nextReq.head.timestamp = TP_CURRENT_TIME_NS;
        sendQueue_.EnqueueMessage(std::move(nextReq));
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