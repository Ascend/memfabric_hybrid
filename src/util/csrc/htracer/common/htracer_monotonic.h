/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#ifndef HTRACER_MONOTONIC_H
#define HTRACER_MONOTONIC_H

#include <cstdio>
#include <fstream>
#include <string>
#include <cstdint>

namespace ock {
namespace utils {
class Monotonic {
#ifdef USE_PROCESS_MONOTONIC
public:
    /*
     * @brief init tick for us
     */
    template <int32_t FAILURE_RET> static int32_t InitTickUs()
    {
        /* get frequ */
        uint64_t tmpFreq = 0;
        __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(tmpFreq));
        auto freq = static_cast<uint32_t>(tmpFreq);

        /* calculate */
        freq = freq / 1000L / 1000L;
        if (freq == 0) {
            printf("Failed to get tick as freq is %d\n", freq);
            return FAILURE_RET;
        }

        return freq;
    }

    /*
     * @brief Get monotonic time in us, is not absolution time
     */
    static inline uint64_t TimeUs()
    {
        const static int32_t TICK_PER_US = InitTickUs<1>();
        uint64_t timeValue = 0;
        __asm__ volatile("mrs %0, cntvct_el0" : "=r"(timeValue));
        return timeValue / TICK_PER_US;
    }

    /*
     * @brief Get monotonic time in ns, is not absolution time
     */
    static inline uint64_t TimeNs()
    {
        const static int32_t TICK_PER_US = InitTickUs<1>();
        uint64_t timeValue = 0;
        __asm__ volatile("mrs %0, cntvct_el0" : "=r"(timeValue));
        return timeValue * 1000L / TICK_PER_US;
    }

    /*
     * @brief Get monotonic time in sec, is not absolution time
     */
    static inline uint64_t TimeSec()
    {
        const static int32_t TICK_PER_US = InitTickUs<1>();
        uint64_t timeValue = 0;
        __asm__ volatile("mrs %0, cntvct_el0" : "=r"(timeValue));
        return timeValue / (TICK_PER_US * 1000000L);
    }

#else /* USE_PROCESS_MONOTONIC */
public:
    template <int32_t FAILURE_RET> static int32_t InitTickUs()
    {
        return 0;
    }

    static inline uint64_t TimeUs()
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec * 1000000L + ts.tv_nsec / 1000L);
    }

    static inline uint64_t TimeNs()
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec * 1000000000L + ts.tv_nsec);
    }

    static inline uint64_t TimeSec()
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec + ts.tv_nsec / 1000000000L);
    }
#endif /* USE_PROCESS_MONOTONIC */
};
}
}
#endif // HTRACER_MONOTONIC_H