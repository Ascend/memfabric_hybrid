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

#ifndef MEM_FABRIC_HYBRID_HYBM_EX_INFO_TRANSFER_H
#define MEM_FABRIC_HYBRID_HYBM_EX_INFO_TRANSFER_H

#include <cstring>
#include <string>
#include <type_traits>
#include <algorithm>
#include "hybm_def.h"
#include "hybm_logger.h"

namespace ock {
namespace mf {
template<class DataType>
class ExInfoTranslator {
public:
    virtual int Serialize(const DataType &d, std::string &info) noexcept = 0;
    virtual int Deserialize(const std::string &info, DataType &d) noexcept = 0;
};

template<class DataType>
class LiteralExInfoTranslater : public ExInfoTranslator<DataType> {
public:
    int Serialize(const DataType &d, std::string &info) noexcept override
    {
        if (!std::is_literal_type<DataType>::value) {
            return -1;
        }

        BM_LOG_DEBUG("serialize data length = " << sizeof(DataType));
        info = std::string(reinterpret_cast<const char *>(&d), sizeof(DataType));
        return 0;
    }

    int Deserialize(const std::string &info, DataType &d) noexcept override
    {
        if (!std::is_literal_type<DataType>::value) {
            return -1;
        }

        if (info.length() != sizeof(DataType)) {
            BM_LOG_ERROR("deserialize info len: " << info.length() << " not matches data type: " << sizeof(DataType));
            return -1;
        }

        std::copy_n(info.data(), sizeof(DataType), reinterpret_cast<uint8_t *>(&d));
        return 0;
    }
};

class ExchangeInfoReader {
public:
    explicit ExchangeInfoReader(const hybm_exchange_info *info = nullptr) noexcept : exchangeInfo_{info}, readOffset_{0}
    {}

    void Reset(const hybm_exchange_info *info = nullptr) noexcept
    {
        if (info != nullptr) {
            exchangeInfo_ = info;
        }
        readOffset_ = 0;
    }

    inline int Test(void *buffer, size_t length) const noexcept
    {
        BM_ASSERT_RETURN(exchangeInfo_ != nullptr, -1);
        if (readOffset_ + length > exchangeInfo_->descLen) {
            BM_LOG_ERROR("read data size: " << length << " too long");
            return -1;
        }

        try {
            std::copy_n(exchangeInfo_->desc + readOffset_, length, (uint8_t *)buffer);
        } catch (...) {
            BM_LOG_ERROR("copy failed.");
            return -1;
        }
        return 0;
    }

    inline int Read(void *buffer, size_t length) const noexcept
    {
        BM_ASSERT_RETURN(exchangeInfo_ != nullptr, -1);
        if (readOffset_ + length > exchangeInfo_->descLen) {
            BM_LOG_ERROR("read data size: " << length << " too long");
            return -1;
        }

        try {
            std::copy_n(exchangeInfo_->desc + readOffset_, length, (uint8_t *)buffer);
        } catch (...) {
            BM_LOG_ERROR("copy failed.");
            return -1;
        }
        readOffset_ += length;
        return 0;
    }

    inline size_t LeftBytes() const noexcept
    {
        BM_ASSERT_RETURN(exchangeInfo_ != nullptr, 0);
        if (readOffset_ >= exchangeInfo_->descLen) {
            return 0U;
        }

        return exchangeInfo_->descLen - readOffset_;
    }

    std::string LeftToString() const noexcept
    {
        BM_ASSERT_RETURN(exchangeInfo_ != nullptr, "");
        if (readOffset_ >= exchangeInfo_->descLen) {
            return "";
        }

        std::string left(exchangeInfo_->desc + readOffset_, exchangeInfo_->desc + exchangeInfo_->descLen);
        readOffset_ = exchangeInfo_->descLen;
        return left;
    }

    template<typename DataType>
    inline int Test(DataType &data) const noexcept
    {
        return Test(static_cast<void *>(&data), sizeof(data));
    }

    template<typename DataType>
    inline int Read(DataType &data) const noexcept
    {
        return Read(static_cast<void *>(&data), sizeof(data));
    }

private:
    const hybm_exchange_info *exchangeInfo_;
    mutable uint32_t readOffset_;
};

class ExchangeInfoWriter {
public:
    explicit ExchangeInfoWriter(hybm_exchange_info *info) noexcept : exchangeInfo_{info}
    {
        if (exchangeInfo_ != nullptr) {
            exchangeInfo_->descLen = 0;
        }
    }

    inline int Append(const void *data, size_t length) noexcept
    {
        BM_ASSERT_RETURN(exchangeInfo_ != nullptr, -1);
        if (exchangeInfo_->descLen > sizeof(exchangeInfo_->desc)) {
            BM_LOG_ERROR("write data size: " << length << " too long");
            return -1;
        }

        try {
            std::copy_n(reinterpret_cast<const uint8_t *>(data), length, exchangeInfo_->desc + exchangeInfo_->descLen);
        } catch (...) {
            BM_LOG_ERROR("copy failed.");
            return -1;
        }
        exchangeInfo_->descLen += length;
        return 0;
    }

    template<class DataType>
    inline int Append(const DataType &data) noexcept
    {
        return Append(reinterpret_cast<const void *>(&data), sizeof(data));
    }

private:
    hybm_exchange_info *exchangeInfo_;
};
} // namespace mf
} // namespace ock
#endif // MEM_FABRIC_HYBRID_HYBM_EX_INFO_TRANSFER_H