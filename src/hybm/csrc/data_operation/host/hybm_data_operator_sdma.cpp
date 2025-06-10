/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_logger.h"
#include "../../under_api/dl_acl_api.h"
#include "hybm_data_operator_sdma.h"

namespace ock {
namespace mf {

HostDataOpSDMA::HostDataOpSDMA(void *stm) noexcept : stream_{stm} {}

int32_t HostDataOpSDMA::DataCopy(const void *srcVA, void *destVA, uint64_t length, DataOpDirection direction,
                                 uint32_t flags) noexcept
{
    int ret;
    switch (direction) {
        case DOP_L2S:
            ret = CopyDevice2Gva(destVA, srcVA, length);
            break;
        case DOP_S2L:
            ret = CopyGva2Device(destVA, srcVA, length);
            break;
        case DOP_H2S:
            ret = CopyHost2Gva(destVA, srcVA, length);
            break;
        case DOP_S2H:
            ret = CopyGva2Host(destVA, srcVA, length);
            break;

        case DOP_S2S:
            ret = CopyDevice2Gva(destVA, srcVA, length);
            break;

        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int HostDataOpSDMA::CopyHost2Gva(void *gvaAddr, const void *hostAddr, size_t count) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, count, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtMemcpy(copyDevice, count, hostAddr, count, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR("copy host data to temp copy memory on local device failed: " << ret);
        DlAclApi::AclrtFree(copyDevice);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyDevice2Gva(gvaAddr, copyDevice, count);
    if (result != BM_OK) {
        DlAclApi::AclrtFree(copyDevice);
        return result;
    }

    DlAclApi::AclrtFree(copyDevice);
    return BM_OK;
}

int HostDataOpSDMA::CopyDevice2Gva(void *gvaAddr, const void *deviceAddr, size_t count) noexcept
{
    auto ret = DlAclApi::AclrtMemcpyAsync(gvaAddr, count, deviceAddr, count, ACL_MEMCPY_DEVICE_TO_DEVICE, stream_);
    if (ret != 0) {
        BM_LOG_ERROR("copy memory on local device to GVA failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtSynchronizeStream(stream_);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    return BM_OK;
}

int HostDataOpSDMA::CopyGva2Device(void *deviceAddr, const void *gvaAddr, size_t count) noexcept
{
    auto ret = DlAclApi::AclrtMemcpyAsync(deviceAddr, count, gvaAddr, count, ACL_MEMCPY_DEVICE_TO_DEVICE, stream_);
    if (ret != 0) {
        BM_LOG_ERROR("copy memory on GVA to local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtSynchronizeStream(stream_);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    return BM_OK;
}

int HostDataOpSDMA::CopyGva2Host(void *hostAddr, const void *gvaAddr, size_t count) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, count, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyGva2Device(copyDevice, gvaAddr, count);
    if (result != BM_OK) {
        DlAclApi::AclrtFree(copyDevice);
        return result;
    }

    ret = DlAclApi::AclrtMemcpy(hostAddr, count, copyDevice, count, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != 0) {
        BM_LOG_ERROR("copy data on temp DEVICE to GVA failed: " << ret);
        DlAclApi::AclrtFree(copyDevice);
        return BM_DL_FUNCTION_FAILED;
    }

    DlAclApi::AclrtFree(copyDevice);
    return BM_OK;
}
}
}
