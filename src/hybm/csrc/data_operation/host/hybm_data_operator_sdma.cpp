/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_logger.h"
#include "../../under_api/dl_acl_api.h"
#include "hybm_data_operator_sdma.h"

namespace ock {
namespace mf {

HostDataOpSDMA::~HostDataOpSDMA()
{
    HostDataOpSDMA::UnInitialize();
}

int32_t HostDataOpSDMA::Initialize() noexcept
{
    if (inited_) {
        return BM_OK;
    }

    auto ret = DlAclApi::AclrtCreateStream(&stream_);
    if (ret != 0) {
        BM_LOG_ERROR("create stream failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    inited_ = true;
    return 0;
}

void HostDataOpSDMA::UnInitialize() noexcept
{
    if (!inited_) {
        return;
    }

    if (stream_ != nullptr) {
        int32_t ret = DlAclApi::AclrtDestroyStream(stream_);
        if (ret != 0) {
            BM_LOG_ERROR("destroy stream failed: " << ret);
        }
        stream_ = nullptr;
    }
    inited_ = false;
}

int32_t HostDataOpSDMA::DataCopy(const void *srcVA, void *destVA, uint64_t length, hybm_data_copy_direction direction,
                                 void *stream, uint32_t flags) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    int ret;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = CopyDevice2Gva(destVA, srcVA, length, stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE:
            ret = CopyGva2Device(destVA, srcVA, length, stream);
            break;
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE:
            ret = CopyHost2Gva(destVA, srcVA, length, stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST:
            ret = CopyGva2Host(destVA, srcVA, length, stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = CopyDevice2Gva(destVA, srcVA, length, stream);
            break;

        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int HostDataOpSDMA::CopyHost2Gva(void *gvaAddr, const void *hostAddr, size_t count, void *stream) noexcept
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
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("free device " << copyDevice << " memory failed " << free_ret);
        }
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyDevice2Gva(gvaAddr, copyDevice, count, stream);
    if (result != BM_OK) {
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("free device " << copyDevice << " memory failed " << free_ret);
        }
        return result;
    }

    int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
    if (free_ret != 0) {
        BM_LOG_ERROR("free device " << copyDevice << " memory failed " << free_ret);
    }
    return BM_OK;
}

int HostDataOpSDMA::CopyDevice2Gva(void *gvaAddr, const void *deviceAddr, size_t count, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }

    auto ret = DlAclApi::AclrtMemcpyAsync(gvaAddr, count, deviceAddr, count, ACL_MEMCPY_DEVICE_TO_DEVICE, st);
    if (ret != 0) {
        BM_LOG_ERROR("copy memory on local device to GVA failed: " << ret << " stream:" << st);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << st);
        return BM_DL_FUNCTION_FAILED;
    }

    return BM_OK;
}

int HostDataOpSDMA::CopyGva2Device(void *deviceAddr, const void *gvaAddr, size_t count, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }

    auto ret = DlAclApi::AclrtMemcpyAsync(deviceAddr, count, gvaAddr, count, ACL_MEMCPY_DEVICE_TO_DEVICE, st);
    if (ret != 0) {
        BM_LOG_ERROR("copy memory on GVA to local device failed: " << ret << " stream:" << st);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << st);
        return BM_DL_FUNCTION_FAILED;
    }

    return BM_OK;
}

int HostDataOpSDMA::CopyGva2Host(void *hostAddr, const void *gvaAddr, size_t count, void *stream) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, count, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyGva2Device(copyDevice, gvaAddr, count, stream);
    if (result != BM_OK) {
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("free device " << copyDevice << " memory failed " << free_ret);
        }
        return result;
    }

    ret = DlAclApi::AclrtMemcpy(hostAddr, count, copyDevice, count, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != 0) {
        BM_LOG_ERROR("copy data on temp DEVICE to GVA failed: " << ret);
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("free device " << copyDevice << " memory failed " << free_ret);
        }
        return BM_DL_FUNCTION_FAILED;
    }

    int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
    if (free_ret != 0) {
        BM_LOG_ERROR("free device " << copyDevice << " memory failed " << free_ret);
    }
    return BM_OK;
}

int HostDataOpSDMA::CopyHost2Gva2d(void *gvaAddr, uint64_t dpitch, const void *hostAddr, uint64_t spitch, size_t width,
                                   uint64_t height, void *stream) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, width * height, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtMemcpy2d(copyDevice, width, hostAddr, spitch, width, height, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR("copy2d host data to temp copy memory on local device failed: "
                     << ret << " hostAddr: " << hostAddr << " spitch: " << spitch << " deviceAddr: " << copyDevice
                     << " dpitch: " << width << " width: " << width << " height:" << height);
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("free device " << copyDevice << " memory failed " << free_ret);
        }
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyDevice2Gva2d(gvaAddr, dpitch, copyDevice, width, width, height, stream);
    if (result != BM_OK) {
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("free device " << copyDevice << " memory failed " << free_ret);
        }
        return result;
    }

    int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
    if (free_ret != 0) {
        BM_LOG_ERROR("free device " << copyDevice << " memory failed " << free_ret);
    }
    return BM_OK;
}

int HostDataOpSDMA::CopyDevice2Gva2d(void *gvaAddr, uint64_t dpitch, const void *deviceAddr, uint64_t spitch,
                                     size_t width, uint64_t height, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }

    if (width == 0) {
        BM_LOG_ERROR("copy width cannot be zero.");
        return BM_INVALID_PARAM;
    }

    if (dpitch < width || spitch < width) {
        BM_LOG_ERROR("dst pitch or src pitch cannot be less than width.");
        return BM_INVALID_PARAM;
    }

    if (height > std::numeric_limits<uint64_t>::max() / dpitch || height > std::numeric_limits<uint64_t>::max() / spitch) {
        BM_LOG_ERROR("length of dst or src address cannot exceed max value of uint64_t.");
        return BM_INVALID_PARAM;
    }

    if ((uint64_t)gvaAddr > std::numeric_limits<uint64_t>::max() - height * dpitch || (uint64_t)deviceAddr > std::numeric_limits<uint64_t>::max() - height * spitch) {
        BM_LOG_ERROR("length of dst or src address with max address length cannot exceed max value of uint64_t.");
        return BM_INVALID_PARAM;
    }

    if ((uint64_t)gvaAddr + height * dpitch > SVM_END_ADDR || (uint64_t)deviceAddr + height * spitch > SVM_END_ADDR) {
        BM_LOG_ERROR("copy addr exceeds available address.");
        return BM_INVALID_PARAM;
    }

    int ret = BM_OK;
    for (uint64_t i = 0; i < height; ++i) {
        void *dstAddr = reinterpret_cast<void *>((uint64_t)gvaAddr + i * dpitch);
        void *srcAddr = reinterpret_cast<void *>((uint64_t)deviceAddr + i * spitch);
        auto asyncRet = DlAclApi::AclrtMemcpyAsync(dstAddr, width, srcAddr, width, ACL_MEMCPY_DEVICE_TO_DEVICE, st);
        if (asyncRet != 0) {
            BM_LOG_ERROR("copy2d memory on gva to device failed:: "
                         << asyncRet << " gvaAddr: " << gvaAddr << " dpitch: " << dpitch
                         << " deviceAddr: " << deviceAddr << " spitch: " << spitch << " width: " << width
                         << " height:" << height << " stream:" << st);
            ret = BM_DL_FUNCTION_FAILED;
            break;
        }
    }

    int syncRet = DlAclApi::AclrtSynchronizeStream(st);
    if (syncRet != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << syncRet << " stream:" << st);
        ret = BM_DL_FUNCTION_FAILED;
    }
    return ret;
}

int HostDataOpSDMA::CopyGva2Host2d(void *hostAddr, uint64_t dpitch, const void *gvaAddr, uint64_t spitch, size_t width,
                                   uint64_t height, void *stream) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, width * height, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyGva2Device2d(copyDevice, width, gvaAddr, spitch, width, height, stream);
    if (result != BM_OK) {
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("free device " << copyDevice << " memory failed " << free_ret);
        }
        return result;
    }

    ret = DlAclApi::AclrtMemcpy2d(hostAddr, dpitch, copyDevice, width, width, height, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != 0) {
        BM_LOG_ERROR("copy data on temp DEVICE to GVA failed: " << ret << " gvaAddr: " << gvaAddr
                                                                << " spitch: " << spitch << " width: " << width
                                                                << " height:" << height);
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("free device " << copyDevice << " memory failed " << free_ret);
        }
        return BM_DL_FUNCTION_FAILED;
    }

    int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
    if (free_ret != 0) {
        BM_LOG_ERROR("free device " << copyDevice << " memory failed " << free_ret);
    }
    return BM_OK;
}

int HostDataOpSDMA::CopyGva2Device2d(void *deviceAddr, uint64_t dpitch, const void *gvaAddr, uint64_t spitch,
                                     size_t width, uint64_t height, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }

    int ret = BM_OK;
    for (uint64_t i = 0; i < height; ++i) {
        void *dstAddr = reinterpret_cast<void *>((uint64_t)deviceAddr + i * dpitch);
        void *srcAddr = reinterpret_cast<void *>((uint64_t)gvaAddr + i * spitch);
        auto asyncRet = DlAclApi::AclrtMemcpyAsync(dstAddr, width, srcAddr, width, ACL_MEMCPY_DEVICE_TO_DEVICE, st);
        if (asyncRet != 0) {
            BM_LOG_ERROR("copy2d memory on gva to device failed:: "
                         << asyncRet << " gvaAddr: " << srcAddr << " spitch: " << spitch << " deviceAddr: " << dstAddr
                         << " dpitch: " << dpitch << " width: " << width << " height:" << height << " stream:" << st);
            ret = BM_DL_FUNCTION_FAILED;
            break;
        }
    }

    int syncRet = DlAclApi::AclrtSynchronizeStream(st);
    if (syncRet != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << syncRet << " stream:" << st);
        ret = BM_DL_FUNCTION_FAILED;
    }
    return ret;
}

int HostDataOpSDMA::DataCopy2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch, uint64_t width,
                               uint64_t height, hybm_data_copy_direction direction, void *stream,
                               uint32_t flags) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    int ret;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = CopyDevice2Gva2d(destVA, dpitch, srcVA, spitch, width, height, stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE:
            ret = CopyGva2Device2d(destVA, dpitch, srcVA, spitch, width, height, stream);
            break;
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE:
            ret = CopyHost2Gva2d(destVA, dpitch, srcVA, spitch, width, height, stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST:
            ret = CopyGva2Host2d(destVA, dpitch, srcVA, spitch, width, height, stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = CopyDevice2Gva2d(destVA, dpitch, srcVA, spitch, width, height, stream);
            break;

        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int32_t HostDataOpSDMA::DataCopyAsync(const void *srcVA, void *destVA, uint64_t length,
                                      hybm_data_copy_direction direction, void *stream, uint32_t flags) noexcept
{
    BM_LOG_ERROR("not supported data copy async!");
    return BM_ERROR;
}

int32_t HostDataOpSDMA::Wait(int32_t waitId) noexcept
{
    BM_LOG_ERROR("not supported data copy wait!");
    return BM_ERROR;
}

}  // namespace mf
}  // namespace ock
