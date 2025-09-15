/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_PTRACE_UTILS_H
#define MEM_FABRIC_PTRACE_UTILS_H

#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ock {
namespace mf {
namespace tracer {

#ifndef PTRACER_UNLIKELY
#define PTRACER_UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

/* @brief Tool to get monotonic time in us, is not absolution time */
class Monotonic {
public:
    static inline uint64_t TimeNs();

private:
    static uint64_t InitTickUs();
};

/* @brief Functions */
class Func {
public:
    static int32_t MakeDir(const std::string &name);
    static std::string CurrentTimeString();
    static std::string HeaderString();
    static std::string FormatString(
        std::string &name, uint64_t begin, uint64_t goodEnd, uint64_t badEnd, uint64_t min, uint64_t max,
        uint64_t total);

private:
    static void StrSplit(const std::string &src, const std::string &sep, std::vector<std::string> &out);
};

/* @brief Tool to store and get last error with thread local variable */
class LastError {
public:
    static void Set(const std::string &msg);
    static const char *Get();

private:
    static thread_local std::string msg_;
};

/*** for Monotonic ***/
inline uint64_t Monotonic::InitTickUs()
{
#if defined(ENABLE_CPU_MONOTONIC) && defined(__aarch64__)
    uint64_t freq = 0;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    freq = freq / 1000ULL / 1000ULL;
    if (freq == 0) {
        printf("Failed to get tick as freq is %lu\n", freq);
        return 1;
    }
    return freq;
#else
    return 0;
#endif
}

inline uint64_t Monotonic::TimeNs()
{
#if defined(ENABLE_CPU_MONOTONIC) && defined(__aarch64__)
    const static uint64_t TICK_PER_US = InitTickUs();
    uint64_t timeValue = 0;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(timeValue));
    return timeValue * 1000ULL / TICK_PER_US;
#else
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000UL +  static_cast<uint64_t>(ts.tv_nsec);
#endif
}

/*** functions for Func ***/
constexpr int32_t TIME_WIDTH = 23;
constexpr int32_t NAME_WIDTH = 35;
constexpr int32_t DIGIT_WIDTH = 15;
constexpr int32_t UNIT_STEP = 1000;
constexpr double IOPS_DIFF = 90.000;
constexpr int32_t NUMBER_PRECISION = 3;
constexpr int32_t WIDTH_FOUR = 4;
constexpr int32_t WIDTH_TWO = 2;
constexpr mode_t DEFAULT_DIR_MODE = 0750;
constexpr int32_t BASE_YEAR_1900 = 1900;

inline int32_t Func::MakeDir(const std::string &name)
{
    std::vector<std::string> paths;
    StrSplit(name, "/", paths);
    int32_t ret = 0;
    std::string pathTmp;
    for (auto &item : paths) {
        if (item.empty()) {
            continue;
        }

        pathTmp += "/" + item;
        if (access(pathTmp.c_str(), F_OK) != 0) {
            ret = mkdir(pathTmp.c_str(), DEFAULT_DIR_MODE);
            if (ret != 0 && errno != EEXIST) {
                break;
            }
        }
    }
    return ret;
}

inline std::string Func::CurrentTimeString()
{
    time_t rawTime;
    (void)time(&rawTime);
    auto tmInfo = localtime(&rawTime);
    if (tmInfo == nullptr) {
        return "";
    }
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(WIDTH_FOUR) << std::right << (tmInfo->tm_year + BASE_YEAR_1900) << "-"
       << std::setfill('0') << std::setw(WIDTH_TWO) << std::right << (tmInfo->tm_mon + 1) << "-" << std::setfill('0')
       << std::setw(WIDTH_TWO) << std::right << tmInfo->tm_mday << " ";

    ss << std::setfill('0') << std::setw(WIDTH_TWO) << std::right << tmInfo->tm_hour << ":" << std::setfill('0')
       << std::setw(WIDTH_TWO) << std::right << tmInfo->tm_min << ":" << std::setfill('0') << std::setw(WIDTH_TWO)
       << std::right << tmInfo->tm_sec << std::setfill(' ') << std::setw(WIDTH_FOUR) << std::right << " ";
    return ss.str();
}

inline std::string Func::FormatString(
    std::string &name, uint64_t begin, uint64_t goodEnd, uint64_t badEnd, uint64_t min, uint64_t max, uint64_t total)
{
    std::string str;
    std::ostringstream os(str);
    os.flags(std::ios::fixed);
    os.precision(NUMBER_PRECISION);
    os << std::left << std::setw(NAME_WIDTH) << name << std::left << std::setw(DIGIT_WIDTH) << begin << std::left
       << std::setw(DIGIT_WIDTH) << goodEnd << std::left << std::setw(DIGIT_WIDTH) << badEnd << std::left
       << std::setw(DIGIT_WIDTH) << ((begin > goodEnd + badEnd) ? (begin - goodEnd - badEnd) : 0) << std::left
       << std::setw(DIGIT_WIDTH) << (static_cast<double>(begin) / IOPS_DIFF) << std::left << std::setw(DIGIT_WIDTH)
       << (min == UINT64_MAX ? 0 : (static_cast<double>(min) / UNIT_STEP)) << std::left << std::setw(DIGIT_WIDTH)
       << static_cast<double>(max) / UNIT_STEP << std::left << std::setw(DIGIT_WIDTH)
       << (goodEnd == 0 ? 0 : static_cast<double>(total) / static_cast<double>(goodEnd) / UNIT_STEP) << std::left
       << std::setw(DIGIT_WIDTH) << static_cast<double>(total) / UNIT_STEP;
    return os.str();
}

inline std::string Func::HeaderString()
{
    std::stringstream ss;
    ss << std::left << std::setw(TIME_WIDTH) << "TIME" << std::left << std::setw(NAME_WIDTH) << "NAME" << std::left
       << std::setw(DIGIT_WIDTH) << "BEGIN" << std::left << std::setw(DIGIT_WIDTH) << "GOOD_END" << std::left
       << std::setw(DIGIT_WIDTH) << "BAD_END" << std::left << std::setw(DIGIT_WIDTH) << "ON_FLY" << std::left
       << std::setw(DIGIT_WIDTH) << "IOPS" << std::left << std::setw(DIGIT_WIDTH) << "MIN(us)" << std::left
       << std::setw(DIGIT_WIDTH) << "MAX(us)" << std::left << std::setw(DIGIT_WIDTH) << "AVG(us)" << std::left
       << std::setw(DIGIT_WIDTH) << "TOTAL(us)";
    return ss.str();
}

inline void Func::StrSplit(const std::string &src, const std::string &sep, std::vector<std::string> &out)
{
    std::string::size_type pos1 = 0;
    std::string::size_type pos2 = src.find(sep);

    std::string tmpStr;
    while (pos2 != std::string::npos) {
        tmpStr = src.substr(pos1, pos2 - pos1);
        out.emplace_back(tmpStr);
        pos1 = pos2 + sep.size();
        pos2 = src.find(sep, pos1);
    }

    if (pos1 != src.length()) {
        tmpStr = src.substr(pos1);
        out.emplace_back(tmpStr);
    }
}

/*** for ptracer last error ***/
inline void LastError::Set(const std::string &msg)
{
    msg_ = msg;
}

inline const char *LastError::Get()
{
    return msg_.c_str();
}
}  // namespace tracer
}  // namespace mf
}  // namespace ock
#endif  // MEM_FABRIC_PTRACE_UTILS_H