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

#include <cstring>
#include "smem_logger.h"
#include "smem_message_packer.h"

namespace ock {
namespace smem {
namespace {
template<class T>
bool ReadScalar(const uint8_t *buffer, uint64_t bufferLen, uint64_t &offset, T &value) noexcept
{
    if (buffer == nullptr || offset + sizeof(T) > bufferLen) {
        return false;
    }
    (void)std::memcpy(&value, buffer + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}
} // namespace

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

bool SmemMessagePacker::Full(const uint8_t *buffer, const uint64_t bufferLen) noexcept
{
    constexpr uint64_t baseSize = 4U * sizeof(uint64_t) + sizeof(MessageType);
    if (buffer == nullptr || bufferLen < baseSize) {
        return false;
    }

    uint64_t offset = 0ULL;
    uint64_t totalSize = 0ULL;
    if (!ReadScalar(buffer, bufferLen, offset, totalSize)) {
        return false;
    }
    return bufferLen >= totalSize;
}

int64_t SmemMessagePacker::MessageSize(const std::vector<uint8_t> &buffer) noexcept
{
    if (buffer.size() < 4U * sizeof(uint64_t) + sizeof(MessageType)) {
        return -1L;
    }

    int64_t totalSize = 0L;
    (void)std::memcpy(&totalSize, buffer.data(), sizeof(totalSize));
    return totalSize;
}

int64_t SmemMessagePacker::Unpack(const uint8_t *buffer, const uint64_t bufferLen, SmemMessage &message) noexcept
{
    SM_ASSERT_RETURN_NOLOG(buffer != nullptr, -1);
    SM_ASSERT_RETURN_NOLOG(Full(buffer, bufferLen), -1);

    uint64_t length = 0ULL;
    uint64_t totalSize = 0ULL;
    SM_ASSERT_RETURN_NOLOG(ReadScalar(buffer, bufferLen, length, totalSize), -1);

    SM_ASSERT_RETURN_NOLOG(ReadScalar(buffer, bufferLen, length, message.userDef), -1);

    SM_ASSERT_RETURN_NOLOG(ReadScalar(buffer, bufferLen, length, message.mt), -1);
    SM_ASSERT_RETURN_NOLOG(message.mt >= MessageType::SET && message.mt <= MessageType::INVALID_MSG, -1);

    uint64_t keyCount = 0;
    SM_ASSERT_RETURN_NOLOG(ReadScalar(buffer, bufferLen, length, keyCount), -1);
    SM_ASSERT_RETURN_NOLOG(keyCount <= MAX_KEY_COUNT, -1);
    message.keys.reserve(keyCount);

    for (auto i = 0UL; i < keyCount; i++) {
        uint64_t keySize = 0;
        SM_ASSERT_RETURN_NOLOG(ReadScalar(buffer, bufferLen, length, keySize), -1);

        SM_ASSERT_RETURN_NOLOG(keySize <= MAX_KEY_SIZE && length + keySize <= bufferLen, -1);
        message.keys.emplace_back(reinterpret_cast<const char *>(buffer + length), keySize);
        length += keySize;
    }

    uint64_t valueCount = 0;
    SM_ASSERT_RETURN_NOLOG(ReadScalar(buffer, bufferLen, length, valueCount), -1);
    SM_ASSERT_RETURN_NOLOG(valueCount <= MAX_VALUE_COUNT, -1);
    message.values.reserve(valueCount);

    for (auto i = 0UL; i < valueCount; i++) {
        uint64_t valueSize = 0;
        SM_ASSERT_RETURN_NOLOG(ReadScalar(buffer, bufferLen, length, valueSize), -1);
        SM_ASSERT_RETURN_NOLOG(valueSize <= MAX_VALUE_SIZE && length + valueSize <= bufferLen, -1);

        message.values.emplace_back(buffer + length, buffer + length + valueSize);
        length += valueSize;
    }
    SM_ASSERT_RETURN_NOLOG(totalSize == length, -1);
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
} // namespace smem
} // namespace ock
