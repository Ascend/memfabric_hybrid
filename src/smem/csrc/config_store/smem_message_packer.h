/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef SMEM_SMEM_MESSAGE_PACKER_H
#define SMEM_SMEM_MESSAGE_PACKER_H

#include <cstdint>
#include <string>
#include <vector>

namespace ock {
namespace smem {
const uint64_t MAX_KEY_COUNT = 10ULL;
const uint64_t MAX_KEY_SIZE = 2048ULL;
const uint64_t MAX_VALUE_COUNT = 10ULL;
const uint64_t MAX_VALUE_SIZE = 64 * 1024 * 1024ULL;
enum MessageType : int16_t { SET, GET, ADD, REMOVE, APPEND, CAS, INVALID_MSG };

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
