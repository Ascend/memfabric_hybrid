/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "acc_common_util.h"
#include "acc_tcp_shared_buf.h"

namespace ock {
namespace acc {
AccDataBuffer::AccDataBuffer(uint32_t memSize) : memSize_{ memSize }, data_{ new (std::nothrow) uint8_t[memSize] } {}

AccDataBuffer::AccDataBuffer(const void *data, uint32_t size) : AccDataBuffer{ size }
{
    if (data_ != nullptr) {
        const uint8_t* src_ptr = static_cast<const uint8_t*>(data);
        std::copy(src_ptr, src_ptr + size, data_);
        dataSize_ = size;
    }
}

AccDataBuffer::~AccDataBuffer()
{
    delete[] data_;
    data_ = nullptr;
    memSize_ = 0;
    dataSize_ = 0;
}

bool AccDataBuffer::AllocIfNeed(uint32_t newSize) noexcept
{
    if (newSize > MAX_RECV_BODY_LEN) {
        return false;
    }

    if (data_ == nullptr) {
        memSize_ = std::max(memSize_, newSize);
        data_ = new (std::nothrow) uint8_t[memSize_];
        return data_ != nullptr;
    }

    if (newSize > memSize_) {
        /* free old and malloc new one */
        delete[] data_;

        memSize_ = std::max(memSize_, newSize);
        data_ = new (std::nothrow) uint8_t[memSize_];
        return data_ != nullptr;
    }

    return true;
}

AccDataBufferPtr AccDataBuffer::Create(const void *data, uint32_t size)
{
    auto buffer = AccMakeRef<AccDataBuffer>(data, size);
    if (buffer.Get() == nullptr || buffer->data_ == nullptr) {
        return nullptr;
    }

    return buffer;
}

AccDataBufferPtr AccDataBuffer::Create(uint32_t memSize)
{
    auto buffer = AccMakeRef<AccDataBuffer>(memSize);
    if (buffer.Get() == nullptr || buffer->data_ == nullptr) {
        return nullptr;
    }

    return buffer;
}
} // namespace acc
} // namespace ock