/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
    BM_ASSERT_RETURN(params->dataSize != 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(direction < HYBM_DATA_COPY_DIRECTION_BUTT, BM_INVALID_PARAM);

    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);

    bool addressValid = true;
    if (direction == HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE || direction == HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = entity->CheckAddressInEntity(params->dest, params->dataSize);
    }
    if (direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE || direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = (addressValid && entity->CheckAddressInEntity(params->src, params->dataSize));
    }

    if (!addressValid) {
        BM_LOG_ERROR("input copy address out of entity range, size: "
                     << std::oct << params->dataSize << ", direction: " << direction);
        return BM_INVALID_PARAM;
    }

    return entity->CopyData(*params, direction, stream, flags);
}

HYBM_API int32_t hybm_data_batch_copy(hybm_entity_t e,
                                      hybm_batch_copy_params* params,
                                      hybm_data_copy_direction direction,
                                      void* stream,
                                      uint32_t flags)
{
    if (e == nullptr || params == nullptr ||
        params->sources == nullptr || params->destinations == nullptr || params->dataSizes == nullptr) {
        BM_LOG_ERROR("Input parameter invalid, please check input.");
        return BM_INVALID_PARAM;
    }
    if (params->batchSize == 0 || direction >= HYBM_DATA_COPY_DIRECTION_BUTT) {
        BM_LOG_ERROR("input parameter invalid, batchSize: " << params->batchSize << ", direction: " << direction);
        return BM_INVALID_PARAM;
    }
    bool addressValid = true;
    auto entity = (MemEntity *)e;
    bool check_dst = false;
    bool check_src = false;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE:
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE:
            check_dst = true;
            check_src = false;
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE:
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST:
            check_src = true;
            check_dst = false;
            break;
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE:
            check_src = true;
            check_dst = true;
            break;
        default:
            BM_LOG_ERROR("Invalid direction: " << direction);
            return BM_INVALID_PARAM;
    }
    for (uint32_t i = 0; i < params->batchSize; i++) {
        if (check_dst) {
            addressValid = entity->CheckAddressInEntity(params->destinations[i], params->dataSizes[i]);
        }
        if (check_src) {
            addressValid = (addressValid && entity->CheckAddressInEntity(params->sources[i], params->dataSizes[i]));
        }

        if (!addressValid) {
            BM_LOG_ERROR("input copy address out of entity range, size: "
                         << std::oct << params->dataSizes[i] << ", direction: " << direction);
            return BM_INVALID_PARAM;
        }
        params->sources[i] = Valid48BitsAddress(params->sources[i]);
        params->destinations[i] = Valid48BitsAddress(params->destinations[i]);
    }

    return entity->BatchCopyData(*params, direction, stream, flags);
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
        addressValid = entity->CheckAddressInEntity(
            params->dest, params->dpitch * (params->height - 1) + params->width);
    }
    if (direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE || direction == HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST ||
        direction == HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE) {
        addressValid = (addressValid && entity->CheckAddressInEntity(
            params->src, params->spitch * (params->height - 1) + params->width));
    }

    if (!addressValid) {
        BM_LOG_ERROR("input copy address out of entity range , spitch: " << std::oct << params->spitch << ", dpitch: "
                     << params->dpitch << ", width: " << params->width << ", height: "
                     << params->height << ") direction: " << direction);
        return BM_INVALID_PARAM;
    }

    return entity->CopyData2d(*params, direction, stream, flags);
}