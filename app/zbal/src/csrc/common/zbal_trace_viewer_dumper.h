/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef ZBAL_TRACE_VIEWER_DUMPER_H
#define ZBAL_TRACE_VIEWER_DUMPER_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#include "zbal_defines.h"
#include "zbal_functions.h"
#include "zbal_logger.h"

namespace zbal {
static const char *TRACE_VIEW_FILE_START = "[";
static const char *TRACE_VIEW_FILE_END = "\n]";

enum TraceElementPhase {
    TEP_BEGIN = 0,
    TEP_END,
    TEP_X,

    TEP_BUTT
};

static const char TRACE_ELEMENT_PHASE_STR[TEP_BUTT] = {'B', 'E', 'X'};
struct TraceElement {
    const char *name = nullptr;      /* name of trace line */
    const char *pid = nullptr;       /* pid of trace line */
    const char *tid = nullptr;       /* tid of trace line */
    TraceElementPhase phase = TEP_X; /* phase of trace line */
    uint64_t ts = 0;                 /* timestamp of trace line */
    uint64_t duration = 0;           /* duration of trace line */
    std::vector<uint64_t> attrs;     /* extra attributes */

    TraceElement(const char *n, const char *p, const char *t, uint64_t timestamp, uint64_t d)
        : name(n), pid(p), tid(t), ts(timestamp), duration(d)
    {}

    TraceElement(const char *n, const char *p, const char *t, const TraceElementPhase &ph, uint64_t timestamp)
        : name(n), pid(p), tid(t), phase(ph), ts(timestamp)
    {}

    void Append(uint64_t attr)
    {
        attrs.push_back(attr);
    }

    friend std::ostream &operator<<(std::ostream &stream, const TraceElement &element);
};

class TraceViewerFormatFileWriter {
public:
    TraceViewerFormatFileWriter(const std::string &dir, const std::string &filename, const std::string &pid)
        : dir_(dir), filename_(filename), pid_(pid)
    {}

    ~TraceViewerFormatFileWriter()
    {
        Close();
    }

    /**
     * @brief initialize writer, within 2 things done:
     * 1. open the file
     * 2. write header
     *
     * @return 0 if successful
     */
    ZResult Open() noexcept;

    /**
     * @brief Un-initialize, within 2 things done:
     * 1. write end
     * 2. close the file
     */
    void Close() noexcept;

    /**
     * @brief Append one trace element
     *
     * @param element      [in] the element to append
     *
     * @return 0 if successful
     */
    ZResult Append(const TraceElement &element) noexcept;

    /**
     * @brief Append one trace with duration
     *
     * @param name         [in] name of this trace
     * @param tid          [in] tid of this trace
     * @param timestamp    [in] timestamp of this trace, i.e. begin
     * @param duration     [in] duration of this trace, end will be calculated by begin and this
     * @return 0 if successful
     */
    ZResult AppendDuration(const char *name, const char *tid, uint64_t timestamp, uint64_t duration);

    /**
     * @brief Append one trace with begin
     *
     * @param name         [in] name of this trace
     * @param tid          [in] tid of this trace
     * @param timestamp    [in] timestamp of this trace, i.e. begin
     * @return 0 if successful
     */
    ZResult AppendBegin(const char *name, const char *tid, uint64_t timestamp);

    /**
     * @brief Append one trace with end
     *
     * @param name         [in] name of this trace
     * @param tid          [in] tid of this trace
     * @param timestamp    [in] timestamp of this trace, i.e. begin
     * @return 0 if successful
     */
    ZResult AppendEnd(const char *name, const char *tid, uint64_t timestamp);

private:
    ZResult CreateFileWithBegin() noexcept;
    ZResult CloseFileWithEnd() noexcept;

private:
    const std::string dir_;
    const std::string filename_;
    const std::string pid_;
    const uint32_t bufferSize_ = 8192;
    std::ostringstream buffer_;
    std::ofstream fileStream_;
    bool inited_ = false;
    bool appendComma_ = false;
};

ALWAYS_INLINE ZResult TraceViewerFormatFileWriter::Open() noexcept
{
    /* already initialized */
    if (inited_) {
        return Z_OK;
    }

    if (dir_.empty()) {
        ZBAL_LOG_ERROR("Dir path @ " << dir_ << " is empty");
        return Z_INVALID_PARAM;
    }

    if (!Func::Exist(dir_)) {
        ZBAL_LOG_ERROR("Dir path @ " << dir_ << " not exists");
        return Z_INVALID_PARAM;
    }

    if (!Func::IsDir(dir_)) {
        ZBAL_LOG_ERROR("Dir path @ " << dir_ << " is not a directory");
        return Z_INVALID_PARAM;
    }

    if (!Func::ReadAndWritable(dir_)) {
        ZBAL_LOG_ERROR("Dir path @ " << dir_ << " is not readable or not writable");
        return Z_INVALID_PARAM;
    }

    /* create file and append header */
    auto result = CreateFileWithBegin();
    if (result != Z_OK) {
        ZBAL_LOG_ERROR("Dir path @ " << dir_ << " create file failed.");
        return Z_INVALID_PARAM;
    }

    inited_ = true;
    return Z_OK;
}

ALWAYS_INLINE void TraceViewerFormatFileWriter::Close() noexcept
{
    if (!inited_) {
        return;
    }

    CloseFileWithEnd();

    inited_ = false;
}

ALWAYS_INLINE ZResult TraceViewerFormatFileWriter::CreateFileWithBegin() noexcept
{
    auto fullpath = dir_ + "/" + filename_;

    fileStream_.open(fullpath, std::ios::out);
    if (!fileStream_.is_open()) {
        ZBAL_LOG_ERROR("Create file @ " << fullpath << " failed");
        return Z_OPEN_FILE_FAILED;
    }

    fileStream_.write(TRACE_VIEW_FILE_START, strlen(TRACE_VIEW_FILE_START));

    return Z_OK;
}

ALWAYS_INLINE ZResult TraceViewerFormatFileWriter::CloseFileWithEnd() noexcept
{
    /* flush there is data in buffer */
    auto str = buffer_.str();
    if (str.length() > 0) {
        /* write file and flush */
        fileStream_.write(str.c_str(), str.length());
        fileStream_.flush();

        /* reset buffer */
        buffer_ = std::ostringstream();
    }

    /* append end and flush file */
    fileStream_.write(TRACE_VIEW_FILE_END, strlen(TRACE_VIEW_FILE_END));
    fileStream_.flush();
    fileStream_.close();

    return Z_OK;
}

ALWAYS_INLINE ZResult TraceViewerFormatFileWriter::Append(const TraceElement &element) noexcept
{
    if (!fileStream_.is_open()) {
        ZBAL_LOG_ERROR("Write file @ " << dir_ << "/" << filename_ << " failed as file is not opened");
        return Z_OPEN_FILE_FAILED;
    }

    /* don't append ',' for first line, other line we need to append ',' */
    if (!appendComma_) {
        appendComma_ = true;
        buffer_ << "\n";
    } else {
        buffer_ << ",\n";
    }
    buffer_ << element;

    auto str = buffer_.str();
    if (str.length() < bufferSize_) {
        return Z_OK;
    }

    /* write file and flush */
    fileStream_.write(str.c_str(), str.length());
    fileStream_.flush();

    /* reset buffer */
    buffer_ = std::ostringstream();

    return Z_OK;
}

ALWAYS_INLINE ZResult TraceViewerFormatFileWriter::AppendDuration(const char *name, const char *tid, uint64_t timestamp,
                                                                  uint64_t duration)
{
    return Append(TraceElement(name, pid_.c_str(), tid, timestamp, duration));
}

ALWAYS_INLINE ZResult TraceViewerFormatFileWriter::AppendBegin(const char *name, const char *tid, uint64_t timestamp)
{
    return Append(TraceElement(name, pid_.c_str(), tid, TEP_BEGIN, timestamp));
}

ALWAYS_INLINE ZResult TraceViewerFormatFileWriter::AppendEnd(const char *name, const char *tid, uint64_t timestamp)
{
    return Append(TraceElement(name, pid_.c_str(), tid, TEP_END, timestamp));
}

ALWAYS_INLINE std::ostream &operator<<(std::ostream &stream, const TraceElement &element)
{
    ZBAL_ASSERT(element.name != nullptr);
    ZBAL_ASSERT(element.pid != nullptr);
    ZBAL_ASSERT(element.tid != nullptr);
    ZBAL_ASSERT(element.phase < TEP_BUTT);

    if (element.phase == TEP_X) {
        // {"name": "a", "ph": "X", "pid": "Main", "tid": "tag", "ts": 0, "dur": 28800000000},
        stream << "{\"name\":\"" << element.name << "\", \"ph\":\"X\", \"pid\":\"" << element.pid << "\", \"tid\":\""
               << element.tid << "\", \"ts\":" << element.ts << ", \"dur\":" << element.duration << "}";
    } else if (element.phase == TEP_BEGIN || element.phase == TEP_END) {
        // {"name": "a", "ph": "E", "pid": "Main", "tid": "tag", "ts": 0},
        // {"name": "a", "ph": "E", "pid": "Main", "tid": "tag", "ts": 0},
        stream << "{\"name\":\"" << element.name << "\", \"ph\":\"" << TRACE_ELEMENT_PHASE_STR[element.phase]
               << "\", \"pid\":\"" << element.pid << "\", \"tid\":\"" << element.tid << "\", \"ts\":" << element.ts;

        if (element.attrs.size() > 0) {
            stream << ", \"args\":{";
            for (size_t i = 0; i < element.attrs.size(); ++i) {
                std::string key = i == 0 ? "LineNo" : "attr_" + std::to_string(i);
                stream << "\"" << key << "\":" << element.attrs[i];
                if (i != element.attrs.size() - 1) {
                    stream << ", ";
                }
            }
            stream << "}";
        }
        stream << "}";
    } else {
        ZBAL_LOG_WARN("Un-supported phase");
        return stream;
    }

    return stream;
}
} // namespace zbal

#endif // ZBAL_TRACE_VIEWER_DUMPER_H