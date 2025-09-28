/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include "smem_message_packer.h"

namespace ock {
namespace smem {
std::vector<uint8_t> SmemMessagePacker::Pack(const SmemMessage &message) noexcept
{
    // size + userDef + mt + keyN + vN
    constexpr uint64_t baseSize = 4U * sizeof(uint64_t) + sizeof(MessageType);
    uint64_t totalSize = baseSize;
    for (auto &key : message.keys) {
        totalSize += (sizeof(uint64_t) + key.size());
    }
    for (auto &value : message.values) {
        totalSize += (sizeof(uint64_t) + value.size());
    }

    std::vector<uint8_t> result;
    result.reserve(totalSize);
    PackValue(result, totalSize);
    PackValue(result, message.userDef);
    PackValue(result, message.mt);

    PackValue(result, message.keys.size());
    for (auto &key : message.keys) {
        PackString(result, key);
    }

    PackValue(result, message.values.size());
    for (auto &value : message.values) {
        PackBytes(result, value);
    }

    return result;
}

bool SmemMessagePacker::Full(const std::vector<uint8_t> &buffer) noexcept
{
    if (buffer.size() < 4U * sizeof(uint64_t) + sizeof(MessageType)) {
        return false;
    }

    auto totalSize = *reinterpret_cast<const uint64_t *>(buffer.data());
    return buffer.size() >= totalSize;
}

int64_t SmemMessagePacker::MessageSize(const std::vector<uint8_t> &buffer) noexcept
{
    if (buffer.size() < 4U * sizeof(uint64_t) + sizeof(MessageType)) {
        return -1L;
    }

    return *reinterpret_cast<const int64_t *>(buffer.data());
}

int64_t SmemMessagePacker::Unpack(const std::vector<uint8_t> &buffer, SmemMessage &message) noexcept
{
    if (!Full(buffer)) {
        return -1;
    }

    auto ptr = buffer.data();
    auto totalSize = *reinterpret_cast<const uint64_t *>(ptr);
    ptr += sizeof(uint64_t);

    message.userDef = *reinterpret_cast<const int64_t *>(ptr);
    ptr += sizeof(int64_t);

    message.mt = *reinterpret_cast<const MessageType *>(ptr);
    ptr += sizeof(MessageType);

    auto keyCount = *reinterpret_cast<const uint64_t *>(ptr);
    ptr += sizeof(uint64_t);
    message.keys.reserve(keyCount);
    for (auto i = 0UL; i < keyCount; i++) {
        auto keySize = *reinterpret_cast<const uint64_t *>(ptr);
        ptr += sizeof(uint64_t);
        message.keys.emplace_back(reinterpret_cast<const char *>(ptr), keySize);
        ptr += keySize;
    }

    auto valueCount = *reinterpret_cast<const uint64_t *>(ptr);
    ptr += sizeof(uint64_t);
    message.values.reserve(valueCount);
    for (auto i = 0UL; i < valueCount; i++) {
        auto valueSize = *reinterpret_cast<const uint64_t *>(ptr);
        ptr += sizeof(uint64_t);
        message.values.emplace_back(ptr, ptr + valueSize);
        ptr += valueSize;
    }

    return static_cast<int64_t>(totalSize);
}

void SmemMessagePacker::PackString(std::vector<uint8_t> &dest, const std::string &str) noexcept
{
    PackValue(dest, static_cast<uint64_t>(str.size()));
    if (!str.empty()) {
        dest.insert(dest.end(), str.data(), str.data() + str.size());
    }
}

void SmemMessagePacker::PackBytes(std::vector<uint8_t> &dest, const std::vector<uint8_t> &bytes) noexcept
{
    PackValue(dest, static_cast<uint64_t>(bytes.size()));
    dest.insert(dest.end(), bytes.begin(), bytes.end());
}
}  // ock
}  // smem