/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MOBS_NET_WAIT_HANDLE_H
#define MOBS_NET_WAIT_HANDLE_H

#include <pthread.h>

#include "mobs_net_common.h"
#include "mobs_net_context_store.h"

namespace ock {
namespace mobs {

class NetWaitHandler : public MoReferable {
public:
    explicit NetWaitHandler(const NetContextStorePtr &ctxStore)
    {
        /* hold the reference */
        ctxStore_ = ctxStore.Get();
        if (LIKELY(ctxStore_ != nullptr)) {
            ctxStore_->IncreaseRef();
        }
    }

    ~NetWaitHandler() override
    {
        /* decrease the reference count */
        if (LIKELY(ctxStore_ != nullptr)) {
            ctxStore_->DecreaseRef();
            ctxStore_ = nullptr;
        }
    }

    /* *
     * @brief Initialize the wait handler, it should be called before issue a net request
     *
     * @return
     */
    Result Initialize()
    {
        /* init pthread condition attr with relative time, instead of abs time */
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
        auto err = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
        if (UNLIKELY(err != 0)) {
            KVS_LOG_ERROR("Failed to init pthread condition, error " << err);
            return K_ERROR;
        }

        /* init condition */
        pthread_cond_init(&cond_, &attr);
        return K_OK;
    }

    /* *
     * @brief Wait for a request to be finished
     *
     * @param second         [in] time in second, default timeout is max value of uint32_t
     *
     * @return 0 if ok, K_TIMEOUT if timeout
     */
    Result TimedWait(uint32_t second = UINT32_MAX) noexcept
    {
        pthread_mutex_lock(&mutex_);
        /* already notified */
        if (notified) {
            pthread_mutex_unlock(&mutex_);
            return K_OK;
        }

        /* relative time instead of abs */
        struct timespec currentTime {};
        clock_gettime(CLOCK_MONOTONIC, &currentTime);

        struct timespec futureTime {};
        futureTime.tv_sec = currentTime.tv_sec + second;
        futureTime.tv_nsec = currentTime.tv_nsec;
        auto waitResult = pthread_cond_timedwait(&cond_, &mutex_, &futureTime);
        if (waitResult == ETIMEDOUT) {
            pthread_mutex_unlock(&mutex_);
            return K_TIMEOUT;
        }

        /* notify by other */
        pthread_mutex_unlock(&mutex_);
        return K_OK;
    }

    /* *
     * @brief notify waiter
     *
     * @param result         [in] net response result
     * @param data           [in] data from peer
     * @return 0 if ok, K_ALREADY_NOTIFIED if already notified
     */
    Result Notify(int32_t result, const TcpDataBufPtr &data) noexcept
    {
        pthread_mutex_lock(&mutex_);
        /* already notified */
        if (notified) {
            pthread_mutex_unlock(&mutex_);
            return K_ALREADY_NOTIFIED;
        }

        data_ = data;
        result_ = result;
        pthread_cond_signal(&cond_);
        notified = true;
        pthread_mutex_unlock(&mutex_);
        return K_OK;
    }

    /* *
     * @brief Get the result replied from notifier (i.e. peer service)
     *
     * @return result set by notifier
     */
    inline int32_t GetResult() const
    {
        return result_;
    }

    /* *
     * @brief Get the data replied from notifier (i.e. peer service)
     *
     * @return data set by notifier
     */
    inline const TcpDataBufPtr &Data() const
    {
        return data_;
    }

private:
    NetContextStore *ctxStore_ = nullptr; /* hold the reference ctx store, in case of use after free */
    TcpDataBufPtr data_;                  /* the data that replied from peer */
    int32_t result_ = INT32_MAX;          /* the result from peer */
    bool notified = false;                /* notified or not to prevent notify again */
    pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond_{};
};
using NetWaitHandlerPtr = MoRef<NetWaitHandler>;
}
}

#endif  //MOBS_NET_WAIT_HANDLE_H
