/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <type_traits>
#include "hybm_logger.h"
#include "hybm_entity_factory.h"
#include "hybm_data_op.h"

using namespace ock::mf;
template <typename T>
struct is_unsigned_number {
    static constexpr bool value =
        std::is_same<T, unsigned short>::value ||
        std::is_same<T, unsigned int>::value ||
        std::is_same<T, unsigned long>::value ||
        std::is_same<T, unsigned long long>::value;
};

/**
 * @brief Check whether an arithmetic operation will overflow
 *
 * checks potential overflow in addition and multiplication
 *
 * @tparam T      Numeric type (integral)
 * @param a       [in] first operand
 * @param b       [in] second operand
 * @param calc    [in] operation type: '+' for addition, '*' for multiplication
 * @return true if the operation does not overflow, false otherwise
 */
template <typename T>
bool is_overflow_check(T a, T b, T max, char calc)
{
    if (!(is_unsigned_number<T>::value)) {
        return false;
    }
    switch (calc) {
        case '+':
            return (a > max - b);
        case '*':
            return ((b != 0) && (a > max / b));
        default:
            return true;
    }
}

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
        BM_LOG_ERROR("input copy address out of entity range, size: " << std::oct << params->dataSize << ", direction: "
                                                                      << direction);
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
        params->sources == nullptr || params->destinations == nullptr ||
        params->dataSizes == nullptr) {
        BM_LOG_ERROR("input parameter invalid, e: 0x" << std::hex << e
                     << ", params: 0x" << params
                     << ", src array: 0x" << params->sources
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
    BM_ASSERT_RETURN(!is_overflow_check(params->dpitch, params->height - 1, UINT64_MAX, '*'), BM_INVALID_PARAM);
    BM_ASSERT_RETURN(!is_overflow_check(params->dpitch * (params->height - 1), params->width, UINT64_MAX, '+'),
                     BM_INVALID_PARAM);
    BM_ASSERT_RETURN(!is_overflow_check(params->spitch, params->height - 1, UINT64_MAX, '*'), BM_INVALID_PARAM);
    BM_ASSERT_RETURN(!is_overflow_check(params->spitch * (params->height - 1), params->width, UINT64_MAX, '+'),
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