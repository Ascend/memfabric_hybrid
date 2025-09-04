/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_logger.h"
#include "../../under_api/dl_acl_api.h"
#include "hybm_data_operator_sdma.h"

namespace ock {
namespace mf {

thread_local void *HostDataOpSDMA::stream_ = nullptr;

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

int HostDataOpSDMA::PrepareThreadLocalStream() noexcept
{
    if (stream_ != nullptr) {
        return BM_OK;
    }

    auto ret = DlAclApi::AclrtCreateStream(&stream_);
    if (ret != 0) {
        BM_LOG_ERROR("create thread local stream failed: " << ret);
        return ret;
    }
    return BM_OK;
}

int32_t HostDataOpSDMA::DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                                 const ExtOptions &options) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    auto ret = PrepareThreadLocalStream();
    if (ret != BM_OK) {
        return ret;
    }
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = CopyDevice2Gva(params.dest, params.src, params.dataSize, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE:
            ret = CopyGva2Device(params.dest, params.src, params.dataSize, options.stream);
            break;
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE:
            ret = CopyHost2Gva(params.dest, params.src, params.dataSize, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST:
            ret = CopyGva2Host(params.dest, params.src, params.dataSize, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = CopyDevice2Gva(params.dest, params.src, params.dataSize, options.stream);
            break;

        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int32_t HostDataOpSDMA::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                      const ExtOptions &options) noexcept
{
    auto ret = PrepareThreadLocalStream();
    if (ret != BM_OK) {
        return ret;
    }
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = BatchCopyDevice2Gva(params.destinations, params.sources, params.dataSizes,
                                      params.batchSize, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE:
            ret = BatchCopyGva2Device(params.destinations, params.sources, params.dataSizes,
                                      params.batchSize, options.stream);
            break;
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE:
            ret = BatchCopyHost2Gva(params.destinations, params.sources, params.dataSizes,
                                    params.batchSize, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST:
            ret = BatchCopyGva2Host(params.destinations, params.sources, params.dataSizes,
                                    params.batchSize, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = BatchCopyDevice2Gva(params.destinations, params.sources, params.dataSizes,
                                      params.batchSize, options.stream);
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
        return ret;
    }

    ret = DlAclApi::AclrtMemcpy(copyDevice, count, hostAddr, count, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR("copy host data to temp copy memory on local device failed: " << ret);
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("device memory free failed, ret: " << free_ret);
        }
        return ret;
    }

    auto result = CopyDevice2Gva(gvaAddr, copyDevice, count, stream);
    if (result != BM_OK) {
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("device memory free failed, ret: " << free_ret);
        }
        return result;
    }

    int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
    if (free_ret != 0) {
        BM_LOG_ERROR("device memory free failed, ret: " << free_ret);
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
        BM_LOG_ERROR("copy memory on local device to GVA failed: " << ret);
        return ret;
    }

    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret);
        return ret;
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
        BM_LOG_ERROR("copy memory on GVA to local device failed: " << ret);
        return ret;
    }

    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret);
        return ret;
    }

    return BM_OK;
}

int HostDataOpSDMA::CopyGva2Host(void *hostAddr, const void *gvaAddr, size_t count, void *stream) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, count, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return ret;
    }

    auto result = CopyGva2Device(copyDevice, gvaAddr, count, stream);
    if (result != BM_OK) {
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("device memory free failed, ret: " << free_ret);
        }
        return result;
    }

    ret = DlAclApi::AclrtMemcpy(hostAddr, count, copyDevice, count, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != 0) {
        BM_LOG_ERROR("copy data on temp DEVICE to GVA failed: " << ret);
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("device memory free failed, ret: " << free_ret);
        }
        return ret;
    }

    int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
    if (free_ret != 0) {
        BM_LOG_ERROR("device memory free failed, ret: " << free_ret);
    }
    return BM_OK;
}

int HostDataOpSDMA::CopyHost2Gva2d(hybm_copy_2d_params &params, void *stream) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, params.width * params.height, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return ret;
    }

    ret = DlAclApi::AclrtMemcpy2d(copyDevice, params.width, params.src, params.spitch,
                                  params.width, params.height, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR("copy2d host data to temp copy memory on local device failed: "
                     << ret << " spitch: " << params.spitch << " dpitch: " << params.width
                     << " width: " << params.width << " height:" << params.height);
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("device memory free failed, ret: " << free_ret);
        }
        return ret;
    }
    params.src = copyDevice;
    auto result = CopyDevice2Gva2d(params, stream);
    if (result != BM_OK) {
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("device memory free failed, ret: " << free_ret);
        }
        return result;
    }

    int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
    if (free_ret != 0) {
        BM_LOG_ERROR("device memory free failed, ret: " << free_ret);
    }
    return BM_OK;
}

int HostDataOpSDMA::CheckDevice2Gva2dStatus(hybm_copy_2d_params &params) noexcept
{
    if (params.width == 0) {
        BM_LOG_ERROR("copy width cannot be zero.");
        return BM_INVALID_PARAM;
    }

    if (params.dpitch < params.width || params.spitch < params.width) {
        BM_LOG_ERROR("dst pitch or src pitch cannot be less than width.");
        return BM_INVALID_PARAM;
    }

    if (params.height > std::numeric_limits<uint64_t>::max() / params.dpitch
        || params.height > std::numeric_limits<uint64_t>::max() / params.spitch) {
        BM_LOG_ERROR("length of dst or src address cannot exceed max value of uint64_t.");
        return BM_INVALID_PARAM;
    }

    if ((uint64_t)params.dest > std::numeric_limits<uint64_t>::max() - params.height * params.dpitch
        || (uint64_t)params.src > std::numeric_limits<uint64_t>::max() - params.height * params.spitch) {
        BM_LOG_ERROR("length of dst or src address with max address length cannot exceed max value of uint64_t.");
        return BM_INVALID_PARAM;
    }

    if ((uint64_t)params.dest + params.height * params.dpitch > SVM_END_ADDR
        || (uint64_t)params.src + params.height * params.spitch > SVM_END_ADDR) {
        BM_LOG_ERROR("copy addr exceeds available address.");
        return BM_INVALID_PARAM;
    }
    return BM_OK;
}

int HostDataOpSDMA::CopyDevice2Gva2d(hybm_copy_2d_params &params, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }

    int status = CheckDevice2Gva2dStatus(params);
    if (status != BM_OK) {
        return status;
    }

    int ret = BM_OK;
    for (uint64_t i = 0; i < params.height; ++i) {
        void *dstAddr = reinterpret_cast<void *>((uint64_t)params.dest + i * params.dpitch);
        void *srcAddr = reinterpret_cast<void *>((uint64_t)params.src + i * params.spitch);
        auto asyncRet = DlAclApi::AclrtMemcpyAsync(dstAddr, params.width,
                                                   srcAddr, params.width, ACL_MEMCPY_DEVICE_TO_DEVICE, st);
        if (asyncRet != 0) {
            BM_LOG_ERROR("copy2d memory on gva to device failed:: "
                         << asyncRet << " dpitch: " << params.dpitch
                         << " spitch: " << params.spitch << " width: " << params.width
                         << " height:" << params.height);
            ret = asyncRet;
            break;
        }
    }

    int syncRet = DlAclApi::AclrtSynchronizeStream(st);
    if (syncRet != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << syncRet);
        ret = syncRet;
    }
    return ret;
}

int HostDataOpSDMA::CopyGva2Host2d(hybm_copy_2d_params &params, void *stream) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, params.width * params.height, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return ret;
    }
    void *dest = params.dest;
    params.dest = copyDevice;
    auto result = CopyGva2Device2d(params, stream);
    if (result != BM_OK) {
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("device memory free failed, ret: " << free_ret);
        }
        return result;
    }

    ret = DlAclApi::AclrtMemcpy2d(dest, params.dpitch, copyDevice,
        params.width, params.width, params.height, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != 0) {
        BM_LOG_ERROR("copy data on temp DEVICE to GVA failed: " << ret << " spitch: " << params.spitch
                                                                << " width: " << params.width
                                                                << " height:" << params.height);
        BM_LOG_ERROR("copy data on temp DEVICE to GVA failed: " << ret << " spitch: " << params.spitch
                                                                << " width: " << params.width
                                                                << " height:" << params.height);
        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("device memory free failed, ret: " << free_ret);
        }
        return ret;
    }

    int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
    if (free_ret != 0) {
        BM_LOG_ERROR("device memory free failed, ret: " << free_ret);
    }
    return BM_OK;
}

int HostDataOpSDMA::CopyGva2Device2d(hybm_copy_2d_params &params, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }

    int ret = BM_OK;
    for (uint64_t i = 0; i < params.height; ++i) {
        void *dstAddr = reinterpret_cast<void *>((uint64_t)params.dest + i * params.dpitch);
        void *srcAddr = reinterpret_cast<void *>((uint64_t)params.src + i * params.spitch);
        auto asyncRet = DlAclApi::AclrtMemcpyAsync(dstAddr, params.width, srcAddr, params.width,
                                                   ACL_MEMCPY_DEVICE_TO_DEVICE, st);
        if (asyncRet != 0) {
            BM_LOG_ERROR("copy2d memory on gva to device failed:: "
                         << asyncRet << " spitch: " << params.spitch
                         << " dpitch: " << params.dpitch << " width: "
                         << params.width << " height:" << params.height);
            ret = asyncRet;
            break;
        }
    }

    int syncRet = DlAclApi::AclrtSynchronizeStream(st);
    if (syncRet != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << syncRet);
        ret = syncRet;
    }
    return ret;
}

int HostDataOpSDMA::DataCopy2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction,
                               const ExtOptions &options) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    BM_ASSERT_RETURN(params.dest != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(params.src != nullptr, BM_INVALID_PARAM);
    auto ret = PrepareThreadLocalStream();
    if (ret != BM_OK) {
        return ret;
    }
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = CopyDevice2Gva2d(params, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE:
            ret = CopyGva2Device2d(params, options.stream);
            break;
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE:
            ret = CopyHost2Gva2d(params, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST:
            ret = CopyGva2Host2d(params, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = CopyDevice2Gva2d(params, options.stream);
            break;

        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int32_t HostDataOpSDMA::DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                                      const ExtOptions &options) noexcept
{
    BM_LOG_ERROR("not supported data copy async!");
    return BM_ERROR;
}

int32_t HostDataOpSDMA::Wait(int32_t waitId) noexcept
{
    BM_LOG_ERROR("not supported data copy wait!");
    return BM_ERROR;
}

int HostDataOpSDMA::BatchCopyDevice2Gva(void *gvaAddrs[], const void *deviceAddrs[], const size_t counts[],
                                        uint32_t batchSize, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }
    auto ret = 0;
    for (auto i = 0U; i < batchSize; i++) {
        ret = DlAclApi::AclrtMemcpyAsync(gvaAddrs[i], counts[i], deviceAddrs[i], counts[i],
                                         ACL_MEMCPY_DEVICE_TO_DEVICE, st);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local device to GVA failed: " << ret);
            return ret;
        }
    }

    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret);
        return ret;
    }

    return BM_OK;
}

int HostDataOpSDMA::BatchCopyGva2Device(void *deviceAddrs[], const void *gvaAddrs[], const size_t counts[],
                                        uint32_t batchSize, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }
    auto ret = 0;
    for (auto i = 0U; i < batchSize; i++) {
        ret = DlAclApi::AclrtMemcpyAsync(deviceAddrs[i], counts[i], gvaAddrs[i], counts[i],
                                         ACL_MEMCPY_DEVICE_TO_DEVICE, st);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on GVA to local device failed: " << ret);
            return ret;
        }
    }

    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret);
        return ret;
    }

    return BM_OK;
}

int HostDataOpSDMA::BatchCopyHost2Gva(void *deviceAddrs[], const void *gvaAddrs[], const size_t counts[],
                                      uint32_t batchSize, void *stream) noexcept
{
    for (uint32_t i = 0; i < batchSize; ++i) {
        const void* hostAddr = deviceAddrs[i];
        const void* gvaAddr = gvaAddrs[i];
        size_t count = counts[i];
        
        if (hostAddr == nullptr || gvaAddr == nullptr || count == 0) {
            BM_LOG_WARN("Invalid parameters at index: " << i << " skipping");
            continue;
        }
        
        void *copyDevice;
        auto ret = DlAclApi::AclrtMalloc(&copyDevice, count, 0);
        if (ret != 0) {
            BM_LOG_ERROR("allocate temp copy memory on local device failed at index: " << i << " ret:" << ret);
            return ret;
        }

        ret = DlAclApi::AclrtMemcpy(copyDevice, count, hostAddr, count, ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != 0) {
            BM_LOG_ERROR("copy host data to temp memory failed at index: " << i << " ret:" << ret);
            int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
            if (free_ret != 0) {
                BM_LOG_ERROR("device memory free failed at index: " << i << " ret:" << free_ret);
            }
            return ret;
        }

        auto result = CopyDevice2Gva(const_cast<void*>(gvaAddr), static_cast<const void*>(copyDevice), count, stream);
        if (result != BM_OK) {
            BM_LOG_ERROR("copy device data to GVA failed at index: " << i << " ret:" << result);
            int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
            if (free_ret != 0) {
                BM_LOG_ERROR("device memory free failed at index: " << i << " ret:" << free_ret);
            }
            return result;
        }

        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("device memory free failed at index: " << i << " ret:" << free_ret);
        }
    }
    return BM_OK;
}

int HostDataOpSDMA::BatchCopyGva2Host(void *deviceAddrs[], const void *gvaAddrs[], const size_t counts[],
                                      uint32_t batchSize, void *stream) noexcept
{
    for (uint32_t i = 0; i < batchSize; ++i) {
        void* hostAddr = deviceAddrs[i];
        const void* gvaAddr = gvaAddrs[i];
        size_t count = counts[i];
        
        if (hostAddr == nullptr || gvaAddr == nullptr || count == 0) {
            BM_LOG_WARN("Invalid parameters at index: " << i << " skipping");
            continue;
        }

        void *copyDevice;
        auto ret = DlAclApi::AclrtMalloc(&copyDevice, count, 0);
        if (ret != 0) {
            BM_LOG_ERROR("allocate temp copy memory failed at index: " << i << " ret:" << ret);
            return ret;
        }

        auto result = CopyGva2Device(copyDevice, gvaAddr, count, stream);
        if (result != BM_OK) {
            BM_LOG_ERROR("copy GVA to temp device memory failed at index: " << i << " ret:" << result);
            int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
            if (free_ret != 0) {
                BM_LOG_ERROR("device memory free failed at index: " << i << " ret:" << free_ret);
            }
            return result;
        }

        ret = DlAclApi::AclrtMemcpy(hostAddr, count, copyDevice, count, ACL_MEMCPY_DEVICE_TO_HOST);
        if (ret != 0) {
            BM_LOG_ERROR("copy temp device memory to host failed at index: " << i << " ret:" << ret);
            int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
            if (free_ret != 0) {
                BM_LOG_ERROR("device memory free failed at index: " << i << " ret:" << free_ret);
            }
            return ret;
        }

        int32_t free_ret = DlAclApi::AclrtFree(copyDevice);
        if (free_ret != 0) {
            BM_LOG_ERROR("device memory free failed at index: " << i << " ret:" << free_ret);
        }
    }
    return BM_OK;
}
}  // namespace mf
}  // namespace ock
