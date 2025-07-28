/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include "smem_logger.h"
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

bool SmemMessagePacker::Full(const uint8_t* buffer, const uint64_t bufferLen) noexcept
{
    constexpr uint64_t baseSize = 4U * sizeof(uint64_t) + sizeof(MessageType);
    if (bufferLen < baseSize) {
        return false;
    }

    auto totalSize = *reinterpret_cast<const uint64_t *>(buffer);
    return bufferLen >= totalSize;
}

int64_t SmemMessagePacker::MessageSize(const std::vector<uint8_t> &buffer) noexcept
{
    if (buffer.size() < sizeof(uint64_t)) {
        return -1L;
    }

    return *reinterpret_cast<const int64_t *>(buffer.data());
}

int64_t SmemMessagePacker::Unpack(const uint8_t* buffer, const uint64_t bufferLen, SmemMessage &message) noexcept
{
    SM_CHECK_CONDITION_RET(buffer == nullptr, -1);
    SM_CHECK_CONDITION_RET(!Full(buffer, bufferLen), -1);

    uint64_t length = 0ULL;
    auto totalSize = *reinterpret_cast<const uint64_t *>(buffer + length);
    length += sizeof(uint64_t);

    message.userDef = *reinterpret_cast<const int64_t *>(buffer + length);
    length += sizeof(int64_t);

    message.mt = *reinterpret_cast<const MessageType *>(buffer + length);
    length += sizeof(MessageType);
    SM_CHECK_CONDITION_RET(message.mt < MessageType::SET || message.mt > MessageType::INVALID_MSG, -1);

    auto keyCount = *reinterpret_cast<const uint64_t *>(buffer + length);
    SM_CHECK_CONDITION_RET(keyCount > MAX_KEY_COUNT, -1);

    length += sizeof(uint64_t);
    message.keys.reserve(keyCount);

    for (auto i = 0UL; i < keyCount; i++) {
        auto keySize = *reinterpret_cast<const uint64_t *>(buffer + length);
        length += sizeof(uint64_t);

        SM_CHECK_CONDITION_RET(keySize > MAX_KEY_SIZE || length + keySize > bufferLen, -1);
        message.keys.emplace_back(reinterpret_cast<const char *>(buffer + length), keySize);
        length += keySize;
    }

    auto valueCount = *reinterpret_cast<const uint64_t *>(buffer + length);
    SM_CHECK_CONDITION_RET(valueCount > MAX_VALUE_COUNT, -1);

    length += sizeof(uint64_t);
    message.values.reserve(valueCount);

    for (auto i = 0UL; i < valueCount; i++) {
        auto valueSize = *reinterpret_cast<const uint64_t *>(buffer + length);
        length += sizeof(uint64_t);
        SM_CHECK_CONDITION_RET(valueSize > MAX_VALUE_SIZE || length + valueSize > bufferLen, -1);

        message.values.emplace_back(buffer + length, buffer + length + valueSize);
        length += valueSize;
    }
    SM_CHECK_CONDITION_RET(totalSize != length, -1);
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