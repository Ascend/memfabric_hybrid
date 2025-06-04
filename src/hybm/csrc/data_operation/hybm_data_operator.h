/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H
#define MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H

#include "hybm_common_include.h"
#include "hybm_big_mem.h"

namespace ock {
namespace mf {
enum DataOpDirection {
    DOP_L2S = HyBM_LOCAL_TO_SHARED,
    DOP_S2L = HyBM_SHARED_TO_LOCAL,
    DOP_H2S = HyBM_DRAM_TO_SHARED,
    DOP_S2H = HyBM_SHARED_TO_DRAM,
    DOP_S2S = HyBM_SHARED_TO_SHARED,

    DOP_BUTT = HyBM_DATA_COPY_DIRECTION_BUTT
};

class DataOperator {
public:
public:
    virtual int32_t DataCopy(const void *srcVA, void *destVA, uint64_t length, DataOpDirection direction,
        uint32_t flags) noexcept = 0;

    virtual ~DataOperator() = default;
};
}
}

#endif // MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H
