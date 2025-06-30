/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_logger.h"
#include "runtime_api.h"
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
    auto ret = RuntimeApi::AclrtMalloc(&copyDevice, count, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = RuntimeApi::AclrtMemcpy(copyDevice, count, hostAddr, count, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR("copy host data to temp copy memory on local device failed: " << ret);
        RuntimeApi::AclrtFree(copyDevice);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyDevice2Gva(gvaAddr, copyDevice, count);
    if (result != BM_OK) {
        RuntimeApi::AclrtFree(copyDevice);
        return result;
    }

    RuntimeApi::AclrtFree(copyDevice);
    return BM_OK;
}

int HostDataOpSDMA::CopyDevice2Gva(void *gvaAddr, const void *deviceAddr, size_t count) noexcept
{
    auto ret = RuntimeApi::AclrtMemcpyAsync(gvaAddr, count, deviceAddr, count, ACL_MEMCPY_DEVICE_TO_DEVICE, stream_);
    if (ret != 0) {
        BM_LOG_ERROR("copy memory on local device to GVA failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = RuntimeApi::AclrtSynchronizeStream(stream_);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    return BM_OK;
}

int HostDataOpSDMA::CopyGva2Device(void *deviceAddr, const void *gvaAddr, size_t count) noexcept
{
    auto ret = RuntimeApi::AclrtMemcpyAsync(deviceAddr, count, gvaAddr, count, ACL_MEMCPY_DEVICE_TO_DEVICE, stream_);
    if (ret != 0) {
        BM_LOG_ERROR("copy memory on GVA to local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = RuntimeApi::AclrtSynchronizeStream(stream_);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    return BM_OK;
}

int HostDataOpSDMA::CopyGva2Host(void *hostAddr, const void *gvaAddr, size_t count) noexcept
{
    void *copyDevice;
    auto ret = RuntimeApi::AclrtMalloc(&copyDevice, count, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyGva2Device(copyDevice, gvaAddr, count);
    if (result != BM_OK) {
        RuntimeApi::AclrtFree(copyDevice);
        return result;
    }

    ret = RuntimeApi::AclrtMemcpy(hostAddr, count, copyDevice, count, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != 0) {
        BM_LOG_ERROR("copy data on temp DEVICE to GVA failed: " << ret);
        RuntimeApi::AclrtFree(copyDevice);
        return BM_DL_FUNCTION_FAILED;
    }

    RuntimeApi::AclrtFree(copyDevice);
    return BM_OK;
}

int HostDataOpSDMA::CopyHost2Gva2d(void *gvaAddr, uint64_t dpitch, const void *hostAddr, uint64_t spitch,
                                   size_t width, uint64_t height) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, width * height, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtMemcpy2d(copyDevice, width, hostAddr, spitch, width, height, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR("copy2d host data to temp copy memory on local device failed: " << ret << " hostAddr: "
                     << hostAddr << " spitch: " << spitch << " deviceAddr: " << copyDevice
                     << " dpitch: " << width << " width: " << width << " height:" << height);
        DlAclApi::AclrtFree(copyDevice);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyDevice2Gva2d(gvaAddr, dpitch, copyDevice, width, width, height);
    if (result != BM_OK) {
        DlAclApi::AclrtFree(copyDevice);
        return result;
    }

    DlAclApi::AclrtFree(copyDevice);
    return BM_OK;
}

int HostDataOpSDMA::CopyDevice2Gva2d(void *gvaAddr, uint64_t dpitch, const void *deviceAddr, uint64_t spitch,
                   size_t width, uint64_t height) noexcept
{
    int ret = BM_OK;
    for (uint64_t i = 0; i < height; ++i) {
        void *dstAddr = reinterpret_cast<void *>((uint64_t) gvaAddr + i * dpitch);
        void *srcAddr = reinterpret_cast<void *>((uint64_t) deviceAddr + i * spitch);
        auto asyncRet = DlAclApi::AclrtMemcpyAsync(dstAddr, width, srcAddr, width,
                                                   ACL_MEMCPY_DEVICE_TO_DEVICE, stream_);
        if (asyncRet != 0) {
            BM_LOG_ERROR("copy2d memory on gva to device failed:: " << asyncRet << " gvaAddr: "
                         << gvaAddr << " dpitch: " << dpitch << " deviceAddr: " << deviceAddr
                         << " spitch: " << spitch << " width: " << width << " height:" << height);
            ret = asyncRet;
        }
    }

    int syncRet  = DlAclApi::AclrtSynchronizeStream(stream_);
    if (syncRet != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret);
        ret = syncRet;
    }
    return ret;
}

int HostDataOpSDMA::CopyGva2Host2d(void *hostAddr, uint64_t dpitch, const void *gvaAddr, uint64_t spitch,
                 size_t width, uint64_t height) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, width * height, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyGva2Device2d(copyDevice, width, gvaAddr, spitch, width, height);
    if (result != BM_OK) {
        DlAclApi::AclrtFree(copyDevice);
        return result;
    }

    ret = DlAclApi::AclrtMemcpy2d(hostAddr, dpitch, copyDevice, width, width, height, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != 0) {
        BM_LOG_ERROR("copy data on temp DEVICE to GVA failed: " << ret << " gvaAddr: "
                     << gvaAddr << " spitch: " << spitch << " width: " << width << " height:" << height);
        DlAclApi::AclrtFree(copyDevice);
        return BM_DL_FUNCTION_FAILED;
    }

    DlAclApi::AclrtFree(copyDevice);
    return BM_OK;
}

int HostDataOpSDMA::CopyGva2Device2d(void *deviceAddr, uint64_t dpitch, const void *gvaAddr, uint64_t spitch,
                   size_t width, uint64_t height) noexcept
{
    int ret = BM_OK;
    for (uint64_t i = 0; i < height; ++i) {
        void *dstAddr = reinterpret_cast<void *>((uint64_t) deviceAddr + i * dpitch);
        void *srcAddr = reinterpret_cast<void *>((uint64_t) gvaAddr + i * spitch);
        auto asyncRet = DlAclApi::AclrtMemcpyAsync(dstAddr, width, srcAddr, width,
                                                   ACL_MEMCPY_DEVICE_TO_DEVICE, stream_);
        if (asyncRet != 0) {
            BM_LOG_ERROR("copy2d memory on gva to device failed:: " << asyncRet << " gvaAddr: "
                         << srcAddr << " spitch: " << spitch << " deviceAddr: " << dstAddr
                         << " dpitch: " << dpitch << " width: " << width << " height:" << height);
            ret = asyncRet;
        }
    }

    int syncRet  = DlAclApi::AclrtSynchronizeStream(stream_);
    if (syncRet != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret);
        ret = syncRet;
    }
    return ret;
}

int HostDataOpSDMA::DataCopy2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                                   uint64_t width, uint64_t height, DataOpDirection direction, uint32_t flags) noexcept
{
    int ret;
    switch (direction) {
        case DOP_L2S:
            ret = CopyDevice2Gva2d(destVA, dpitch, srcVA, spitch, width, height);
            break;
        case DOP_S2L:
            ret = CopyGva2Device2d(destVA, dpitch, srcVA, spitch, width, height);
            break;
        case DOP_H2S:
            ret = CopyHost2Gva2d(destVA, dpitch, srcVA, spitch, width, height);
            break;
        case DOP_S2H:
            ret = CopyGva2Host2d(destVA, dpitch, srcVA, spitch, width, height);
            break;

        case DOP_S2S:
            ret = CopyDevice2Gva2d(destVA, dpitch, srcVA, spitch, width, height);
            break;

        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}
}
}
