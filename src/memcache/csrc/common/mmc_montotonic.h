/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Author: bao
 */
#ifndef OCK_HCOM_NET_MONOTONIC_H
#define OCK_HCOM_NET_MONOTONIC_H

#include <cstdio>
#include <fstream>
#include <string>
#ifdef __x86_64__
#include <cmath>
#include <vector>
#include <x86intrin.h>
#endif

namespace ock {
    namespace dagger {
        class Monotonic {
#ifdef USE_PROCESS_MONOTONIC
        public:
#ifdef __aarch64__
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
        return timeValue / (TICK_PER_US * 100000L);
    }

#elif __x86_64__
    template <int32_t FAILURE_RET> static int32_t InitTickUs()
    {
        const std::string path = "/proc/cpuinfo";
        const std::string prefix = "model name";
        const std::string gHZ = "GHz";

        std::ifstream inConfFile(path);
        if (!inConfFile) {
            printf("Failed to get tick as failed to open %s\n", path.c_str());
            return FAILURE_RET;
        }

        bool found = false;
        std::string strLine;
        while (getline(inConfFile, strLine)) {
            if (strLine.compare(0, prefix.size(), prefix) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            printf("Failed to get tick as failed to find %s\n", prefix.c_str());
            return FAILURE_RET;
        }

        std::vector<std::string> splitVec;
        NN_SplitStr(strLine, " ", splitVec);
        if (splitVec.empty()) {
            ;
            printf("Failed to get tick as failed to get line %s\n", prefix.c_str());
            return FAILURE_RET;
        }

        std::string lastWord = splitVec[splitVec.size() - 1];
        auto index = lastWord.find(gHZ);
        if (index == std::string::npos) {
            printf("Failed to get tick as failed to get %s\n", gHZ.c_str());
            return FAILURE_RET;
        }

        auto strGhz = lastWord.substr(0, index);
        float fhz = 0.0f;
        if (!NN_Stof(strGhz, fhz)) {
            printf("Failed to get tick as failed to convert %s to float\n", strGhz.c_str());
            return FAILURE_RET;
        }

        return static_cast<int32_t>(fhz * 1000L);
    }

    /*
     * @brief Get monotonic time in us, is not absolution time
     */
    static inline uint64_t TimeUs()
    {
        const static int32_t TICK_PER_US = InitTickUs<1>();
        return __rdtsc() / TICK_PER_US;
    }

    /*
     * @brief Get monotonic time in ns, is not absolution time
     */
    static inline uint64_t TimeNs()
    {
        const static int32_t TICK_PER_US = InitTickUs<1>();
        return __rdtsc() * 1000L / TICK_PER_US;
    }

    /*
     * @brief Get monotonic time in sec, is not absolution time
     */
    static inline uint64_t TimeSec()
    {
        const static int32_t TICK_PER_US = InitTickUs<1>();
        return __rdtsc() / (TICK_PER_US * 1000000L);
    }

#endif /* __x86_64__ || __aarch64__ */

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
#ifdef __x86_64__
        private:
    /* NN_SplitStr */
    static void NN_SplitStr(const std::string &str, const std::string &separator, std::vector<std::string> &result)
    {
        result.clear();
        std::string::size_type pos1 = 0;
        std::string::size_type pos2 = str.find(separator);

        std::string tmpStr;
        while (pos2 != std::string::npos) {
            tmpStr = str.substr(pos1, pos2 - pos1);
            result.emplace_back(tmpStr);
            pos1 = pos2 + separator.size();
            pos2 = str.find(separator, pos1);
        }

        if (pos1 != str.length()) {
            tmpStr = str.substr(pos1);
            result.emplace_back(tmpStr);
        }
    }

    static bool NN_Stof(const std::string &str, float &value)
    {
        constexpr float EPSINON = 0.000001;
        char *remain = nullptr;
        errno = 0;
        value = std::strtof(str.c_str(), &remain);
        if (remain == nullptr || strlen(remain) > 0 ||
            ((value - HUGE_VALF) >= -EPSINON && (value - HUGE_VALF) <= EPSINON && errno == ERANGE)) {
            return false;
        } else if ((value >= -EPSINON && value <= EPSINON) && (str != "0.0")) {
            return false;
        }
        return true;
    }
#endif
        };
    }
}

#endif // OCK_HCOM_NET_MONOTONIC_H
