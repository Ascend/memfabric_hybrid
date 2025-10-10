/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MONOTONIC_H
#define MEMFABRIC_HYBRID_MONOTONIC_H

#include <cstdio>
#include <cstdint>
#include <ctime>

namespace ock {
namespace smem {
class MonotonicTime {
public:
    /**
     * @brief Get monotonic time in us, is not absolution time
     */
    static inline uint64_t TimeUs();

    /**
     * @brief Get monotonic time in ns, is not absolution time
     */
    static inline uint64_t TimeNs();

private:
    /* only ENABLE_CPU_MONOTONIC */
    template <uint64_t FAILURE_RET>
    static uint64_t InitTickUs();
};

class MonoPerfTrace {
public:
    MonoPerfTrace();

    ~MonoPerfTrace() = default;

    /**
     * @brief Record start time
     */
    void RecordStart() noexcept;

    /**
     * @brief Record end time
     */
    void RecordEnd() noexcept;

    /**
     * @brief Get period in ns
     */
    uint64_t PeriodNs() const noexcept;

    /**
     * @brief Get period in us
     */
    uint64_t PeriodUs() const noexcept;

    /**
     * @brief Get period in ms
     */
    uint64_t PeriodMs() const noexcept;

public:
    uint64_t start = 0; /* start time in ns */
    uint64_t end = UINT64_MAX;   /* end time in ns */
};

#if defined(ENABLE_CPU_MONOTONIC) && defined(__aarch64__)

template <uint64_t FAILURE_RET>
inline uint64_t MonotonicTime::InitTickUs()
{
    /* get frequ */
    uint64_t tmpFreq = 0;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(tmpFreq));
    uint64_t freq = tmpFreq;

    /* calculate */
    freq = freq / 1000ULL / 1000ULL;
    if (freq == 0) {
        printf("Failed to get tick as freq is %lu\n", freq);
        return FAILURE_RET;
    }

    return freq;
}

inline uint64_t MonotonicTime::TimeUs()
{
    const static uint64_t TICK_PER_US = InitTickUs<1>();
    uint64_t timeValue = 0;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(timeValue));
    return timeValue / TICK_PER_US;
}

inline uint64_t MonotonicTime::TimeNs()
{
    const static uint64_t TICK_PER_US = InitTickUs<1>();
    uint64_t timeValue = 0;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(timeValue));
    // 使用 128 位中间结果防止溢出
    unsigned __int128 tmp = (unsigned __int128)timeValue * 1000ULL;
    return (uint64_t)(tmp / TICK_PER_US);
}

#else  /* defined(ENABLE_CPU_MONOTONIC) && defined(__aarch64__) */

template <uint64_t FAILURE_RET>
uint64_t MonotonicTime::InitTickUs()
{
    return 0;
}

inline uint64_t MonotonicTime::TimeUs()
{
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000L + static_cast<uint64_t>(ts.tv_nsec) / 1000L;
}

inline uint64_t MonotonicTime::TimeNs()
{
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000L + static_cast<uint64_t>(ts.tv_nsec);
}
#endif /* ENABLE_CPU_MONOTONIC */

/* functions of MonoPerfTrace */
inline MonoPerfTrace::MonoPerfTrace()
{
    RecordStart();
}

inline void MonoPerfTrace::RecordStart() noexcept
{
    start = MonotonicTime::TimeNs();
}

inline void MonoPerfTrace::RecordEnd() noexcept
{
    end = MonotonicTime::TimeNs();
}

inline uint64_t MonoPerfTrace::PeriodNs() const noexcept
{
    return end - start;
}

inline uint64_t MonoPerfTrace::PeriodUs() const noexcept
{
    return (end - start) / 1000L;
}

inline uint64_t MonoPerfTrace::PeriodMs() const noexcept
{
    return (end - start) / 100000L;
}

}  // namespace smem
}  // namespace ock

#endif  // MEMFABRIC_HYBRID_MONOTONIC_H