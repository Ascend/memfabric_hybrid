/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_MM_COMMON_H
#define MEM_FABRIC_HYBRID_HYBM_MM_COMMON_H

#include "hybm_common_include.h"

namespace ock {
namespace mf {

constexpr auto DEVICE_SHM_NAME_SIZE = 64U;
class MemSlice;
class MemSegment;
class MemSegmentDevice;

using MemSlicePtr = std::shared_ptr<MemSlice>;
using MemSegmentPtr = std::shared_ptr<MemSegment>;

enum MemType : uint8_t {
    MEM_TYPE_HOST_DRAM = 0,
    MEM_TYPE_DEVICE_HBM,

    MEM_TYPE_DEVICE_BUTT
};

enum MemPageTblType : uint8_t {
    MEM_PT_TYPE_SVM = 0,
    MEM_PT_TYPE_HYM,

    MEM_PT_TYPE_BUTT
};

enum MemAddrType : uint8_t {
    MEM_ADDR_TYPE_VIRTUAL = 0,
    MEM_ADDR_TYPE_PHYSICAL,

    MEM_ADDR_TYPE_BUTT
};

enum MemSegType : uint8_t {
    HyBM_MST_HBM = 0,
    HyBM_MST_DRAM,

    HyBM_MST_BUTT
};

enum MemSegInfoExchangeType : uint8_t {
    HyBM_INFO_EXG_IN_NODE,
    HyBM_INFO_EXG_CROSS_NODE_HCCS,
    HyBM_INFO_EXG_CROSS_NODE_SDMA,

    HyBM_INFO_EXG_BUTT
};

struct MemSegmentOptions {
    int32_t id = 0;
    MemSegType segType = HyBM_MST_HBM;
    MemSegInfoExchangeType infoExType = HyBM_INFO_EXG_IN_NODE;
    uint64_t size = 0;
};
}
}

#endif  // MEM_FABRIC_HYBRID_HYBM_MM_COMMON_H
