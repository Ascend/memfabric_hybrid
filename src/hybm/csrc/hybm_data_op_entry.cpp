/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <iomanip>
#include "hybm_logger.h"
#include "hybm_entity.h"
#include "hybm_data_op.h"

using namespace ock::mf;

HYBM_API int32_t hybm_data_copy(hybm_entity_t e, const void *src, void *dest, size_t count,
                                hybm_data_copy_direction direction, void *stream, uint32_t flags)
{
    if (e == nullptr || src == nullptr || dest == nullptr) {
        BM_LOG_ERROR("input parameter invalid, e: 0x" << std::hex << e << ", src: 0x" << src << ", dest: 0x" << dest);
        return BM_INVALID_PARAM;
    }

    if (count == 0 || direction >= HYBM_DATA_COPY_DIRECTION_BUTT) {
        BM_LOG_ERROR("input parameter invalid, count: " << count << ", direction: " << direction);
        return BM_INVALID_PARAM;
    }

    bool addressValid = true;
    auto entity = (MemEntity *)e;
    if (direction == HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE || direction == HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = entity->CheckAddressInEntity(dest, count);
    }
    if (direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE || direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = (addressValid && entity->CheckAddressInEntity(src, count));
    }

    if (!addressValid) {
        BM_LOG_ERROR("input copy address(src: 0x" << std::hex << src << ", dest: 0x" << dest << ", size: " << std::oct
                                                  << count << ") direction: " << direction << ", not in entity range.");
        return BM_INVALID_PARAM;
    }

    return entity->CopyData(src, dest, count, direction, stream, flags);
}

HYBM_API int32_t hybm_data_copy_2d(hybm_entity_t e, const void *src, uint64_t spitch,
                                   void *dest, uint64_t dpitch, uint64_t width, uint64_t height,
                                   hybm_data_copy_direction direction, void *stream, uint32_t flags)
{
    if (e == nullptr || src == nullptr || dest == nullptr) {
        BM_LOG_ERROR("input parameter invalid, e: 0x" << std::hex << e << ", src: 0x" << src << ", dest: 0x" << dest);
        return BM_INVALID_PARAM;
    }

    if (width == 0 || height == 0 || direction >= HYBM_DATA_COPY_DIRECTION_BUTT) {
        BM_LOG_ERROR("input parameter invalid, width: " << width << " height: "
                                                        << height << ", direction: " << direction);
        return BM_INVALID_PARAM;
    }

    bool addressValid = true;
    auto entity = (MemEntity *)e;
    if (direction == HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE || direction == HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = entity->CheckAddressInEntity(dest, dpitch * (height - 1) + width);
    }
    if (direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE || direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = (addressValid && entity->CheckAddressInEntity(src, spitch * (height - 1) + width));
    }

    if (!addressValid) {
        BM_LOG_ERROR("input copy address(src: 0x" << std::hex << src << ", dest: 0x" << dest << ", spitch: " << std::oct
                     << spitch << ", dpitch: " << dpitch << ", width: " << width << ", height: " << height
                     << ") direction: " << direction << ", not in entity range.");
        return BM_INVALID_PARAM;
    }

    return entity->CopyData2d(src, spitch, dest, dpitch, width, height, direction, stream, flags);
}