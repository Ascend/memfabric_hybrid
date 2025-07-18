/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H
#define MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H

#include "hybm_common_include.h"
#include "hybm_big_mem.h"

namespace ock {
namespace mf {

class DataOperator {
public:
    virtual int32_t Initialize() noexcept = 0;
    virtual void UnInitialize() noexcept = 0;

    virtual int32_t DataCopy(const void *srcVA, void *destVA, uint64_t length, hybm_data_copy_direction direction, void *stream,
        uint32_t flags) noexcept = 0;
    virtual int32_t DataCopy2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch, uint64_t width,
                               uint64_t height, hybm_data_copy_direction direction, void *stream, uint32_t flags) noexcept = 0;

    /*
     * 异步data copy
     * @return 0 if successful, > 0 is wait id, < 0 is error
     */
    virtual int32_t DataCopyAsync(const void* srcVA, void* destVA, uint64_t length, hybm_data_copy_direction direction,
                                  void *stream, uint32_t flags) noexcept = 0;

    virtual int32_t Wait(int32_t waitId) noexcept = 0;

    virtual ~DataOperator() = default;
};
}
}

#endif // MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H
