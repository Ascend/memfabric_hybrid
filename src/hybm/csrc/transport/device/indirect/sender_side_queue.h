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

#ifndef MEMFABRIC_HYBRID_SENDER_SIDE_QUEUE_H
#define MEMFABRIC_HYBRID_SENDER_SIDE_QUEUE_H

#include <functional>
#include <unordered_map>
#include "async_socket_queue.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

using SendPhProcess = std::function<int(const QueueMessage &res, QueueMessage &nextReq, bool &finished, void *)>;

class SenderSideQueue {
public:
    SenderSideQueue(uint32_t threadCount, std::unordered_map<uint16_t, SendPhProcess> processors) noexcept;

    virtual ~SenderSideQueue() noexcept;

    bool Start(const std::shared_ptr<ThreadContext> &ctx = nullptr) noexcept;

    void Stop() noexcept;

    void AddRankIdSocket(uint32_t rankId, int socket) noexcept
    {
        sendQueue_.AddRankIdSocket(rankId, socket);
    }

    void RemoveRankIdSocket(uint32_t rankId) noexcept
    {
        sendQueue_.RemoveRankIdSocket(rankId);
    }

    bool ExistRankIdSocket(uint32_t rankId) noexcept
    {
        return sendQueue_.ExistRankIdSocket(rankId);
    }

    void CloseAllSockets() noexcept
    {
        sendQueue_.CloseSockets();
    }

    int BeginRequest(QueueMessage &&request, void *context) noexcept;

private:
    void SenderThreadProcess(int index, const std::shared_ptr<ThreadContext> &ctx);

private:
    AsyncSocketQueue sendQueue_;
    std::mutex contextMutex_;
    std::unordered_map<uint64_t, void *> messageContext_;
    std::vector<std::thread> threads_;
    std::atomic<bool> started_{false};
    std::shared_ptr<ThreadContext> threadContext_;
    const uint32_t sendThreadCount_;
    const std::unordered_map<uint16_t, SendPhProcess> phraseProcessors_;
};
} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_SENDER_SIDE_QUEUE_H
