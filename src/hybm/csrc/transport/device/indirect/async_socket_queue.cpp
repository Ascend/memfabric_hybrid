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
#include <sys/uio.h>
#include <fcntl.h>
#include <pthread.h>
#include "ptracer.h"
#include "hybm_logger.h"
#include "hybm_types.h"
#include "hybm_ptracer.h"
#include "async_socket_queue.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
constexpr auto MAX_EVENTS_COUNT = 1000U;
constexpr auto EPOLL_WAIT_TIMEOUT = 2000; // 2seconds

StreamMessageRW::StreamMessageRW(uint32_t rankId, int fd) noexcept : remoteRankId_{rankId}, socketFd_{fd} {}

bool StreamMessageRW::Read(QueueMessage &message) noexcept
{
    if (readOffset_ < sizeof(QueueMessageHead)) {
        auto headBuf = static_cast<uint8_t *>(static_cast<void *>(&readingMessage_.head));
        auto n = read(socketFd_, headBuf + readOffset_, sizeof(QueueMessageHead) - readOffset_);
        if (n == 0 || (n < 0 && errno != EAGAIN)) {
            if (n < 0) {
                BM_LOG_ERROR("read from socket: " << socketFd_ << " failed: " << errno << ": " << strerror(errno)
                                                  << ", n:" << n);
            }
            return false;
        }

        readOffset_ += static_cast<uint32_t>(n);
        if (readOffset_ < sizeof(QueueMessageHead)) {
            BM_LOG_ERROR("read from socket: " << socketFd_ << " failed: " << errno << ": " << strerror(errno)
                                              << ", n:" << n << ", off:" << readOffset_);
            return false;
        }
        readingMessage_.body.resize(readingMessage_.head.bodySize);
    }

    if (readingMessage_.head.bodySize == 0) {
        readOffset_ = 0;
        BM_LOG_DEBUG("read no body message: " << readingMessage_.head << " success.");
        message = std::move(readingMessage_);
        return true;
    }

    auto bodyOffset = readOffset_ - sizeof(QueueMessageHead);
    auto bodyBuf = readingMessage_.body.data();
    auto n = read(socketFd_, bodyBuf + bodyOffset, readingMessage_.head.bodySize - bodyOffset);
    if (n == 0 || (n < 0 && errno != EAGAIN)) {
        BM_LOG_ERROR("read from socket: " << socketFd_ << " failed: " << errno << ": " << strerror(errno));
        return false;
    }

    readOffset_ += static_cast<uint32_t>(n);
    if (readOffset_ < sizeof(QueueMessageHead) + readingMessage_.head.bodySize) {
        // 数据未完全读取，继续等待下一次读取
        BM_LOG_DEBUG("read from socket: " << socketFd_ << " incomplete, need more data, n:" << n
                                          << ", off:" << readOffset_ << ", bodySize:" << readingMessage_.head.bodySize);
        return false;
    }

    readOffset_ = 0;
    message = std::move(readingMessage_);
    message.head.timestamp = TP_CURRENT_TIME_NS;
    BM_LOG_DEBUG("readmessage: " << message.head << " success.");
    return true;
}

void StreamMessageRW::BeginWrite(QueueMessage &&message) noexcept
{
    std::unique_lock<std::mutex> locker{writingMutex_};
    writingMessages_.emplace_back(std::move(message));
}

bool StreamMessageRW::ContinueWrite() noexcept
{
    {
        std::unique_lock<std::mutex> locker{writingMutex_};
        cachedWriteMessages_.splice(cachedWriteMessages_.end(), writingMessages_);
    }

    while (!cachedWriteMessages_.empty()) {
        if (!WriteOneMessage(cachedWriteMessages_.front())) {
            return false;
        }
        cachedWriteMessages_.pop_front();
    }

    if (!cachedWriteMessages_.empty()) {
        BM_LOG_ERROR("write cache not empty");
        return false;
    }

    std::unique_lock<std::mutex> locker{writingMutex_};
    return writingMessages_.empty();
}

bool StreamMessageRW::WriteOneMessage(QueueMessage &message) noexcept
{
    TP_TRACE_RECORD(INDIRECT_WRITE_OUT_SCHE, (TP_CURRENT_TIME_NS - message.head.timestamp), 0);
    std::vector<iovec> iov;
    auto head = static_cast<uint8_t *>(static_cast<void *>(&message.head));
    auto body = message.body.data();
    if (writeOffset_ < sizeof(QueueMessageHead)) {
        iov.emplace_back(iovec{head + writeOffset_, sizeof(QueueMessageHead) - writeOffset_});
    }

    if (message.head.bodySize > 0) {
        auto bodyOffset = writeOffset_ < sizeof(QueueMessageHead) ? 0 : (writeOffset_ - sizeof(QueueMessageHead));
        if (bodyOffset < message.head.bodySize) {
            iov.emplace_back(iovec{body + bodyOffset, message.head.bodySize - bodyOffset});
        }
    }

    if (iov.empty()) {
        writeOffset_ = 0;
        BM_LOG_WARN("no need send:" << message.head);
        return true;
    }

    auto n = writev(socketFd_, iov.data(), iov.size());
    if (n == 0 || (n < 0 && errno != EAGAIN)) {
        BM_LOG_ERROR("write to socket: " << socketFd_ << " failed: " << errno << ": " << strerror(errno));
        return false;
    }

    writeOffset_ += static_cast<uint32_t>(n);
    if (writeOffset_ < sizeof(QueueMessageHead) + message.head.bodySize) {
        // 数据未完全写入，继续等待下一次写入
        BM_LOG_DEBUG("write to socket: " << socketFd_ << " incomplete, need more writes, writeOffset_: " << writeOffset_
                                         << ", body: " << message.head.bodySize);
        return false;
    }

    writeOffset_ = 0;
    BM_LOG_DEBUG("write message: " << message.head << " success.");
    return true;
}

AsyncSocketQueue::AsyncSocketQueue(std::string name) noexcept : name_{std::move(name)} {}

AsyncSocketQueue::~AsyncSocketQueue() noexcept
{
    Stop();
}

bool AsyncSocketQueue::Start() noexcept
{
    if (started_) {
        BM_LOG_WARN("SocketQueue(" << name_ << ") already started");
        return true;
    }

    if (CreateEpollSocket() != BM_OK) {
        BM_LOG_WARN("CreateEpollSocket failed.");
        return false;
    }

    started_ = true;
    epollLoopThread_ = std::thread([this]() { AsyncSocketThreadProcess(); });
    return true;
}

void AsyncSocketQueue::Stop() noexcept
{
    if (!started_) {
        return;
    }

    started_ = false;
    recvQueueCond_.notify_one();
    if (epollLoopThread_.joinable()) {
        epollLoopThread_.join();
    }
    if (readEpollFd_ >= 0) {
        close(readEpollFd_);
        readEpollFd_ = -1;
    }
}

void AsyncSocketQueue::AddRankIdSocket(uint32_t rankId, int socket) noexcept
{
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);

    epoll_event ev {};
    ev.events = EPOLLIN; // 可读事件
    ev.data.fd = socket;
    epoll_ctl(readEpollFd_, EPOLL_CTL_ADD, socket, &ev);

    std::unique_lock<std::mutex> locker{rankId2SockMutex_};
    rankId2SockMap_.emplace(rankId, socket);
    streamRWs_.emplace(socket, std::make_shared<StreamMessageRW>(rankId, socket));
}

void AsyncSocketQueue::RemoveRankIdSocket(uint32_t rankId) noexcept
{
    std::unique_lock<std::mutex> locker{rankId2SockMutex_};
    auto pos = rankId2SockMap_.find(rankId);
    if (pos == rankId2SockMap_.end()) {
        return;
    }
    auto socketFd = pos->second;
    rankId2SockMap_.erase(pos);
    streamRWs_.erase(socketFd);
    locker.unlock();

    epoll_ctl(readEpollFd_, EPOLL_CTL_DEL, socketFd, nullptr);
}

bool AsyncSocketQueue::ExistRankIdSocket(uint32_t rankId) noexcept
{
    std::unique_lock<std::mutex> locker{rankId2SockMutex_};
    return rankId2SockMap_.find(rankId) != rankId2SockMap_.end();
}

void AsyncSocketQueue::AddSocket(int socket) noexcept
{
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);

    epoll_event ev {};
    ev.events = EPOLLIN; // 可读事件
    ev.data.fd = socket;
    epoll_ctl(readEpollFd_, EPOLL_CTL_ADD, socket, &ev);

    std::unique_lock<std::mutex> locker{rankId2SockMutex_};
    noRankSockets_.emplace(socket);
    streamRWs_.emplace(socket, std::make_shared<StreamMessageRW>(std::numeric_limits<uint32_t>::max(), socket));
}

void AsyncSocketQueue::RemoveSocket(int socket) noexcept
{
    epoll_ctl(readEpollFd_, EPOLL_CTL_DEL, socket, nullptr);
    std::unique_lock<std::mutex> locker{rankId2SockMutex_};
    noRankSockets_.erase(socket);
}

void AsyncSocketQueue::CloseSockets() noexcept
{
    std::unique_lock<std::mutex> locker{rankId2SockMutex_};
    for (auto fd : noRankSockets_) {
        epoll_ctl(readEpollFd_, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
    }
    noRankSockets_.clear();

    for (auto it = rankId2SockMap_.begin(); it != rankId2SockMap_.end(); ++it) {
        epoll_ctl(readEpollFd_, EPOLL_CTL_DEL, it->second, nullptr);
        close(it->second);
    }
    rankId2SockMap_.clear();
}

bool AsyncSocketQueue::EnqueueMessage(QueueMessage &&message) noexcept
{
    if (!started_) {
        return false;
    }

    BM_LOG_DEBUG("enqueue message: " << message.head);
    auto remoteRankId = message.head.request != 0 ? message.head.dstRankId : message.head.srcRankId;
    auto socketFd = message.head.socketFd;

    std::unique_lock<std::mutex> rankIdLocker{rankId2SockMutex_};
    if (socketFd < 0) {
        auto pos = rankId2SockMap_.find(remoteRankId);
        if (pos == rankId2SockMap_.end()) {
            BM_LOG_ERROR("send " << message.head << " remote rank: " << remoteRankId << " no socket.");
            return false;
        }
        socketFd = pos->second;
    }
    auto streamPos = streamRWs_.find(socketFd);
    if (streamPos == streamRWs_.end()) {
        BM_LOG_ERROR("send " << message.head << " remote rank: " << remoteRankId << " no stream.");
        return false;
    }
    auto stream = streamPos->second;
    rankIdLocker.unlock();

    BM_LOG_DEBUG("start open write for socket fd: " << socketFd);
    stream->BeginWrite(std::move(message));
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.fd = socketFd;
    epoll_ctl(readEpollFd_, EPOLL_CTL_MOD, socketFd, &ev);
    return true;
}

bool AsyncSocketQueue::DequeueMessage(QueueMessage &message) noexcept
{
    if (!started_) {
        return false;
    }

    std::unique_lock<std::mutex> locker{recvQueueMutex_};
    if (recvQueue_.empty()) {
        recvQueueCond_.wait_for(locker, std::chrono::seconds(1), [this]() { return !recvQueue_.empty() || !started_; });
    }

    if (recvQueue_.empty() || !started_) {
        return false;
    }

    message = std::move(recvQueue_.front());
    recvQueue_.pop();
    locker.unlock();
    BM_LOG_DEBUG("dequeue one message: " << message.head);

    return true;
}

int AsyncSocketQueue::CreateEpollSocket() noexcept
{
    auto epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd < 0) {
        BM_LOG_ERROR("AsyncSocketQueue epoll fd target create failed: " << errno << ": " << strerror(errno));
        return BM_ERROR;
    }

    std::vector<int> sockets;
    std::unique_lock<std::mutex> locker{rankId2SockMutex_};
    sockets.reserve(rankId2SockMap_.size());
    for (auto it = rankId2SockMap_.begin(); it != rankId2SockMap_.end(); ++it) {
        sockets.emplace_back(it->second);
    }
    locker.unlock();

    for (auto fd : sockets) {
        epoll_event ev {};
        ev.events = EPOLLIN;
        epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev);
    }

    readEpollFd_ = epollFd;
    return BM_OK;
}

void AsyncSocketQueue::AsyncSocketThreadProcess() noexcept
{
    std::string thName = name_ + "_EP";
    pthread_setname_np(pthread_self(), thName.c_str());
    BM_LOG_INFO("start thread for " << thName);

    std::vector<epoll_event> events(MAX_EVENTS_COUNT);
    while (started_) {
        TP_TRACE_BEGIN(INDIRECT_EPOOL_WAIT);
        int count = epoll_wait(readEpollFd_, events.data(), MAX_EVENTS_COUNT, EPOLL_WAIT_TIMEOUT);
        if (count <= 0) {
            TP_TRACE_END(INDIRECT_EPOOL_WAIT, count);
            continue;
        }
        TP_TRACE_END(INDIRECT_EPOOL_WAIT, 0);

        for (auto i = 0; i < count; i++) {
            ProcessEvent(events[i]);
        }
    }
    BM_LOG_INFO("end thread for " << thName);
}

void AsyncSocketQueue::ProcessEvent(const epoll_event event)
{
    auto fd = event.data.fd;
    auto evt = event.events;

    if (evt & (EPOLLERR | EPOLLHUP)) {
        return;
    }

    auto streamRW = GetStreamMessageRW(fd);
    if (streamRW == nullptr) {
        return;
    }

    if (evt & EPOLLIN) {
        QueueMessage inMessage;
        if (streamRW->Read(inMessage)) {
            inMessage.head.socketFd = fd;
            std::unique_lock<std::mutex> locker{recvQueueMutex_};
            recvQueue_.push(std::move(inMessage));
            locker.unlock();
            recvQueueCond_.notify_one();

            if (name_ == "ind_recv") {
                TP_TRACE_RECORD(INDIRECT_RECEIVER_RECV_CNT, MAX_EVENTS_COUNT, 0);
            } else if (name_ == "ind_send") {
                TP_TRACE_RECORD(INDIRECT_SENDER_RECV_CNT, MAX_EVENTS_COUNT, 0);
            }
        } else {
            // BM_LOG_ERROR("read failed for socket fd: " << fd);
        }
    } else if (evt & EPOLLOUT) {
        if (streamRW->ContinueWrite()) {
            epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.fd = fd;
            epoll_ctl(readEpollFd_, EPOLL_CTL_MOD, fd, &ev);
            if (name_ == "ind_recv") {
                TP_TRACE_RECORD(INDIRECT_RECEIVER_SEND_CNT, MAX_EVENTS_COUNT, 0);
            } else if (name_ == "ind_send") {
                TP_TRACE_RECORD(INDIRECT_SENDER_SEND_CNT, MAX_EVENTS_COUNT, 0);
            }
        } else {
            BM_LOG_ERROR("write failed for socket fd: " << fd);
        }
    }
}

std::shared_ptr<StreamMessageRW> AsyncSocketQueue::GetStreamMessageRW(int fd) noexcept
{
    std::shared_ptr<StreamMessageRW> result = nullptr;
    std::unique_lock<std::mutex> locker{rankId2SockMutex_};
    auto pos = streamRWs_.find(fd);
    if (pos == streamRWs_.end()) {
        locker.unlock();
        BM_LOG_WARN("socket " << fd << " not exist.");
        return nullptr;
    }
    result = pos->second;
    locker.unlock();

    return result;
}
} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock