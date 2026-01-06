/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_PTRACER_TRACEPOINT_H
#define MEM_FABRIC_PTRACER_TRACEPOINT_H

#include "ptracer_utils.h"

namespace ock {
namespace mf {
namespace tracer {
class Tracepoint {
public:
    __always_inline void TraceBegin(const std::string &tpName)
    {
        bool expectVal = false;
        if (nameValid_.compare_exchange_weak(expectVal, true)) {
            name_ = tpName;
        }
        begin_.fetch_add(1u, std::memory_order_relaxed);
    }

    __always_inline void TraceEnd(uint64_t diff, int32_t goodBadExecution)
    {
        if (goodBadExecution != 0) { /* ignore the time cost counting for bad execution */
            badEnd_.fetch_add(1u, std::memory_order_relaxed);
            return;
        }

        if (diff < min_) {
            min_.store(diff, std::memory_order_relaxed);
        }

        if (diff < previousMin_) {
            previousMin_.store(diff, std::memory_order_relaxed);
        }

        if (diff > max_) {
            max_.store(diff, std::memory_order_relaxed);
        }

        if (diff > previousMax_) {
            previousMax_.store(diff, std::memory_order_relaxed);
        }

        total_.fetch_add(diff, std::memory_order_relaxed);
        goodEnd_.fetch_add(1u, std::memory_order_relaxed);
    }

    __always_inline void Reset()
    {
        begin_ = 0;
        goodEnd_ = 0;
        badEnd_ = 0;
        min_ = UINT64_MAX;
        max_ = 0;
        total_ = 0;
    }

    __always_inline const std::string &GetName() const
    {
        return name_;
    }

    __always_inline uint64_t GetBegin() const
    {
        return begin_.load(std::memory_order_relaxed);
    }

    __always_inline uint64_t GetGoodEnd() const
    {
        return goodEnd_.load(std::memory_order_relaxed);
    }

    __always_inline uint64_t GetBadEnd() const
    {
        return badEnd_.load(std::memory_order_relaxed);
    }

    __always_inline uint64_t GetMin() const
    {
        return min_.load(std::memory_order_relaxed);
    }

    __always_inline uint64_t GetMax() const
    {
        return max_.load(std::memory_order_relaxed);
    }

    __always_inline uint64_t GetTotal() const
    {
        return total_.load(std::memory_order_relaxed);
    }

    __always_inline bool Valid(const bool needTotal) const
    {
        if (needTotal) {
            return nameValid_ && begin_.load(std::memory_order_relaxed) > 0;
        } else {
            return nameValid_ && begin_.load(std::memory_order_relaxed) > previousBegin_;
        }
    }

    void UpdatePreviousData()
    {
        previousBegin_ = begin_.load(std::memory_order_relaxed);
        previousGoodEnd_ = goodEnd_.load(std::memory_order_relaxed);
        previousBadEnd_ = badEnd_.load(std::memory_order_relaxed);
        previousTotal_ = total_.load(std::memory_order_relaxed);
        previousMin_ = UINT64_MAX;
        previousMax_ = 0;
    }

    std::string ToPeriodString()
    {
        auto beginGap = begin_.load(std::memory_order_relaxed) - previousBegin_;
        auto goodEndGap = goodEnd_.load(std::memory_order_relaxed) - previousGoodEnd_;
        auto badEndGap = badEnd_.load(std::memory_order_relaxed) - previousBadEnd_;
        auto totalGap = total_.load(std::memory_order_relaxed) - previousTotal_;
        auto minGap = previousMin_.load(std::memory_order_relaxed);
        auto maxGap = previousMax_.load(std::memory_order_relaxed);
        UpdatePreviousData();
        return Func::FormatString(name_, beginGap, goodEndGap, badEndGap, minGap, maxGap, totalGap);
    }

    std::string ToTotalString()
    {
        auto beginGap = begin_.load(std::memory_order_relaxed);
        auto goodEndGap = goodEnd_.load(std::memory_order_relaxed);
        auto badEndGap = badEnd_.load(std::memory_order_relaxed);
        auto totalGap = total_.load(std::memory_order_relaxed);
        auto minGap = min_.load(std::memory_order_relaxed);
        auto maxGap = max_.load(std::memory_order_relaxed);
        return Func::FormatString(name_, beginGap, goodEndGap, badEndGap, minGap, maxGap, totalGap);
    }

private:
    std::string name_;
    std::atomic<bool> nameValid_{false};
    std::atomic_uint_fast64_t begin_{0};
    std::atomic_uint_fast64_t goodEnd_{0};
    std::atomic_uint_fast64_t badEnd_{0};
    std::atomic_uint_fast64_t min_{UINT64_MAX};
    std::atomic_uint_fast64_t max_{0};
    std::atomic_uint_fast64_t total_{0};

    uint64_t previousBegin_{0};
    uint64_t previousGoodEnd_{0};
    uint64_t previousBadEnd_{0};
    std::atomic_uint_fast64_t previousMin_{UINT64_MAX};
    std::atomic_uint_fast64_t previousMax_{0};
    uint64_t previousTotal_{0};
};

class TracepointCollection {
public:
    static __always_inline Tracepoint **GetTracepoints()
    {
        static Tracepoint **tracePoints = CreateInstance();
        return tracePoints;
    }

    static __always_inline Tracepoint *GetTracepoint(uint32_t tpId)
    {
        auto tracePoints = GetTracepoints();
        if (PTRACER_UNLIKELY(tracePoints == nullptr)) {
            return nullptr;
        }

        uint32_t moduleId = GetModuleId(tpId);
        uint32_t traceId = GetTraceId(tpId);
        if (PTRACER_UNLIKELY(moduleId > MAX_MODULE_COUNT || traceId > MAX_TRACE_ID_COUNT)) {
            return nullptr;
        }

        return &tracePoints[moduleId][traceId];
    }

    static __always_inline void TraceBegin(uint32_t tpId, const std::string &tpName)
    {
        auto tracepoint = GetTracepoint(tpId);
        if (PTRACER_UNLIKELY(tracepoint == nullptr)) {
            return;
        }

        tracepoint->TraceBegin(tpName);
    }

    static __always_inline void TraceEnd(uint32_t tpId, const uint64_t &diff, int32_t goodBadExecution)
    {
        auto tracepoint = GetTracepoint(tpId);
        if (PTRACER_UNLIKELY(tracepoint == nullptr)) {
            return;
        }

        tracepoint->TraceEnd(diff, goodBadExecution);
    }

public:
    static constexpr int32_t MAX_MODULE_COUNT = 64;
    static constexpr int32_t MAX_TRACE_ID_COUNT = 1024;

private:
    static __always_inline Tracepoint **CreateInstance()
    {
        auto **instance = new (std::nothrow) Tracepoint *[MAX_MODULE_COUNT];
        if (instance == nullptr) {
            return nullptr;
        }

        for (int32_t i = 0; i < MAX_MODULE_COUNT; ++i) {
            instance[i] = nullptr;
        }

        int32_t ret = 0;
        uint16_t i = 0;
        for (i = 0; i < MAX_MODULE_COUNT; ++i) {
            instance[i] = new (std::nothrow) Tracepoint[MAX_TRACE_ID_COUNT];
            if (instance[i] == nullptr) {
                ret = -1;
                break;
            }
        }

        if (ret != 0) {
            for (uint16_t j = 0; j < i; ++j) {
                delete instance[j];
            }
            delete[] instance;
            return nullptr;
        }
        return instance;
    }

    static __always_inline uint32_t GetModuleId(uint32_t tpId)
    {
        return ((tpId >> 16) & 0xFFFF);
    }

    static __always_inline uint32_t GetTraceId(uint32_t tpId)
    {
        return (tpId & 0xFFFF);
    }
};
} // namespace tracer
} // namespace mf
} // namespace ock

#endif // MEM_FABRIC_PTRACER_TRACEPOINT_H
