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
#include <iomanip>
#include "hybm_logger.h"
#include "mf_num_util.h"
#include "hybm_entity_factory.h"
#include "hybm_data_op.h"

using namespace ock::mf;

using hybm_check_enum = enum {
    OP_CHECK_IDX = 0U,
    OP_CHECK_SRC,
    OP_CHECK_DEST,
    OP_CHECK_BUTT
};

static uint32_t g_checkMap[HYBM_DATA_COPY_DIRECTION_BUTT][OP_CHECK_BUTT] = {
    {HYBM_LOCAL_HOST_TO_GLOBAL_HOST, false, true},
    {HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, false, true},
    {HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, false, true},
    {HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, false, true},
    {HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, true, true},
    {HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE, true, true},
    {HYBM_GLOBAL_HOST_TO_LOCAL_HOST, true, false},
    {HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, true, false},
    {HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST, true, true},
    {HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, true, true},
    {HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, true, false},
    {HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, true, false}
};

HYBM_API int32_t hybm_data_copy(hybm_entity_t e, hybm_copy_params *params, hybm_data_copy_direction direction,
                                void *stream, uint32_t flags)
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
    if (g_checkMap[direction][OP_CHECK_DEST]) {
        addressValid = entity->CheckAddressInEntity(params->dest, params->dataSize);
    }
    if (g_checkMap[direction][OP_CHECK_SRC]) {
        addressValid = (addressValid && entity->CheckAddressInEntity(params->src, params->dataSize));
    }

    if (!addressValid) {
        BM_LOG_ERROR("input copy address out of entity range, size: " << std::oct << params->dataSize
                                                                      << ", direction: " << direction);
        return BM_INVALID_PARAM;
    }

    return entity->CopyData(*params, direction, stream, flags);
}

HYBM_API int32_t hybm_wait(hybm_entity_t e)
{
    if (e == nullptr) {
        BM_LOG_ERROR("input parameter invalid, e: 0x" << std::hex << e);
        return BM_INVALID_PARAM;
    }
    auto entity = static_cast<MemEntity *>(e);
    return entity->Wait();
}

HYBM_API int32_t hybm_data_batch_copy(hybm_entity_t e, hybm_batch_copy_params *params,
                                      hybm_data_copy_direction direction, void *stream, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params->sources != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params->destinations != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params->dataSizes != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params->batchSize != 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(direction < HYBM_DATA_COPY_DIRECTION_BUTT, BM_INVALID_PARAM);

    bool addressValid = true;
    auto entity = (MemEntity *)e;
    bool check_dst = g_checkMap[direction][OP_CHECK_DEST];
    bool check_src = g_checkMap[direction][OP_CHECK_SRC];
    for (uint32_t i = 0; i < params->batchSize; i++) {
        if (params->sources[i] == nullptr || params->destinations[i] == nullptr) {
            BM_LOG_ERROR("input copy address is invalid, source or dest is nullptr, index:" << i);
            return BM_INVALID_PARAM;
        }
        if (check_dst) {
            addressValid = entity->CheckAddressInEntity(params->destinations[i], params->dataSizes[i]);
        }
        if (check_src) {
            addressValid = (addressValid && entity->CheckAddressInEntity(params->sources[i], params->dataSizes[i]));
        }

        if (!addressValid) {
            BM_LOG_ERROR("input copy address out of entity range, size: " << std::oct << params->dataSizes[i]
                                                                          << ", direction: " << direction);
            return BM_INVALID_PARAM;
        }
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