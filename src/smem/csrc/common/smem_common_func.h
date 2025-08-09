/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H
#define MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H

#include <climits>
#include <string>
#include "smem_types.h"
#include "smem_logger.h"

namespace ock {
namespace smem {

class CommonFunc {
public:
    template <class T>
    static std::string SmemArray2String(const T *arr, uint32_t len);
};

template <class T>
inline std::string CommonFunc::SmemArray2String(const T *arr, uint32_t len)
{
    std::ostringstream oss;
    oss << "[";
    if (arr == nullptr) {
        oss << "]";
        return oss.str();
    }
    for (uint32_t i = 0; i < len; i++) {
        oss << arr[i] << (i + 1 == len ? "]" : ",");
    }
    return oss.str();
}
}  // namespace smem
}  // namespace ock
#endif  // MEM_FABRIC_HYBRID_SMEM_COMMON_FUNC_H
