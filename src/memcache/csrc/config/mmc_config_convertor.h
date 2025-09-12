/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_MMC_CONFIG_CONVERTOR_H
#define MEMFABRIC_MMC_CONFIG_CONVERTOR_H
#include <string>

#include "mmc_ref.h"

namespace ock {
namespace mmc {

class Converter : public MmcReferable {
public:
    ~Converter() override = default;

    virtual std::string Convert(const std::string &str)
    {
        return str;
    }
};

using ConverterPtr = MmcRef<Converter>;

}
}

#endif