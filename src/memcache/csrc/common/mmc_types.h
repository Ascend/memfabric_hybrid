/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MMC_TYPES_H
#define MEMFABRIC_HYBRID_MMC_TYPES_H

#include <cstdint>
#include <atomic>
#include <iostream>
#include <vector>

#include <mmc_def.h>

namespace ock {
namespace mmc {
using Result = int32_t;

enum MmcErrorCode : int32_t {
    MMC_OK = 0,
    MMC_ERROR = -1,
    MMC_INVALID_PARAM = -3000,
    MMC_MALLOC_FAILED = -3001,
    MMC_NEW_OBJECT_FAILED = -3002,
    MMC_NOT_STARTED = -3003,
    MMC_TIMEOUT = -3004,
    MMC_REPEAT_CALL = -3005,
    MMC_DUPLICATED_OBJECT = -3006,
    MMC_OBJECT_NOT_EXISTS = -3007,
    MMC_NOT_INITIALIZED = -3008,
    MMC_NET_SEQ_DUP = -3009,
    MMC_NET_SEQ_NO_FOUND = -3010,
    MMC_ALREADY_NOTIFIED = -3011,
    MMC_EXCEED_CAPACITY = -3013,
    MMC_LINK_NOT_FOUND = -3014,
    MMC_NET_REQ_HANDLE_NO_FOUND = -3015,
    MMC_NOT_ENOUGH_MEMORY = -3016,
    MMC_NOT_CONNET_META = -3017,
    MMC_NOT_CONNET_LOCAL = -3018,
    MMC_CLIENT_NOT_INIT = -3019,
    MMC_UNMATCHED_STATE = -3101,
    MMC_UNMATCHED_KEY = -3102,
    MMC_UNMATCHED_RET = -3103,
    MMC_LEASE_NOT_EXPIRED = -3104,
    MMC_META_BACKUP_ERROR = -3105,
};

inline std::ostream& operator<<(std::ostream& os, MmcErrorCode errCode)
{
    os << std::to_string(static_cast<int32_t>(errCode));
    return os;
}

constexpr int32_t N16 = 16;
constexpr int32_t N64 = 64;
constexpr int32_t N256 = 256;
constexpr int32_t N1024 = 1024;
constexpr int32_t N8192 = 8192;

constexpr uint32_t UN2 = 2;
constexpr uint32_t UN16 = 16;
constexpr uint32_t UN32 = 32;
constexpr uint32_t UN58 = 58;
constexpr uint32_t UN128 = 128;
constexpr uint32_t UN65536 = 65536;
constexpr uint32_t UN16777216 = 16777216;

constexpr uint32_t MMC_DEFAUT_WAIT_TIME = 120;  // 120s

enum MediaType : uint8_t {
    MEDIA_HBM,
    MEDIA_DRAM,
    MEDIA_NONE,
};

inline MediaType MoveUp(MediaType mediaType)
{
    if (mediaType == MediaType::MEDIA_HBM) {
        return MediaType::MEDIA_NONE;
    } else if (mediaType == MediaType::MEDIA_DRAM) {
        return MediaType::MEDIA_HBM;
    } else {
        return MediaType::MEDIA_NONE;
    }
}

inline MediaType MoveDown(MediaType mediaType)
{
    if (mediaType == MediaType::MEDIA_HBM) {
        return MediaType::MEDIA_DRAM;
    } else if (mediaType == MediaType::MEDIA_DRAM) {
        return MediaType::MEDIA_NONE;
    } else {
        return MediaType::MEDIA_NONE;
    }
}

inline std::ostream& operator<<(std::ostream& os, MediaType type)
{
    switch (type) {
        case MEDIA_DRAM: os << "MEDIA_DRAM"; break;
        case MEDIA_HBM: os << "MEDIA_HBM"; break;
        default: os << "UNKNOWN"; break;
    }
    return os;
}

enum class EvictResult {
    REMOVE,    // 直接删除
    MOVE_DOWN, // 向下层移动
    FAIL,      // 淘汰失败
};

struct MmcLocation {
    uint32_t rank_;
    MediaType mediaType_;
    MmcLocation() : rank_(UINT32_MAX), mediaType_(MediaType::MEDIA_NONE) {}
    MmcLocation(uint32_t rank, MediaType mediaType) : rank_(rank), mediaType_(mediaType) {}

    bool operator<(const MmcLocation &other) const
    {
        // 先比较 mediaType_
        if (mediaType_ != other.mediaType_) {
            return mediaType_ < other.mediaType_;
        }
        // mediaType_ 相等时比较 rank_
        return rank_ < other.rank_;
    }

    bool operator==(const MmcLocation &other) const
    {
        return rank_ == other.rank_ && mediaType_ == other.mediaType_;
    }

    friend std::ostream& operator<<(std::ostream& os, const MmcLocation& loc)
    {
        os << "loc{rank=" << loc.rank_ << ",media=" << loc.mediaType_ << "}";
        return os;
    }
};

struct MmcLocalMemlInitInfo {
    uint64_t bmAddr_;
    uint64_t capacity_;
};

union MmcOperateIdUnion {
    uint64_t operateId_;
    struct {
        uint32_t sequence_;
        uint32_t rankid_;
    };
};

inline uint64_t GenerateOperateId(uint32_t rankid)
{
    static std::atomic<uint32_t> gRequestIdGenerator{0U};
    MmcOperateIdUnion requestUnion{};
    requestUnion.rankid_ = rankid;
    requestUnion.sequence_ = gRequestIdGenerator.fetch_add(1U);
    return requestUnion.operateId_;
}

inline uint64_t GetRankIdByOperateId(uint64_t operateId)
{
    MmcOperateIdUnion requestUnion{};
    requestUnion.operateId_ = operateId;
    return requestUnion.rankid_;
}

inline uint64_t GetSequenceByOperateId(uint64_t operateId)
{
    MmcOperateIdUnion requestUnion{};
    requestUnion.operateId_ = operateId;
    return requestUnion.sequence_;
}

inline uint64_t MmcBufSize(const mmc_buffer& buf)
{
    return buf.len;
}

class MmcBufferArray {
public:
    MmcBufferArray() : totalSize_(0) {}

    explicit MmcBufferArray(const std::vector<mmc_buffer>& buffers) : buffers_(buffers)
    {
        totalSize_ = 0;
        for (const auto& buf : buffers_) {
            totalSize_ += MmcBufSize(buf);
        }
    }

    void AddBuffer(const mmc_buffer& buf)
    {
        buffers_.push_back(buf);
        totalSize_ += MmcBufSize(buf);
    }

    const std::vector<mmc_buffer>& Buffers() const { return buffers_; }
    size_t TotalSize() const { return totalSize_; }

private:
    std::vector<mmc_buffer> buffers_{};
    size_t totalSize_{0};
};

}  // namespace mmc
}  // namespace ock

#endif  // MEMFABRIC_HYBRID_MMC_TYPES_H
