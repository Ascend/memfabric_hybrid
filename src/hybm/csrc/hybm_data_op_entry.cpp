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
#include <type_traits>
#include <iomanip>
#include "hybm_logger.h"
#include "mf_num_util.h"
#include "hybm_entity_factory.h"
#include "hybm_data_op.h"
#include "hybm_va_manager.h"

using namespace ock::mf;

using hybm_check_enum = enum { OP_CHECK_IDX = 0U, OP_CHECK_SRC, OP_CHECK_DEST, OP_CHECK_BUTT };

static uint32_t g_checkMap[HYBM_DATA_COPY_DIRECTION_BUTT][OP_CHECK_BUTT] = {
    {HYBM_LOCAL_HOST_TO_GLOBAL_HOST, false, true},   {HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, false, true},
    {HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, false, true}, {HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, false, true},
    {HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, true, true},   {HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE, true, true},
    {HYBM_GLOBAL_HOST_TO_LOCAL_HOST, true, false},   {HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, true, false},
    {HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST, true, true}, {HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, true, true},
    {HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, true, false}, {HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, true, false}};

HYBM_API int32_t hybm_data_copy(hybm_entity_t e, hybm_copy_params *params, hybm_data_copy_direction direction,
                                void *stream, uint32_t flags)
{
    BM_LOG_DEBUG("Src: " << VaToInfo(params->src) << ", dest: " << VaToInfo(params->dest));
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
    BM_LOG_DEBUG("Src[0]: " << VaToInfo(params->sources[0]) << ", dest[0]: " << VaToInfo(params->destinations[0]));

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