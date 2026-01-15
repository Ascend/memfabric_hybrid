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
#ifndef ACC_LINKS_ACC_TCP_SHARED_BUF_H
#define ACC_LINKS_ACC_TCP_SHARED_BUF_H

#include "acc_def.h"

namespace ock {
namespace acc {
class ACC_API AccDataBuffer : public AccReferable {
public:
    /**
     * @brief Create a data buffer object
     */
    static AccDataBufferPtr Create(const void *data, uint32_t size);

    /**
     * @brief Create a data buffer object
     */
    static AccDataBufferPtr Create(uint32_t memSize);

public:
    /**
     * @brief Allocate memory if current allocated memory is not enough
     *
     * @param newSize      [in] new size of memory to be allocated
     * @return 0 if allocated successfully
     */
    bool AllocIfNeed(uint32_t newSize = 0) noexcept;

    /**
     * @brief Get the data ptr
     *
     * @return data ptr
     */
    uint8_t *DataPtr() const;

    /**
     * @brief Get the data ptr
     *
     * @return  data ptr
     */
    void *DataPtrVoid() const;

    /**
     * @brief Get the data ptr
     *
     * @return data ptr
     */
    uintptr_t DataIntPtr() const;

    /**
     * @brief Get the data length
     *
     * @return length of dta
     */
    uint32_t DataLen() const;

    /**
     * @brief Get the memory size
     *
     * @return size of memory
     */
    uint32_t MemSize() const;

    /**
     * @brief Set the data size after fill data
     *
     * @param size         [in] size of data
     */
    void SetDataSize(uint32_t size);

    ~AccDataBuffer() override;

    AccDataBuffer(const void *data, uint32_t size);

    explicit AccDataBuffer(uint32_t memSize);

private:
    uint32_t dataSize_ = 0;
    uint32_t memSize_;
    uint8_t *data_;
};

inline uint8_t *AccDataBuffer::DataPtr() const
{
    return data_;
}

inline void *AccDataBuffer::DataPtrVoid() const
{
    return static_cast<void *>(data_);
}

inline uintptr_t AccDataBuffer::DataIntPtr() const
{
    return reinterpret_cast<uintptr_t>(data_);
}

inline uint32_t AccDataBuffer::DataLen() const
{
    return dataSize_;
}

inline uint32_t AccDataBuffer::MemSize() const
{
    return memSize_;
}

inline void AccDataBuffer::SetDataSize(uint32_t size)
{
    dataSize_ = size;
}
} // namespace acc
} // namespace ock

#endif // ACC_LINKS_ACC_TCP_SHARED_BUF_H
