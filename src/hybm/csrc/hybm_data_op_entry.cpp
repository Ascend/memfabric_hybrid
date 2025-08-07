/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include "hybm_logger.h"
#include "hybm_entity_factory.h"
#include "hybm_data_op.h"

using namespace ock::mf;

HYBM_API int32_t hybm_data_copy(hybm_entity_t e, const void *src, void *dest, size_t count,
                                hybm_data_copy_direction direction, void *stream, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(src != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(dest != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(count != 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(direction < HYBM_DATA_COPY_DIRECTION_BUTT, BM_INVALID_PARAM);

    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);

    bool addressValid = true;
    if (direction == HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE || direction == HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = entity->CheckAddressInEntity(dest, count);
    }
    if (direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE || direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = (addressValid && entity->CheckAddressInEntity(src, count));
    }

    if (!addressValid) {
        BM_LOG_ERROR("input copy address out of entity range, size: " << std::oct << count << ", direction: "
                                                                      << direction);
        return BM_INVALID_PARAM;
    }

    return entity->CopyData(src, dest, count, direction, stream, flags);
}

HYBM_API int32_t hybm_data_copy_2d(hybm_entity_t e, const void *src, uint64_t spitch,
                                   void *dest, uint64_t dpitch, uint64_t width, uint64_t height,
                                   hybm_data_copy_direction direction, void *stream, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(src != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(dest != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(width != 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(height != 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(direction < HYBM_DATA_COPY_DIRECTION_BUTT, BM_INVALID_PARAM);

    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);

    bool addressValid = true;
    if (direction == HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE || direction == HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = entity->CheckAddressInEntity(dest, dpitch * (height - 1) + width);
    }
    if (direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE || direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = (addressValid && entity->CheckAddressInEntity(src, spitch * (height - 1) + width));
    }

    if (!addressValid) {
        BM_LOG_ERROR("input copy address out of entity range , spitch: " << std::oct << spitch << ", dpitch: "
                     << dpitch << ", width: " << width << ", height: " << height << ") direction: " << direction);
        return BM_INVALID_PARAM;
    }

    return entity->CopyData2d(src, spitch, dest, dpitch, width, height, direction, stream, flags);
}
