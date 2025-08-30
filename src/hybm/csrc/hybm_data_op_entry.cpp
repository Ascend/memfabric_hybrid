/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <type_traits>
#include "hybm_logger.h"
#include "mf_num_util.h"
#include "hybm_entity_factory.h"
#include "hybm_data_op.h"

using namespace ock::mf;
HYBM_API int32_t hybm_data_copy(hybm_entity_t e, hybm_copy_params *params,
                                hybm_data_copy_direction direction, void *stream, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params->src != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params->dest != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params->count != 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(direction < HYBM_DATA_COPY_DIRECTION_BUTT, BM_INVALID_PARAM);

    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);

    bool addressValid = true;
    if (direction == HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE || direction == HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = entity->CheckAddressInEntity(params->dest, params->count);
    }
    if (direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE || direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = (addressValid && entity->CheckAddressInEntity(params->src, params->count));
    }

    if (!addressValid) {
        BM_LOG_ERROR("input copy address out of entity range, size: " << std::oct << params->count << ", direction: "
                                                                      << direction);
        return BM_INVALID_PARAM;
    }

    return entity->CopyData(*params, direction, stream, flags);
}

HYBM_API int32_t hybm_data_copy_2d(hybm_entity_t e, hybm_copy_2d_params *params,
                                   hybm_data_copy_direction direction, void *stream, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params->src != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params->dest != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params->width != 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params->height != 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(direction < HYBM_DATA_COPY_DIRECTION_BUTT, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(!NumUtil::IsOverflowCheck(params->dpitch, params->height - 1, UINT64_MAX, '*'), BM_INVALID_PARAM);
    BM_ASSERT_RETURN(!NumUtil::IsOverflowCheck(params->dpitch * (params->height - 1), params->width, UINT64_MAX, '+'),
                     BM_INVALID_PARAM);
    BM_ASSERT_RETURN(!NumUtil::IsOverflowCheck(params->spitch, params->height - 1, UINT64_MAX, '*'), BM_INVALID_PARAM);
    BM_ASSERT_RETURN(!NumUtil::IsOverflowCheck(params->spitch * (params->height - 1), params->width, UINT64_MAX, '+'),
                     BM_INVALID_PARAM);

    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);

    bool addressValid = true;
    if (direction == HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE || direction == HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = entity->CheckAddressInEntity(params->dest, params->dpitch * (params->height - 1) + params->width);
    }
    if (direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE || direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = (addressValid && entity->CheckAddressInEntity(params->src, params->spitch * (params->height - 1) + params->width));
    }

    if (!addressValid) {
        BM_LOG_ERROR("input copy address out of entity range , spitch: " << std::oct << params->spitch << ", dpitch: "
                     << params->dpitch << ", width: " << params->width << ", height: " << params->height << ") direction: " << direction);
        return BM_INVALID_PARAM;
    }

    return entity->CopyData2d(*params, direction, stream, flags);
}