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

#ifndef SMEM_SMEM_MESSAGE_PACKER_H
#define SMEM_SMEM_MESSAGE_PACKER_H

#include <cstdint>
#include <string>
#include <vector>

namespace ock {
namespace smem {

constexpr const char *WATCH_RANK_DOWN_KEY = "WATCH_RANK_DOWN_KEY";
#ifdef UT_ENABLED
constexpr uint32_t HEARTBEAT_INTERVAL = 100; // ms
#else
constexpr uint32_t HEARTBEAT_INTERVAL = 2000; // ms
#endif
const uint64_t MAX_KEY_COUNT = 10ULL;
const uint64_t MAX_KEY_SIZE = 2048ULL;
const uint64_t MAX_VALUE_COUNT = 10ULL;
const uint64_t MAX_VALUE_SIZE = 64 * 1024 * 1024ULL;
enum MessageType : int16_t { SET, GET, ADD, REMOVE, APPEND, CAS, WRITE, WATCH_RANK_STATE, HEARTBEAT, INVALID_MSG };

struct SmemMessage {
    SmemMessage() noexcept : mt{MessageType::INVALID_MSG} {}

    explicit SmemMessage(MessageType type) noexcept : mt{type} {}

    SmemMessage(MessageType type, std::string k) noexcept : mt{type}
    {
        keys.emplace_back(std::move(k));
    }

    SmemMessage(MessageType type, std::vector<uint8_t> v) noexcept : mt{type}
    {
        values.emplace_back(std::move(v));
    }

    SmemMessage(MessageType type, std::string k, std::vector<uint8_t> v) noexcept : mt{type}
    {
        keys.emplace_back(std::move(k));
        values.emplace_back(std::move(v));
    }

    SmemMessage(MessageType type, std::string k, std::vector<uint8_t> v, std::vector<uint8_t> vv) noexcept : mt{type}
    {
        keys.emplace_back(std::move(k));
        values.emplace_back(std::move(v));
        values.emplace_back(std::move(vv));
    }

    SmemMessage(MessageType type, std::vector<std::string> ks) noexcept : mt{type}, keys{std::move(ks)} {}

    SmemMessage(MessageType type, std::vector<std::string> ks, int64_t value) noexcept : mt{type}, keys{std::move(ks)}
    {
        values.emplace_back(reinterpret_cast<const uint8_t *>(&value),
                            reinterpret_cast<const uint8_t *>(&value) + sizeof(int64_t));
    }

    SmemMessage(MessageType type, std::vector<std::vector<uint8_t>> vs) noexcept : mt{type}, values{std::move(vs)} {}

    MessageType mt;
    int64_t userDef{-1L};
    std::vector<std::string> keys;
    std::vector<std::vector<uint8_t>> values;
};

class SmemMessagePacker {
public:
    static std::vector<uint8_t> Pack(const SmemMessage &message) noexcept;

    static bool Full(const uint8_t* buffer, const uint64_t bufferLen) noexcept;

    static int64_t MessageSize(const std::vector<uint8_t> &buffer) noexcept;

    static int64_t Unpack(const uint8_t* buffer, const uint64_t bufferLen, SmemMessage &message) noexcept;

    template <class T>
    static std::vector<uint8_t> PackPod(const T &v) noexcept
    {
        auto begin = reinterpret_cast<const uint8_t *>(&v);
        return std::vector<uint8_t>{begin, begin + sizeof(T)};
    }

    template <class T>
    static T UnpackPod(const std::vector<uint8_t> &vec) noexcept
    {
        return *reinterpret_cast<const T *>(vec.data());
    }

private:
    template <class T>
    static void PackValue(std::vector<uint8_t> &dest, T value) noexcept
    {
        dest.insert(dest.end(), reinterpret_cast<const uint8_t *>(&value),
                    reinterpret_cast<const uint8_t *>(&value) + sizeof(T));
    }

    static void PackString(std::vector<uint8_t> &dest, const std::string &str) noexcept;

    static void PackBytes(std::vector<uint8_t> &dest, const std::vector<uint8_t> &bytes) noexcept;
};

}  // ock
}  // smem

#endif  // SMEM_SMEM_MESSAGE_PACKER_H
