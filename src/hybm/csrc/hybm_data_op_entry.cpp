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

    auto entity = (MemEntity *)e;
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
        BM_LOG_ERROR("input copy address(spitch: " << std::oct << spitch << ", dpitch: " << dpitch
            << ", width: " << width << ", height: " << height << ") direction: " << direction
            << ", not in entity range.");
        return BM_INVALID_PARAM;
    }

    return entity->CopyData2d(src, spitch, dest, dpitch, width, height, direction, stream, flags);
}

HYBM_API int32_t hybm_wait(hybm_entity_t e)
{
    if (e == nullptr) {
        BM_LOG_ERROR("input parameter invalid, e: 0x" << std::hex << e);
        return BM_INVALID_PARAM;
    }
    auto entity = (MemEntity *)e;
    return entity->Wait();
}

HYBM_API int32_t hybm_data_batch_copy(hybm_entity_t e,
                                      hybm_batch_copy_params* params,
                                      hybm_data_copy_direction direction,
                                      void* stream,
                                      uint32_t flags)
{
    if (e == nullptr || params == nullptr) {
        BM_LOG_ERROR("input parameter invalid, e: 0x" << std::hex << e  << ", params: 0x" << params);
        return BM_INVALID_PARAM;
    }

    if (params->sources == nullptr || params->destinations == nullptr ||
        params->dataSizes == nullptr) {
        BM_LOG_ERROR("input parameter invalid, e: 0x" << ", src array: 0x" << params->sources
                                                      << ", dest array: 0x" << params->destinations
                                                      << ", dataSizes array: 0x" << params->dataSizes);
        return BM_INVALID_PARAM;
    }

    if (params->batchSize == 0 || direction >= HYBM_DATA_COPY_DIRECTION_BUTT) {
        BM_LOG_ERROR("input parameter invalid, batchSize: " << params->batchSize
                                                            << ", direction: " << direction);
        return BM_INVALID_PARAM;
    }

    bool addressValid = true;
    auto entity = (MemEntity *)e;
    for (uint32_t i = 0; i < params->batchSize; i++) {
        if (direction == HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE || direction == HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE ||
            direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
            addressValid = entity->CheckAddressInEntity(params->destinations[i], params->dataSizes[i]);
        }
        if (direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE || direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST ||
            direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
            addressValid = (addressValid && entity->CheckAddressInEntity(params->sources[i], params->dataSizes[i]));
        }

        if (!addressValid) {
            BM_LOG_ERROR("input copy address out of entity range, size: "
                                 << std::oct << params->dataSizes[i] << ", direction: " << direction);
            return BM_INVALID_PARAM;
        }
    }

    return entity->BatchCopyData(*params, direction, stream, flags);
}
