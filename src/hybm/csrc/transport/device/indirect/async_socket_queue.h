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

#ifndef MEMFABRIC_HYBRID_ASYNC_SOCKET_QUEUE_H
#define MEMFABRIC_HYBRID_ASYNC_SOCKET_QUEUE_H

#include <vector>
#include <queue>
#include <list>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <ostream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <condition_variable>
#include <sys/epoll.h>

namespace ock {
namespace mf {
namespace transport {
namespace device {

struct QueueMessageHead {
    uint8_t request;
    int8_t errorCode{0};
    uint16_t opCode;
    uint32_t srcRankId;
    uint32_t dstRankId;
    int socketFd{-1};
    uint32_t bodySize;
    uint64_t requestId;
    uint64_t timestamp{0UL};

    QueueMessageHead() noexcept : request(1), opCode{0}, srcRankId{0}, dstRankId{0}, bodySize{0}, requestId{0} {}
};

inline std::ostream &operator<<(std::ostream &stream, const QueueMessageHead &head) noexcept
{
    stream << "MessageHead(req?" << static_cast<uint32_t>(head.request) << ",err=" << static_cast<int>(head.errorCode)
           << ",opCode=" << head.opCode << ",src=" << head.srcRankId << ",dst=" << head.dstRankId
           << ",fd=" << head.socketFd << ",reqId=" << head.requestId << ",bodySz=" << head.bodySize << ")";
    return stream;
}

struct QueueMessage {
    QueueMessageHead head;
    std::vector<uint8_t> body;
};

class StreamMessageRW {
public:
    StreamMessageRW(uint32_t rankId, int fd) noexcept;
    inline uint32_t GetRemoteRankId() const noexcept
    {
        return remoteRankId_;
    }
    bool Read(QueueMessage &message) noexcept;
    void BeginWrite(QueueMessage &&message) noexcept;
    bool ContinueWrite() noexcept;

private:
    bool WriteOneMessage(QueueMessage &message) noexcept;

private:
    QueueMessage readingMessage_;
    std::mutex writingMutex_;
    std::list<QueueMessage> writingMessages_;
    std::list<QueueMessage> cachedWriteMessages_;
    uint32_t readOffset_{0};
    uint32_t writeOffset_{0};
    const uint32_t remoteRankId_;
    const int socketFd_;
};

class ThreadContext {
public:
    virtual ~ThreadContext() = default;
    virtual int ThreadStartup() noexcept = 0;
    virtual void ThreadShutdown() noexcept = 0;
};

class AsyncSocketQueue {
public:
    explicit AsyncSocketQueue(std::string name) noexcept;
    ~AsyncSocketQueue() noexcept;
    bool Start() noexcept;
    void Stop() noexcept;
    void AddRankIdSocket(uint32_t rankId, int socket) noexcept;
    void RemoveRankIdSocket(uint32_t rankId) noexcept;
    bool ExistRankIdSocket(uint32_t rankId) noexcept;
    void AddSocket(int socket) noexcept;
    void RemoveSocket(int socket) noexcept;
    void CloseSockets() noexcept;
    bool EnqueueMessage(QueueMessage &&message) noexcept;
    bool DequeueMessage(QueueMessage &message) noexcept;

private:
    int CreateEpollSocket() noexcept;
    void AsyncSocketThreadProcess() noexcept;
    std::shared_ptr<StreamMessageRW> GetStreamMessageRW(int fd) noexcept;
    void ProcessEvent(epoll_event event);

private:
    const std::string name_;
    std::unordered_map<uint32_t, int> rankId2SockMap_;
    std::unordered_set<int> noRankSockets_;
    std::mutex rankId2SockMutex_;
    int readEpollFd_{-1};
    std::unordered_map<int, std::shared_ptr<StreamMessageRW>> streamRWs_;

    std::queue<QueueMessage> sendQueue_;
    std::queue<QueueMessage> recvQueue_;
    std::mutex recvQueueMutex_;
    std::condition_variable recvQueueCond_;
    std::thread epollLoopThread_;
    std::atomic<bool> started_{false};
};
} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_ASYNC_SOCKET_QUEUE_H
