/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef ACC_LINKS_ACC_TCP_WORKER_H
#define ACC_LINKS_ACC_TCP_WORKER_H

#include <utility>

#include "acc_tcp_common.h"
#include "acc_tcp_link.h"
#include "acc_tcp_link_complex_default.h"
#include "acc_tcp_request_context.h"
#include "acc_tcp_shared_buf.h"

namespace ock {
namespace acc {
using LinkBrokenHandlerInner = std::function<int32_t(const AccTcpLinkComplexDefaultPtr &link)>;

/*
 * Worker is for epoll event from connection sockets
 */
class AccTcpWorker : public AccReferable {
public:
    explicit AccTcpWorker(AccTcpWorkerOptions options) : options_(std::move(options))
    {
    }
    ~AccTcpWorker() override
    {
        Stop();
    }

    Result Start();
    void Stop(bool afterFork = false);

    Result AddLink(const AccTcpLinkComplexDefaultPtr &link, uint32_t events) noexcept;
    Result ModifyLink(const AccTcpLinkComplexDefaultPtr &link, uint32_t events) noexcept;
    Result RemoveLink(const AccTcpLinkComplexDefaultPtr &link) noexcept;

    void RegisterNewRequestHandler(const AccNewReqHandler &h);
    void RegisterRequestSentHandler(const AccReqSentHandler &h);
    void RegisterLinkBrokenHandler(const LinkBrokenHandlerInner &h);

private:
    void SetPropertiesForThread();
    void RunInThread(std::atomic<bool>* started);
    Result ValidateOptions();
    void StopInner(bool afterFork);
    Result ProcessEvent(struct epoll_event &event) noexcept;

private:
    int epollFD_ = -1; /* epoll fd */
    bool needStop_ = false; /* if the worker need to be stopped */
    AccNewReqHandler newRequestHandle_ = nullptr;
    AccReqSentHandler requestSentHandle_ = nullptr;
    LinkBrokenHandlerInner linkBrokenHandle_ = nullptr;

    /* non-hot variables */
    std::mutex mutex_;
    AccTcpWorkerOptions options_; /* worker options */
    std::atomic<bool> started_{false}; /* if the worker started */
    std::thread epollThread_; /* thread */
    std::atomic<bool> threadStarted_{false};
};
using AccTcpWorkerPtr = AccRef<AccTcpWorker>;

inline Result AccTcpWorker::ModifyLink(const AccTcpLinkComplexDefaultPtr &link, uint32_t events) noexcept
{
    ASSERT_RETURN(link.Get(), ACC_INVALID_PARAM);

    LOG_TRACE("Try to modify link " << link->ShortName() << " in sock worker " << options_.Name() << " with event "
                                    << events);

    struct epoll_event evNewFd {};
    evNewFd.data.ptr = link.Get();
    evNewFd.events = events;

    if (UNLIKELY(epoll_ctl(epollFD_, EPOLL_CTL_MOD, link->fd_, &evNewFd) != 0)) {
        LOG_ERROR("Failed to modify " << link->ShortName() << " for sock worker " << options_.Name()
                                      << ", errno:" << errno);
        return ACC_EPOLL_ERROR;
    }

    return ACC_OK;
}

inline Result AccTcpWorker::ProcessEvent(struct epoll_event &event) noexcept
{
    auto* link = static_cast<AccTcpLinkComplexDefault*>(event.data.ptr);
    if (UNLIKELY(link == nullptr)) {
        LOG_ERROR("Link is null in polled event for worker " << options_.Name());
        return ACC_EPOLL_ERROR;
    }

    if (event.events & EPOLLIN) { /* there is in data */
        auto result = link->HandlePollIn();
        if (result == ACC_LINK_MSG_READY) { /* ready for message, do upper call */
            AccTcpRequestContext ctx(link->header_, link->data_, link);
            (void)newRequestHandle_(ctx);
            /* ET mode, each loop only handle one message, need to add event again */
            (void)ModifyLink(link, EPOLLIN | EPOLLOUT | EPOLLET);
            return ACC_OK;
        } else if (result == ACC_LINK_EAGAIN) { /* need to continue read data */
            (void)ModifyLink(link, EPOLLIN | EPOLLOUT | EPOLLET);
            return ACC_OK; /* not fully received, continue to process next event */
        } else if (result == ACC_LINK_ERROR) { /* link error */
            (void)linkBrokenHandle_(link);
            return ACC_OK;
        }

        return ACC_OK; /* ignore other error */
    } else if (event.events & EPOLLOUT) { /* there is free out buffer */
        AccMsgHeader outHeader{};
        AccDataBufferPtr cbCtx;
        auto result = link->HandlePollOut(outHeader, cbCtx); /* call link to send something */
        if (result == ACC_LINK_MSG_SENT) { /* if message sent */
            if (requestSentHandle_ != nullptr) { /* call sent callback if set */
                (void)requestSentHandle_(MSG_SENT, outHeader, cbCtx);
            }
            /* ET mode, each loop only handle one message, need to add event again */
            (void)ModifyLink(link, EPOLLIN | EPOLLOUT | EPOLLET);
        } else if (result == ACC_LINK_EAGAIN) { /* if message is partial sent */
            (void)ModifyLink(link, EPOLLIN | EPOLLOUT | EPOLLET);
        } else if (result == ACC_LINK_ERROR) { /* if link error */
            (void)ModifyLink(link, EPOLLWRNORM);
        }

        return ACC_OK;
    } else if (event.events & EPOLLWRNORM) {
        (void)linkBrokenHandle_(link);
        return ACC_OK;
    }

    LOG_TRACE("Receive link " << link->id_ << " event " << event.events); /* continue to process next event */
    return ACC_OK;
}

inline void AccTcpWorker::RegisterNewRequestHandler(const AccNewReqHandler &h)
{
    ASSERT_RET_VOID(h != nullptr);
    ASSERT_RET_VOID(newRequestHandle_ == nullptr);
    newRequestHandle_ = h;
}

inline void AccTcpWorker::RegisterRequestSentHandler(const AccReqSentHandler &h)
{
    ASSERT_RET_VOID(h != nullptr);
    ASSERT_RET_VOID(requestSentHandle_ == nullptr);
    requestSentHandle_ = h;
}

inline void AccTcpWorker::RegisterLinkBrokenHandler(const LinkBrokenHandlerInner &h)
{
    ASSERT_RET_VOID(h != nullptr);
    ASSERT_RET_VOID(linkBrokenHandle_ == nullptr);
    linkBrokenHandle_ = h;
}
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_TCP_WORKER_H
