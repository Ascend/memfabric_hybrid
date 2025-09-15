/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_PTRACER_DEFAULT_H
#define MEM_FABRIC_PTRACER_DEFAULT_H

#include <condition_variable>
#include <mutex>
#include <sstream>
#include <thread>

#include "ptracer.h"
#include "ptracer_tracepoint.h"

namespace ock {
namespace mf {
namespace tracer {
/**
 * @brief A tracer with dump thread thread which periodically dumps tracepoints info into the file,
 * then view the tracepoints info by 'cat filename'
 */
class DefaultTracer {
public:
    static void TraceBegin(uint32_t tpId, const char *tpName)
    {
        TracepointCollection::TraceBegin(tpId, tpName);
    }

    static void TraceEnd(uint32_t tpId, uint64_t ns, int32_t goodBad)
    {
        TracepointCollection::TraceEnd(tpId, ns, goodBad);
    }

    static uint64_t TimeNs()
    {
        return Monotonic::TimeNs();
    }

public:
    static DefaultTracer &GetInstance()
    {
        static DefaultTracer tracer;
        return tracer;
    }

    int32_t StartUp(const std::string &dumpDir);
    void ShutDown();

public:
    DefaultTracer(const DefaultTracer &) = delete;
    DefaultTracer &operator=(const DefaultTracer &) = delete;
    DefaultTracer(DefaultTracer &&) = delete;
    DefaultTracer &operator=(DefaultTracer &&) = delete;

private:
    int PrepareDumpFile(const std::string &dumpDir);
    void StartDumpThread();
    void RunInThread();
    void DumpTracepoints();
    int GenerateAllTpString(std::stringstream &ss, bool needTotal = false);
    void GenerateOneTpString(Tracepoint &tp, bool needTotal, std::stringstream &outSS, int32_t &tpCount);
    void WriteTracepoints(std::stringstream &ss);
    void OverrideWrite(std::stringstream &ss);

    DefaultTracer() = default;
    ~DefaultTracer();

private:
    off_t writePos_{0};
    std::string dumpFilePath_{};
    std::string headline_{};
    std::mutex dumpLock_{};
    bool running_{false};
    std::thread dumpThread_{};
    std::condition_variable dumpCond_{};

private:
    int32_t DUMP_PERIOD_SECOND = 10;         /* dump every 10 seconds */
    size_t MAX_DUMP_SIZE = 10 * 1024 * 1024; /* 10MB */
};
}  // namespace tracer
}  // namespace mf
}  // namespace ock

#endif  // MEM_FABRIC_PTRACER_DEFAULT_H
