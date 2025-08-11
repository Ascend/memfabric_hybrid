/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
    HYBM_MST_HBM = 0,
    HYBM_MST_DRAM,
    HYBM_MST_HBM_USER,

    HYBM_MST_BUTT
};

enum MemSegInfoExchangeType : uint8_t {
    HYBM_INFO_EXG_IN_NODE,
    HYBM_INFO_EXG_CROSS_NODE_HCCS,
    HYBM_INFO_EXG_CROSS_NODE_SDMA,

    HYBM_INFO_EXG_BUTT
};

struct MemSegmentOptions {
    int32_t devId = 0;
    MemSegType segType = HYBM_MST_HBM;
    MemSegInfoExchangeType infoExType = HYBM_INFO_EXG_IN_NODE;
    uint64_t size = 0;
    uint32_t rankId = 0;  // must start from 0 and increase continuously
    uint32_t rankCnt = 0;  // total rank count
};
}
}

#endif  // MEM_FABRIC_HYBRID_HYBM_MM_COMMON_H
