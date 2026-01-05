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
#include "ptracer_default.h"
#include "ptracer_utils.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

namespace ock {
namespace mf {
namespace tracer {
thread_local std::string LastError::msg_;

DefaultTracer::~DefaultTracer()
{
    ShutDown();
}

int32_t DefaultTracer::StartUp(const std::string &dumpDir)
{
    /* get tracepoints which is to store tracepoints */
    auto tracePoints = TracepointCollection::GetTracepoints();
    if (tracePoints == nullptr) {
        LastError::Set("get tracepoints collection failed");
        return -1;
    }
    /* create the file where store the tracepoint info */
    if (PrepareDumpFile(dumpDir) != 0) {
        return -1;
    }
    /* start dump thread */
    StartDumpThread();
    return 0;
}

void DefaultTracer::ShutDown()
{
    if (dumpThread_.joinable()) {
        {
            std::unique_lock<std::mutex> lock(dumpLock_);
            running_ = false;
            dumpCond_.notify_all();
        }
        dumpThread_.join();
    }
}

void DefaultTracer::OverrideWrite(std::stringstream &ss)
{
    std::ifstream infile(dumpFilePath_);
    if (!infile) {
        return;
    }

    infile.seekg(0, std::ios_base::beg);
    std::stringstream outInfo;
    std::string line;
    if (writePos_ > 0) {
        auto bufferLen = writePos_ + 1;
        std::vector<char> readBuffer(bufferLen);
        infile.read(readBuffer.data(), bufferLen);
        outInfo.write(readBuffer.data(), static_cast<int64_t>(readBuffer.size()));
    }

    int32_t lineCount = 0;
    while (std::getline(ss, line)) {
        outInfo << line << std::endl;
        lineCount++;
    }

    writePos_ = outInfo.tellp();
    int32_t jumpCount = 0;
    while (std::getline(infile, line)) {
        if (++jumpCount < lineCount) {
            continue;
        }
        if (line == headline_) {
            outInfo << headline_ << std::endl;
            break;
        }
    }
    outInfo << infile.rdbuf();
    infile.close();

    std::ofstream outfile;
    outfile.open(dumpFilePath_, std::ios::out | std::ios::trunc);
    if (!outfile.is_open()) {
        return;
    }
    outfile << outInfo.str();
    outfile.flush();
    outfile.close();
}

void DefaultTracer::WriteTracepoints(std::stringstream &ss)
{
    size_t filesize = 0;
    struct stat statbuf{};
    if (stat(dumpFilePath_.c_str(), &statbuf) != 0) {
        if (errno != ENOENT) {
            return;
        }
        filesize = 0;
    } else {
        filesize = static_cast<size_t>(statbuf.st_size);
    }

    if (filesize + static_cast<size_t>(ss.tellp()) > MAX_DUMP_SIZE) {
        if (static_cast<size_t>(writePos_ + ss.tellp()) > MAX_DUMP_SIZE) {
            writePos_ = 0;
        }
        return OverrideWrite(ss);
    }

    auto mode = std::ios::out | std::ios::app;
    std::ofstream dump;
    dump.open(dumpFilePath_, mode);
    if (!dump.is_open()) {
        return;
    }

    dump << ss.str();
    writePos_ = dump.tellp();
    dump.flush();
    dump.close();
}

void DefaultTracer::GenerateOneTpString(Tracepoint &tp, bool needTotal, std::stringstream &outSS, int32_t &tpCount)
{
    if (!tp.Valid(needTotal)) {
        return;
    }

    if (needTotal) {
        outSS << Func::CurrentTimeString() << tp.ToTotalString() << std::endl;
    } else {
        outSS << Func::CurrentTimeString() << tp.ToPeriodString() << std::endl;
    }
    ++tpCount;
}

int32_t DefaultTracer::GenerateAllTpString(std::stringstream &ss, bool needTotal)
{
    auto tracePoints = TracepointCollection::GetTracepoints();
    if (tracePoints == nullptr) {
        return 0;
    }

    /* append headline */
    ss << Func::HeaderString() << std::endl;
    /* append all tracepoints one by one */
    int32_t tpCount = 0;
    for (int32_t i = 0; i < TracepointCollection::MAX_MODULE_COUNT; ++i) {
        for (int32_t j = 0; j < TracepointCollection::MAX_TRACE_ID_COUNT; ++j) {
            auto &tp = tracePoints[i][j];
            GenerateOneTpString(tp, needTotal, ss, tpCount);
        }
    }
    return tpCount;
}

void DefaultTracer::DumpTracepoints()
{
    std::stringstream ss;
    /* step1: generate string of all tracepoints */
    if (GenerateAllTpString(ss) <= 0) {
        return;
    }
    /* step2: append the string into file */
    WriteTracepoints(ss);
}

int32_t DefaultTracer::PrepareDumpFile(const std::string &dumpDir)
{
    if (dumpDir.empty()) {
        LastError::Set("create dump file failed as dumpDir is empty");
        return -1;
    }

    std::string dumpFileDir = dumpDir;
    if (dumpFileDir.back() != '/') {
        dumpFileDir += "/";
    }

    /* create dir if not exists */
    int32_t ret = Func::MakeDir(dumpFileDir);
    if (ret != 0) {
        LastError::Set("create dir @" + dumpFileDir + " failed, errno " + std::to_string(errno));
        return -1;
    }

    dumpFilePath_ = dumpFileDir + "ptracer_" + std::to_string(getpid()) + ".dat";
    /* create the dump file if not exists */
    int32_t fd = open(dumpFilePath_.c_str(), O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
    if (fd < 0) {
        LastError::Set("create or open file @" + dumpFilePath_ + " failed, errno " + std::to_string(errno));
        return -1;
    }
    /* close the file description, use file stream to do file operation */
    close(fd);
    return 0;
}

void DefaultTracer::StartDumpThread()
{
    std::unique_lock<std::mutex> lock(dumpLock_);
    if (running_) {
        return;
    }

    running_ = true;
    dumpThread_ = std::thread(&DefaultTracer::RunInThread, this);
    pthread_setname_np(dumpThread_.native_handle(), "ptracer_dump");
}

void DefaultTracer::RunInThread()
{
    std::unique_lock<std::mutex> lock(dumpLock_);
    while (running_) {
        /* wait for some time if no one signal the condition */
        dumpCond_.wait_for(lock, std::chrono::seconds(DUMP_PERIOD_SECOND));
        /* dump tracepoints into the file */
        DumpTracepoints();
    }
}
}  // namespace tracer
}  // namespace mf
}  // namespace ock