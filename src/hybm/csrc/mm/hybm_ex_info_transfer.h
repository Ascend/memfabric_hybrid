/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
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
template <class DataType> class ExInfoTranslator {
public:
    virtual int Serialize(const DataType &d, std::string &info) noexcept = 0;
    virtual int Deserialize(const std::string &info, DataType &d) noexcept = 0;
};

template <class DataType> class LiteralExInfoTranslater : public ExInfoTranslator<DataType> {
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

        memcpy(&d, info.data(), sizeof(DataType));
        return 0;
    }
};

class ExchangeInfoReader {
public:
    explicit ExchangeInfoReader(const hybm_exchange_info *info = nullptr) noexcept : exchangeInfo_{info}, readOffset_{0}
    {
    }

    void Reset(const hybm_exchange_info *info = nullptr) noexcept
    {
        if (info != nullptr) {
            exchangeInfo_ = info;
        }
        readOffset_ = 0;
    }

    inline int Test(void *buffer, size_t length) const noexcept
    {
        if (readOffset_ + length > exchangeInfo_->descLen) {
            BM_LOG_ERROR("read data size: " << length << " too long");
            return -1;
        }

        std::copy_n(exchangeInfo_->desc + readOffset_, length, (uint8_t *)buffer);
        return 0;
    }

    inline int Read(void *buffer, size_t length) const noexcept
    {
        if (readOffset_ + length > exchangeInfo_->descLen) {
            BM_LOG_ERROR("read data size: " << length << " too long");
            return -1;
        }

        std::copy_n(exchangeInfo_->desc + readOffset_, length, (uint8_t *)buffer);
        readOffset_ += length;
        return 0;
    }

    inline size_t LeftBytes() const noexcept
    {
        if (readOffset_ >= exchangeInfo_->descLen) {
            return 0U;
        }

        return exchangeInfo_->descLen - readOffset_;
    }

    std::string LeftToString() const noexcept
    {
        if (readOffset_ >= exchangeInfo_->descLen) {
            return "";
        }

        std::string left(exchangeInfo_->desc + readOffset_, exchangeInfo_->desc + exchangeInfo_->descLen);
        readOffset_ = exchangeInfo_->descLen;
        return left;
    }

    template <typename DataType>
    inline int Test(DataType &data) const noexcept
    {
        return Test(static_cast<void *>(&data), sizeof(data));
    }

    template <typename DataType>
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
        exchangeInfo_->descLen = 0;
    }

    inline int Append(const void *data, size_t length) noexcept
    {
        if (exchangeInfo_->descLen > sizeof(exchangeInfo_->desc)) {
            BM_LOG_ERROR("write data size: " << length << " too long");
            return -1;
        }

        std::copy_n(reinterpret_cast<const uint8_t *>(data), length, exchangeInfo_->desc + exchangeInfo_->descLen);
        exchangeInfo_->descLen += length;
        return 0;
    }

    template <class DataType>
    inline int Append(const DataType &data) noexcept
    {
        return Append(reinterpret_cast<const void *>(&data), sizeof(data));
    }

private:
    hybm_exchange_info *exchangeInfo_;
};
}
}
#endif // MEM_FABRIC_HYBRID_HYBM_EX_INFO_TRANSFER_H
