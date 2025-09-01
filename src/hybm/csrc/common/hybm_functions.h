/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_FUNCTIONS_H
#define MEM_FABRIC_HYBRID_HYBM_FUNCTIONS_H

#include "hybm_define.h"
#include "hybm_types.h"
#include "hybm_logger.h"

namespace ock {
namespace mf {
class Func {
public:
    static uint64_t MakeObjectMagic(uint64_t srcAddress);

private:
    const static uint64_t gMagicBits = 0xFFFFFFFFFF; /* get lower 40bits */
};

inline uint64_t Func::MakeObjectMagic(uint64_t srcAddress)
{
    return (srcAddress & gMagicBits) + UN40;
}
}
}

#endif // MEM_FABRIC_HYBRID_HYBM_FUNCTIONS_H
