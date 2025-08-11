/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_SMEM_TIMEDWAIT_H
#define MEM_FABRIC_HYBRID_SMEM_TIMEDWAIT_H

#include <mutex>
#include "smem_types.h"

constexpr uint64_t SECOND_TO_MILLSEC = 1000U;

namespace ock {
namespace smem {
class SmemTimedwait {    // wait signal or overtime, instead of sem_timedwait
public:
    SmemTimedwait() = default;
    ~SmemTimedwait() = default;

    Result Initialize()
    {
        signalFlag = false;
        int32_t attrInitRet = pthread_condattr_init(&cattr_);
        int32_t setLockRet = pthread_condattr_setclock(&cattr_, CLOCK_MONOTONIC);
        int32_t condInitRet = pthread_cond_init(&condTimeChecker_, &cattr_);
        int32_t mutexInitRet = pthread_mutex_init(&timeCheckerMutex_, nullptr);
        if (attrInitRet || setLockRet || condInitRet || mutexInitRet) {
            return SM_ERROR;
        }
        return SM_OK;
    }

    int32_t TimedwaitMillsecs(long msecs)
    {
        struct timespec ts {0, 0};
        int32_t ret = 0;

        pthread_mutex_lock(&this->timeCheckerMutex_);
        clock_gettime(CLOCK_MONOTONIC, &ts);

        // avoid ts.tv_nsec overflow
        long secs = msecs / SECOND_TO_MILLSEC;
        msecs = (msecs % SECOND_TO_MILLSEC) * SECOND_TO_MILLSEC * SECOND_TO_MILLSEC + ts.tv_nsec;
        long add = msecs / (SECOND_TO_MILLSEC * SECOND_TO_MILLSEC * SECOND_TO_MILLSEC);
        ts.tv_sec += (add + secs);
        ts.tv_nsec = msecs % (SECOND_TO_MILLSEC * SECOND_TO_MILLSEC * SECOND_TO_MILLSEC);
        while (!this->signalFlag) {    // avoid spurious wakeup
            ret = pthread_cond_timedwait(&this->condTimeChecker_, &this->timeCheckerMutex_, &ts);
            if (ret == ETIMEDOUT) {    // avoid infinite loop
                ret = SM_TIMEOUT;
                break;
            }
        }
        this->signalFlag = false;
        pthread_mutex_unlock(&this->timeCheckerMutex_);

        return ret;
    }

    // signal will NOT lost when call PthreadSignal before PthreadTimedwaitMillsecs, so we can proactive cleanup
    void SignalClean()
    {
        signalFlag = false;
    }

    int32_t PthreadSignal()
    {
        int32_t signalRet = 0;
        pthread_mutex_lock(&this->timeCheckerMutex_);
        signalFlag = true;
        signalRet = pthread_cond_signal(&this->condTimeChecker_);
        pthread_mutex_unlock(&this->timeCheckerMutex_);
        return signalRet;
    }
private:
    pthread_condattr_t cattr_;
    pthread_cond_t condTimeChecker_;
    pthread_mutex_t timeCheckerMutex_;
    bool signalFlag { false };  // signal will NOT lost when call PthreadSignal before PthreadTimedwaitMillsecs
};

}
}

#endif // MEM_FABRIC_HYBRID_SMEM_TIMEDWAIT_H
