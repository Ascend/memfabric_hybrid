/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_HYBM_EX_INFO_TRANSFER_H
#define MEM_FABRIC_HYBRID_HYBM_EX_INFO_TRANSFER_H

#include <cstring>
#include <string>
#include <type_traits>

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
}
}
#endif // MEM_FABRIC_HYBRID_HYBM_EX_INFO_TRANSFER_H
