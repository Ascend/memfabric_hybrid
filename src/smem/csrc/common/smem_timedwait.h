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

#ifndef MEM_FABRIC_HYBRID_SMEM_TIMEDWAIT_H
#define MEM_FABRIC_HYBRID_SMEM_TIMEDWAIT_H

#include <mutex>
#include <functional>
#include "smem_types.h"

constexpr uint64_t SECOND_TO_MILLSEC = 1000U;
constexpr uint64_t MILLSEC_TO_NANOSSEC = 1000000U;
constexpr uint64_t SECOND_TO_NANOSSEC = 1000000000U;

namespace ock {
namespace smem {
class SmemTimedwait {    // wait signal or overtime, instead of sem_timedwait
public:
    SmemTimedwait() = default;
    ~SmemTimedwait()
    {
        if (inited_) {
            pthread_condattr_destroy(&cattr_);
            pthread_cond_destroy(&condTimeChecker_);
            pthread_mutex_destroy(&timeCheckerMutex_);
        }
    }

    Result Initialize()
    {
        if (inited_) {
            return SM_OK;
        }
        signalFlag = false;
        int32_t attrInitRet = pthread_condattr_init(&cattr_);
        if (attrInitRet != 0) {
            return SM_ERROR;
        }

        int32_t setClockRet = pthread_condattr_setclock(&cattr_, CLOCK_MONOTONIC);
        if (setClockRet != 0) {
            pthread_condattr_destroy(&cattr_);
            return SM_ERROR;
        }

        int32_t condInitRet = pthread_cond_init(&condTimeChecker_, &cattr_);
        if (condInitRet != 0) {
            pthread_condattr_destroy(&cattr_);
            return SM_ERROR;
        }

        int32_t mutexInitRet = pthread_mutex_init(&timeCheckerMutex_, nullptr);
        if (mutexInitRet != 0) {
            pthread_cond_destroy(&condTimeChecker_);
            pthread_condattr_destroy(&cattr_);
            return SM_ERROR;
        }
        inited_ = true;
        return SM_OK;
    }

    int32_t TimedwaitMillsecs(long msecs, const std::function<void()> &wakeupOp = nullptr)
    {
        struct timespec ts {0, 0};
        int32_t ret = 0;

        pthread_mutex_lock(&this->timeCheckerMutex_);
        if (this->signalFlag && wakeupOp != nullptr) {
            wakeupOp();
            this->signalFlag = false;
            pthread_mutex_unlock(&this->timeCheckerMutex_);
            return SM_OK;
        }

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
                ret = SM_OK;
                break;
            }
        }
        this->signalFlag = false;
        if (wakeupOp != nullptr) {
            wakeupOp();
        }
        pthread_mutex_unlock(&this->timeCheckerMutex_);

        return ret;
    }

    void OperateInLock(const std::function<void()> &op, bool notify = false)
    {
        pthread_mutex_lock(&this->timeCheckerMutex_);
        if (op != nullptr) {
            op();
        }
        if (notify) {
            signalFlag = true;
        }
        pthread_mutex_unlock(&this->timeCheckerMutex_);
        if (notify) {
            pthread_cond_signal(&this->condTimeChecker_);
        }
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
    bool inited_{false};
    pthread_condattr_t cattr_;
    pthread_cond_t condTimeChecker_;
    pthread_mutex_t timeCheckerMutex_;
    bool signalFlag { false };  // signal will NOT lost when call PthreadSignal before PthreadTimedwaitMillsecs
};

}
}

#endif // MEM_FABRIC_HYBRID_SMEM_TIMEDWAIT_H