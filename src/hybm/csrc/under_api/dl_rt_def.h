/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* MemFabric_Hybrid is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#ifndef MF_HYBM_CORE_DL_RT_DEF_H
#define MF_HYBM_CORE_DL_RT_DEF_H

#include <mutex>
#include "hybm_common_include.h"

namespace ock {
namespace mf {
typedef enum {
    RT_STREAM_CREATE_ATTR_FLAGS = 1,
    RT_STREAM_CREATE_ATTR_PRIORITY = 2,
    RT_STREAM_CREATE_ATTR_MAX = 3
} rtStreamCreateAttrId;

typedef union {
    uint32_t flags;
    uint32_t priority;
    uint32_t rsv[4];
} rtStreamCreateAttrValue_t;

typedef struct {
    rtStreamCreateAttrId id;
    rtStreamCreateAttrValue_t value;
} rtStreamCreateAttr_t;

typedef struct {
    rtStreamCreateAttr_t *attrs;
    size_t numAttrs;
} rtStreamCreateConfig_t;
} // namespace mf
} // namespace ock

#endif  // MF_HYBM_CORE_DL_RT_DEF_H