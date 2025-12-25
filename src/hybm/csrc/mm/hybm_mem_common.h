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
#ifndef MEM_FABRIC_HYBRID_HYBM_MM_COMMON_H
#define MEM_FABRIC_HYBRID_HYBM_MM_COMMON_H

#include "hybm_common_include.h"

namespace ock {
namespace mf {

constexpr auto DEVICE_SHM_NAME_SIZE = 64U;
class MemSlice;
class MemSegment;
class HybmDevLegacySegment;

using MemSlicePtr = std::shared_ptr<MemSlice>;
using MemSegmentPtr = std::shared_ptr<MemSegment>;

enum MemType : int32_t {
    MEM_TYPE_HOST_DRAM = 0,
    MEM_TYPE_DEVICE_HBM,

    MEM_TYPE_BUTT
};

enum MemPageTblType : int32_t {
    MEM_PT_TYPE_SVM = 0,
    MEM_PT_TYPE_GVM,
    MEM_PT_TYPE_HYM,

    MEM_PT_TYPE_BUTT
};

enum MemAddrType : int32_t {
    MEM_ADDR_TYPE_VIRTUAL = 0,
    MEM_ADDR_TYPE_PHYSICAL,

    MEM_ADDR_TYPE_BUTT
};

enum MemSegType : int32_t {
    HYBM_MST_HBM = 0,
    HYBM_MST_DRAM,
    HYBM_MST_HBM_USER,

    HYBM_MST_BUTT
};

enum MemSegInfoExchangeType : int32_t {
    HYBM_INFO_EXG_IN_NODE,
    HYBM_INFO_EXG_CROSS_NODE_HCCS,
    HYBM_INFO_EXG_CROSS_NODE_SDMA,

    HYBM_INFO_EXG_BUTT
};

struct MemSegmentOptions {
    int32_t devId = 0;
    hybm_role_type role = HYBM_ROLE_PEER;
    hybm_data_op_type dataOpType = HYBM_DOP_TYPE_SDMA;
    MemSegType segType = HYBM_MST_HBM;
    MemSegInfoExchangeType infoExType = HYBM_INFO_EXG_IN_NODE;
    bool shared = true;
    uint64_t size = 0;
    uint32_t rankId = 0;  // must start from 0 and increase continuously
    uint32_t rankCnt = 0; // total rank count
    uint32_t flags = 0;
};
}
}

#endif // MEM_FABRIC_HYBRID_HYBM_MM_COMMON_H