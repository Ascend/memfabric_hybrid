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

#ifndef MEMFABRIC_HYBRID_RECEIVER_SIDE_QUEUE_H
#define MEMFABRIC_HYBRID_RECEIVER_SIDE_QUEUE_H

#include <functional>
#include <unordered_map>
#include "async_socket_queue.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

using RecvPhProcess = std::function<int(const QueueMessage &request, QueueMessage &response)>;
class ReceiverSideQueue {
public:
    ReceiverSideQueue(uint32_t threadCount, std::unordered_map<uint16_t, RecvPhProcess> processors) noexcept;
    virtual ~ReceiverSideQueue() noexcept;

    bool Start(const std::shared_ptr<ThreadContext> &ctx = nullptr) noexcept;

    void Stop() noexcept;

    void AddAcceptSocket(int socket) noexcept
    {
        recvQueue_.AddSocket(socket);
    }

    void RemoveAcceptSocket(int socket) noexcept
    {
        recvQueue_.RemoveSocket(socket);
    }

    void CloseAllSockets() noexcept
    {
        recvQueue_.CloseSockets();
    }

private:
    void ReceiverThreadProcess(int index, const std::shared_ptr<ThreadContext> &ctx);

private:
    AsyncSocketQueue recvQueue_;
    std::atomic<bool> started_{false};
    std::vector<std::thread> threads_;
    std::shared_ptr<ThreadContext> threadContext_;
    const uint32_t recvThreadCount_;
    const std::unordered_map<uint16_t, RecvPhProcess> phraseProcessors_;
};
}
} // namespace transport
} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_RECEIVER_SIDE_QUEUE_H
