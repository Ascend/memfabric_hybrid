/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_data_operator_sdma.h"

#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "hybm_ptracer.h"
#include "hybm_gvm_user.h"
#include "hybm_data_op.h"

namespace ock {
namespace mf {
constexpr uint64_t HBM_SWAP_SPACE_SIZE = 128 * 1024 * 1024;
HostDataOpSDMA::HostDataOpSDMA(void *stm) noexcept : stream_{stm} {}

int32_t HostDataOpSDMA::Initialize() noexcept
{
    if (inited_) {
        return BM_OK;
    }

    if (HybmGvmHasInited()) {
        uint32_t devId = static_cast<uint32_t>(HybmGetInitedLogicDeviceId());
        hybmStream_ = std::make_shared<HybmStream>(devId, 0, 0);
        BM_ASSERT_RETURN(hybmStream_ != nullptr, BM_MALLOC_FAILED);

        auto ret = hybmStream_->Initialize();
        if (ret != 0) {
            BM_LOG_ERROR("create stream failed, dev:" << devId << " ret:" << ret);
            hybmStream_ = nullptr;
            return BM_ERROR;
        }

        ret = DlAclApi::AclrtMalloc(&sdmaSwapMemAddr_, HBM_SWAP_SPACE_SIZE, 0);
        if (ret != 0 || !sdmaSwapMemAddr_) {
            hybmStream_->Destroy();
            hybmStream_ = nullptr;
            BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }

        sdmaSwapMemoryAllocator_ = std::make_shared<RbtreeRangePool>((uint8_t *) sdmaSwapMemAddr_, HBM_SWAP_SPACE_SIZE);
        ret = hybm_gvm_mem_register((uint64_t)sdmaSwapMemAddr_, HBM_SWAP_SPACE_SIZE);
        if (ret != BM_OK) {
            DlAclApi::AclrtFree(&sdmaSwapMemAddr_);
            sdmaSwapMemAddr_ = nullptr;
            hybmStream_->Destroy();
            hybmStream_ = nullptr;
            BM_LOG_ERROR("hybm_gvm_mem_fetch failed: " << ret << " addr:" << sdmaSwapMemAddr_);
            return BM_DL_FUNCTION_FAILED;
        }
        BM_LOG_INFO("Success to register sdma swap memory add: " << sdmaSwapMemAddr_
            << " length:" << HBM_SWAP_SPACE_SIZE);
    }

    inited_ = true;
    return BM_OK;
}

void HostDataOpSDMA::UnInitialize() noexcept
{
    if (!inited_) {
        return;
    }

    if (sdmaSwapMemoryAllocator_) {
        DlAclApi::AclrtFree(&sdmaSwapMemAddr_);
        sdmaSwapMemAddr_ = nullptr;
    }

    if (hybmStream_ != nullptr) {
        hybmStream_->Destroy();
        hybmStream_ = nullptr;
    }
    inited_ = false;
}

int32_t HostDataOpSDMA::DataCopy(const void *srcVA, void *destVA, uint64_t length, hybm_data_copy_direction direction,
                                 const ExtOptions &options) noexcept
{
    if (options.flags & ASYNC_COPY_FLAG) {
        return DataCopyAsync(srcVA, destVA, length, direction, options);
    }
    int ret;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LD_TO_GD);
            ret = CopyLD2GD(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_LD);
            ret = CopyGD2LD(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_LD, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LH_TO_GD);
            ret = CopyLH2GD(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_LH);
            ret = CopyGD2LH(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_GD);
            ret = CopyGD2GD(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_GH);
            ret = CopyGD2GH(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_GD);
            ret = CopyGH2GD(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_GD);
            ret = CopyGH2GH(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LD_TO_GH);
            ret = CopyLD2GH(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LD_TO_GH, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LH_TO_GH);
            ret = CopyLH2GH(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_LH);
            ret = CopyGH2LH(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_LD);
            ret = CopyGH2LD(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_LD, ret);
            break;
        }
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int HostDataOpSDMA::CopyLH2GD(void *gvaAddr, const void *hostAddr, size_t count, void *stream) noexcept
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

    auto result = CopyLD2GD(gvaAddr, copyDevice, count, stream);
    if (result != BM_OK) {
        DlAclApi::AclrtFree(copyDevice);
        return result;
    }

    DlAclApi::AclrtFree(copyDevice);
    return BM_OK;
}

int HostDataOpSDMA::CopyGD2GD(void* gvaAddr, const void* deviceAddr, size_t count, void* stream) noexcept
{
    return CopyLD2GD(gvaAddr, deviceAddr, count, stream);
}

int HostDataOpSDMA::CopyLD2GD(void *gvaAddr, const void *deviceAddr, size_t count, void *stream) noexcept
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

int HostDataOpSDMA::CopyGD2LD(void *deviceAddr, const void *gvaAddr, size_t count, void *stream) noexcept
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

int HostDataOpSDMA::CopyGD2LH(void *hostAddr, const void *gvaAddr, size_t count, void *stream) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, count, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyGD2LD(copyDevice, gvaAddr, count, stream);
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

int HostDataOpSDMA::CopyLH2GD2d(void* gvaAddr, uint64_t dpitch, const void* hostAddr, uint64_t spitch, size_t width,
                                uint64_t height, void* stream) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, width * height, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtMemcpy2d(copyDevice, width, hostAddr, spitch, width, height, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR("copy2d host data to temp copy memory on local device failed: " << ret
                     << " spitch: " << spitch << " dpitch: " << width << " width: " << width << " height:" << height);
        DlAclApi::AclrtFree(copyDevice);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyLD2GD2d(gvaAddr, dpitch, copyDevice, width, width, height, stream);
    if (result != BM_OK) {
        DlAclApi::AclrtFree(copyDevice);
        return result;
    }

    DlAclApi::AclrtFree(copyDevice);
    return BM_OK;
}

int HostDataOpSDMA::CopyLD2GD2d(void *gvaAddr, uint64_t dpitch, const void *deviceAddr, uint64_t spitch,
                                size_t width, uint64_t height, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }

    int ret = BM_OK;
    for (uint64_t i = 0; i < height; ++i) {
        void *dstAddr = reinterpret_cast<void *>((uint64_t) gvaAddr + i * dpitch);
        void *srcAddr = reinterpret_cast<void *>((uint64_t) deviceAddr + i * spitch);
        auto asyncRet = DlAclApi::AclrtMemcpyAsync(dstAddr, width, srcAddr, width,
                                                   ACL_MEMCPY_DEVICE_TO_DEVICE, st);
        if (asyncRet != 0) {
            BM_LOG_ERROR("copy2d memory on gva to device failed:: " << asyncRet
                         << " dpitch: " << dpitch << " spitch: " << spitch << " width: " << width
                         << " height:" << height << " stream:" << st);
            ret = asyncRet;
        }
    }

    int syncRet  = DlAclApi::AclrtSynchronizeStream(st);
    if (syncRet != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << syncRet << " stream:" << st);
        ret = syncRet;
    }
    return ret;
}

int HostDataOpSDMA::CopyGD2LH2d(void *hostAddr, uint64_t dpitch, const void *gvaAddr, uint64_t spitch,
                                size_t width, uint64_t height, void *stream) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, width * height, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyGD2LD2d(copyDevice, width, gvaAddr, spitch, width, height, stream);
    if (result != BM_OK) {
        DlAclApi::AclrtFree(copyDevice);
        return result;
    }

    ret = DlAclApi::AclrtMemcpy2d(hostAddr, dpitch, copyDevice, width, width, height, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != 0) {
        BM_LOG_ERROR("copy data on temp DEVICE to GVA failed: " << ret
                     << " spitch: " << spitch << " width: " << width << " height:" << height);
        DlAclApi::AclrtFree(copyDevice);
        return BM_DL_FUNCTION_FAILED;
    }

    DlAclApi::AclrtFree(copyDevice);
    return BM_OK;
}

int HostDataOpSDMA::CopyGD2LD2d(void *deviceAddr, uint64_t dpitch, const void *gvaAddr, uint64_t spitch,
                                size_t width, uint64_t height, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }

    int ret = BM_OK;
    for (uint64_t i = 0; i < height; ++i) {
        void *dstAddr = reinterpret_cast<void *>((uint64_t) deviceAddr + i * dpitch);
        void *srcAddr = reinterpret_cast<void *>((uint64_t) gvaAddr + i * spitch);
        auto asyncRet = DlAclApi::AclrtMemcpyAsync(dstAddr, width, srcAddr, width,
                                                   ACL_MEMCPY_DEVICE_TO_DEVICE, st);
        if (asyncRet != 0) {
            BM_LOG_ERROR("copy2d memory on gva to device failed:: " << asyncRet
                         << " spitch: " << spitch << " dpitch: " << dpitch << " width: " << width
                         << " height:" << height << " stream:" << st);
            ret = asyncRet;
        }
    }

    int syncRet  = DlAclApi::AclrtSynchronizeStream(st);
    if (syncRet != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << syncRet << " stream:" << st);
        ret = syncRet;
    }
    return ret;
}

int HostDataOpSDMA::DataCopy2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                               uint64_t width, uint64_t height, hybm_data_copy_direction direction,
                               const ExtOptions &options) noexcept
{
    int ret;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = CopyLD2GD2d(destVA, dpitch, srcVA, spitch, width, height, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE:
            ret = CopyGD2LD2d(destVA, dpitch, srcVA, spitch, width, height, options.stream);
            break;
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE:
            ret = CopyLH2GD2d(destVA, dpitch, srcVA, spitch, width, height, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST:
            ret = CopyGD2LH2d(destVA, dpitch, srcVA, spitch, width, height, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = CopyLD2GD2d(destVA, dpitch, srcVA, spitch, width, height, options.stream);
            break;
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST:
            ret = CopyLD2GH2d(destVA, dpitch, srcVA, spitch, width, height, options.stream);
            break;
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE:
            ret = CopyGH2LD2d(destVA, dpitch, srcVA, spitch, width, height, options.stream);
            break;

        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int32_t HostDataOpSDMA::DataCopyAsync(const void* srcVA, void* destVA, uint64_t length,
                                      hybm_data_copy_direction direction, const ExtOptions &options) noexcept
{
    int ret;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LD_TO_GD);
            ret = CopyLD2GD(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_LD);
            ret = CopyGD2LD(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_LD, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LH_TO_GD);
            ret = CopyLH2GD(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_LH);
            ret = CopyGD2LH(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_GD);
            ret = CopyGD2GD(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_GH);
            ret = CopyGD2GH(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_GD);
            ret = CopyGH2GD(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_GD);
            ret = CopyGH2GH(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LH_TO_GH);
            ret = CopyLH2GH(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_LH);
            ret = CopyGH2LH(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_LH, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LD_TO_GH_ASYNC);
            ret = CopyLD2GHAsync(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LD_TO_GH_ASYNC, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_LD_ASYNC);
            ret = CopyGH2LDAsync(destVA, srcVA, length, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_LD_ASYNC, ret);
            break;
        }
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int32_t HostDataOpSDMA::Wait(int32_t waitId) noexcept
{
    if (hybmStream_ != nullptr) {
        return hybmStream_->Synchronize();
    }
    return BM_OK;
}

int HostDataOpSDMA::CopyGD2GH(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(HybmGvmHasInited(), "Failed to CopyGD2GH hybm gvm not init", BM_NOT_SUPPORT_FUNC);
    // HBM池拷贝到HOST池
    BM_LOG_INFO("Copy global device to global host, destVa:" << destVA << " srcVa:" << srcVA << " length:" << length);
    return CopyG2G(destVA, srcVA, length);
}

int HostDataOpSDMA::CopyGH2GD(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(HybmGvmHasInited(), "Failed to CopyGH2GD hybm gvm not init", BM_NOT_SUPPORT_FUNC);
    // HOST池到HBM池的拷贝
    BM_LOG_INFO("Copy global host to global device, destVa:" << destVA << " srcVa:" << srcVA << " length:" << length);
    return CopyG2G(destVA, srcVA, length);
}

int HostDataOpSDMA::CopyGH2GH(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(HybmGvmHasInited(), "Failed to CopyGH2GH hybm gvm not init", BM_NOT_SUPPORT_FUNC);
    // HOST池到HOST池的拷贝
    BM_LOG_INFO("Copy global host to global host, destVa:" << destVA << " srcVa:" << srcVA << " length:" << length);
    return CopyG2G(destVA, srcVA, length);
}

int HostDataOpSDMA::CopyLD2GH(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(HybmGvmHasInited(), "Failed to CopyLD2GH hybm gvm not init", BM_NOT_SUPPORT_FUNC);
    // local device到dram池的拷贝
    BM_LOG_INFO("Copy local device to global host, destVa:" << destVA << " srcVa:" << srcVA << " length:" << length);
    // LD2GD
    if (hybm_gvm_mem_has_registered((uint64_t)srcVA, length)) {
        return CopyG2G(destVA, srcVA, length);
    }

    auto tmpSdmaMemory = sdmaSwapMemoryAllocator_->Allocate(length);
    void *tmpHbm = tmpSdmaMemory.Address();
    if (tmpHbm == nullptr) {
        BM_LOG_ERROR("Failed to malloc swap srcVa: " << srcVA << " destVa: "
                                                     << destVA << " length: " << length);
        return BM_MALLOC_FAILED;
    }
    auto ret = CopyLD2GD(tmpHbm, srcVA, length, stream);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyLD2GD ret: " << ret);
        sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
        return ret;
    }
    // GD2GH
    ret = CopyG2G(destVA, tmpHbm, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyG2G ret: " << ret << " dest:"<< tmpHbm << " srcVa:" << srcVA
            << " length:" << length);
    }
    sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
    return ret;
}

int HostDataOpSDMA::CopyLH2GH(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(HybmGvmHasInited(), "Failed to CopyLH2GH hybm gvm not init", BM_NOT_SUPPORT_FUNC);
    // local host到dram池的拷贝
    BM_LOG_INFO("Copy local host to global host, destVa:" << destVA << " srcVa:" << srcVA << " length:" << length);
    if (hybm_gvm_mem_has_registered((uint64_t)srcVA, length)) {
        return CopyG2G(destVA, srcVA, length);
    }

    // LH2GD
    auto tmpSdmaMemory = sdmaSwapMemoryAllocator_->Allocate(length);
    void *tmpHbm = tmpSdmaMemory.Address();
    if (tmpHbm == nullptr) {
        BM_LOG_ERROR("Failed to malloc swap srcVa: " << srcVA << " destVa: "
                                                     << destVA << " length: " << length);
        return BM_MALLOC_FAILED;
    }
    auto ret = CopyLH2GD(tmpHbm, srcVA, length, stream);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyLD2GD ret: " << ret);
        sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
        return ret;
    }
    // GD2GH
    ret = CopyG2G(destVA, tmpHbm, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyG2G ret: " << ret << " dest:"<< tmpHbm << " srcVa:" << srcVA
            << " length:" << length);
    }
    sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
    return 0;
}

int HostDataOpSDMA::CopyGH2LD(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(HybmGvmHasInited(), "Failed to CopyGH2LD hybm gvm not init", BM_NOT_SUPPORT_FUNC);
    // dram池的拷贝到local device
    BM_LOG_INFO("Copy global host to local device, destVa:" << destVA << " srcVa:" << srcVA << " length:" << length);
    if (hybm_gvm_mem_has_registered((uint64_t)destVA, length)) {
        return CopyG2G(destVA, srcVA, length);
    }

    // GH2GD
    auto tmpSdmaMemory = sdmaSwapMemoryAllocator_->Allocate(length);
    void *tmpHbm = tmpSdmaMemory.Address();
    if (tmpHbm == nullptr) {
        BM_LOG_ERROR("Failed to malloc swap srcVa: " << srcVA << " destVa: "
                                                     << destVA << " length: " << length);
        return BM_MALLOC_FAILED;
    }
    auto ret = CopyG2G(tmpHbm, srcVA, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyG2G ret: " << ret << " dest:"<< tmpHbm << " srcVa:" << srcVA
            << " length:" << length);
        sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
        return ret;
    }
    // GD2LD
    ret = CopyGD2LD(destVA, tmpHbm, length, stream);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyLD2GD ret: " << ret << " dest:"<< destVA << " srcVa:" << tmpHbm
            << " length:" << length);
    }
    sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
    return ret;
}

int HostDataOpSDMA::CopyGH2LH(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(HybmGvmHasInited(), "Failed to CopyGH2LH hybm gvm not init", BM_NOT_SUPPORT_FUNC);
    // dram池的拷贝到local host
    BM_LOG_INFO("Copy global host to local host, destVa:" << destVA << " srcVa:" << srcVA << " length:" << length);
    if (hybm_gvm_mem_has_registered((uint64_t)destVA, length)) {
        return CopyG2G(destVA, srcVA, length);
    }

    // GH2GD
    auto tmpSdmaMemory = sdmaSwapMemoryAllocator_->Allocate(length);
    void *tmpHbm = tmpSdmaMemory.Address();
    if (tmpHbm == nullptr) {
        BM_LOG_ERROR("Failed to malloc swap srcVa: " << srcVA << " destVa: "
                                                     << destVA << " length: " << length);
        return BM_MALLOC_FAILED;
    }
    auto ret = CopyG2G(tmpHbm, srcVA, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyG2G ret: " << ret << " dest:"<< tmpHbm << " srcVa:" << srcVA
            << " length:" << length);
        sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
        return ret;
    }
    // GD2LH
    ret = CopyGD2LH(destVA, tmpHbm, length, stream);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyLD2GD ret: " << ret << " dest:"<< destVA << " srcVa:" << tmpHbm
            << " length:" << length);
    }
    sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
    return ret;
}

int HostDataOpSDMA::CopyLD2GHAsync(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept
{
    if (hybm_gvm_mem_has_registered((uint64_t)srcVA, length)) {
        return CopyG2GAsync(destVA, srcVA, length);
    }
    BM_LOG_DEBUG("Can not to direct copy, deviceId:" << HybmGetInitDeviceId() << " addr:" << srcVA
        << " length:" << length << " not register!");
    return CopyLD2GH(destVA, srcVA, length, stream);
}

int HostDataOpSDMA::CopyGH2LDAsync(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept
{
    if (hybm_gvm_mem_has_registered((uint64_t)destVA, length)) {
        return CopyG2GAsync(destVA, srcVA, length);
    }
    BM_LOG_DEBUG("Can not to direct copy, deviceId:" << HybmGetInitDeviceId() << " addr:" << destVA
        << " length:" << length << " not register!");
    return CopyGH2LD(destVA, srcVA, length, stream);
}

void HostDataOpSDMA::InitG2GStreamTask(StreamTask &task) noexcept
{
    task.type = STREAM_TASK_TYPE_SDMA;
    rtStarsMemcpyAsyncSqe_t *const sqe = &(task.sqe.memcpyAsyncSqe);
    sqe->header.type = RT_STARS_SQE_TYPE_SDMA;
    sqe->header.ie = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.pre_p = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.wr_cqe = 0U;
    sqe->header.rt_stream_id = hybmStream_->GetId();
    sqe->header.task_id = 0;

    sqe->kernelCredit = RT_STARS_DEFAULT_KERNEL_CREDIT;
    sqe->ptrMode = 0;
    sqe->opcode = 0U;

    sqe->src_streamid = 0U; // get sid and ssid from sq, leave 0 here
    sqe->dst_streamid = 0U;
    sqe->src_sub_streamid = 0U;
    sqe->dstSubStreamId = 0U;
    sqe->ie2 = 0U;
    sqe->sssv = 1U;
    sqe->dssv = 1U;
    sqe->sns = 1U;
    sqe->dns = 1U;
    sqe->qos = 6U;
    sqe->sro = 0U;
    sqe->dro = 0U;
    sqe->partid = 0U;
    sqe->mpam = 0U;

    sqe->res3 = 0U;
    sqe->res4 = 0U;
    sqe->res5 = 0U;
    sqe->res6 = 0U;

    sqe->d2dOffsetFlag = 0U;
    sqe->srcOffsetLow = 0U;
    sqe->dstOffsetLow = 0U;
    sqe->srcOffsetHigh = 0U;
    sqe->dstOffsetHigh = 0U;
}

int HostDataOpSDMA::CopyG2G(void *destVA, const void *srcVA, size_t count) noexcept
{
    StreamTask task{};
    InitG2GStreamTask(task);
    rtStarsMemcpyAsyncSqe_t *const sqe = &(task.sqe.memcpyAsyncSqe);
    sqe->length = count;
    sqe->src_addr_low =
            static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(srcVA)) & 0x00000000FFFFFFFFU);
    sqe->src_addr_high = static_cast<uint32_t>(
            (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(srcVA)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);
    sqe->dst_addr_low =
            static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(destVA)) & 0x00000000FFFFFFFFU);
    sqe->dst_addr_high = static_cast<uint32_t>(
            (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(destVA)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);

    auto ret = hybmStream_->SubmitTasks(task);
    BM_ASSERT_RETURN(ret == 0, BM_ERROR);

    ret = hybmStream_->Synchronize();
    BM_ASSERT_RETURN(ret == 0, BM_ERROR);
    return BM_OK;
}


int HostDataOpSDMA::CopyG2G2d(void* destVA, uint64_t dpitch, const void* srcVA, uint64_t spitch,
                              size_t width, uint64_t height) noexcept
{
    StreamTask task{};
    InitG2GStreamTask(task);
    rtStarsMemcpyAsyncSqe_t *const sqe = &(task.sqe.memcpyAsyncSqe);
    for (size_t i = 0; i < height; ++i) {
        void *dst = reinterpret_cast<void *>((uint64_t) destVA + i * dpitch);
        void *src = reinterpret_cast<void *>((uint64_t) srcVA + i * spitch);
        sqe->length = width;
        sqe->src_addr_low =
                static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(src)) & 0x00000000FFFFFFFFU);
        sqe->src_addr_high = static_cast<uint32_t>(
                (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(src)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);
        sqe->dst_addr_low =
                static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(dst)) & 0x00000000FFFFFFFFU);
        sqe->dst_addr_high = static_cast<uint32_t>(
                (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(dst)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);
        auto ret = hybmStream_->SubmitTasks(task);
        BM_ASSERT_LOG_AND_RETURN(ret == 0, "Failed to submit g2g task ret:" << ret, BM_ERROR);
    }

    auto ret = hybmStream_->Synchronize();
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "Failed to wait g2g task ret:" << ret, BM_ERROR);
    return BM_OK;
}

int HostDataOpSDMA::CopyG2GAsync(void *destVA, const void *srcVA, size_t count) noexcept
{
    BM_LOG_DEBUG("[TEST] CopyG2GAsync srcVA:" << srcVA << " destVA:" << destVA << " count:" << count);
    StreamTask task{};
    InitG2GStreamTask(task);
    rtStarsMemcpyAsyncSqe_t *const sqe = &(task.sqe.memcpyAsyncSqe);
    sqe->length = count;
    sqe->src_addr_low =
            static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(srcVA)) & 0x00000000FFFFFFFFU);
    sqe->src_addr_high = static_cast<uint32_t>(
            (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(srcVA)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);
    sqe->dst_addr_low =
            static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(destVA)) & 0x00000000FFFFFFFFU);
    sqe->dst_addr_high = static_cast<uint32_t>(
            (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(destVA)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);

    BM_LOG_DEBUG("[TEST] submit task"<< " src:" << task.sqe.memcpyAsyncSqe.src_addr_low << "~"
                                     << task.sqe.memcpyAsyncSqe.src_addr_high
                                     << " dest:" << task.sqe.memcpyAsyncSqe.dst_addr_low << "~"
                                     << task.sqe.memcpyAsyncSqe.dst_addr_high
                                     << " length:" << task.sqe.memcpyAsyncSqe.length);
    TP_TRACE_BEGIN(TP_HYBM_SDMA_SUBMIT_G2G_TASK);
    auto ret = hybmStream_->SubmitTasks(task);
    TP_TRACE_END(TP_HYBM_SDMA_SUBMIT_G2G_TASK, ret);
    BM_ASSERT_RETURN(ret == 0, BM_ERROR);
    return BM_OK;
}

int
HostDataOpSDMA::BatchCopyG2G(void **destVAs, const void **srcVAs, const uint32_t *counts, uint32_t batchSize) noexcept
{
    auto ret = 0;
    auto asyncRet = 0;
    for (auto i = 0U; i < batchSize; i++) {
        auto srcAddr = srcVAs[i];
        auto destAddr = destVAs[i];
        auto count = counts[i];
        asyncRet = CopyG2GAsync(destAddr, srcAddr, count);
        if (asyncRet !=  0) {
            BM_LOG_ERROR("BatchCopyLD2GH failed:" << asyncRet << " index:" << i << " src:" << srcAddr
                                                  << " dest:" << destAddr << " length:" << count);
            ret = asyncRet;
        }
    }

    TP_TRACE_BEGIN(TP_HYBM_SDMA_GET_WAIT);
    asyncRet = Wait(0);
    TP_TRACE_END(TP_HYBM_SDMA_GET_WAIT, ret);
    if (asyncRet != 0) {
        BM_LOG_ERROR("BatchCopyLD2GH wait copy failed:" << asyncRet);
        ret = asyncRet;
    }
    return ret;
}

int HostDataOpSDMA::CopyGH2LD2d(void *deviceAddr, uint64_t dpitch, const void *gvaAddr, uint64_t spitch, size_t width,
                                uint64_t height, void *stream) noexcept
{
    if (spitch != width) {
        BM_LOG_ERROR("Invalid param spitch: " << spitch << " width: " << width);
        return BM_INVALID_PARAM;
    }
    if (hybm_gvm_mem_has_registered((uint64_t)deviceAddr, width)) {
        return CopyG2G2d(deviceAddr, dpitch, gvaAddr, spitch, width, height);
    }
    uint64_t length = height * width;
    auto tmpSdmaMemory = sdmaSwapMemoryAllocator_->Allocate(length);
    void *tmpHbm = tmpSdmaMemory.Address();
    if (tmpHbm == nullptr) {
        BM_LOG_ERROR("Failed to malloc swap length: " << length << " width: " << width << " height: " << height);
        return BM_MALLOC_FAILED;
    }
    auto ret = CopyG2G(tmpHbm, gvaAddr, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyG2G ret: " << ret << " dest:"<< tmpHbm << " srcVa:" << gvaAddr
                                               << " length:" << length);
        sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
        return ret;
    }

    ret = CopyGD2LD2d(deviceAddr, dpitch, tmpHbm, spitch, width, height, stream);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyGD2LD2d ret: " << ret << " dest:"<< deviceAddr << " srcVa:" << tmpHbm
                     << " spitch: " << spitch << " dpitch: " << dpitch << " width: " << width
                     << " height:" << height << " stream:" << stream);
        sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
    }
    return ret;
}

int HostDataOpSDMA::CopyLD2GH2d(void *gvaAddr, uint64_t dpitch, const void *deviceAddr, uint64_t spitch, size_t width,
                                uint64_t height, void *stream) noexcept
{
    if (dpitch != width) {
        BM_LOG_ERROR("Invalid param dpitch: " << dpitch << " width: " << width);
        return BM_INVALID_PARAM;
    }
    if (hybm_gvm_mem_has_registered((uint64_t)deviceAddr, width)) {
        return CopyG2G2d(gvaAddr, dpitch, deviceAddr, spitch, width, height);
    }
    uint64_t length = height * width;
    auto tmpSdmaMemory = sdmaSwapMemoryAllocator_->Allocate(length);
    void *tmpHbm = tmpSdmaMemory.Address();
    if (tmpHbm == nullptr) {
        BM_LOG_ERROR("Failed to malloc swap length: " << length << " width: " << width << " height: " << height);
        return BM_MALLOC_FAILED;
    }

    auto ret = CopyLD2GD2d(tmpHbm, dpitch, deviceAddr, spitch, width, height, stream);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyLD2GD2d ret: " << ret << " dest:"<< deviceAddr << " srcVa:" << tmpHbm
                     << " spitch: " << spitch << " dpitch: " << dpitch << " width: " << width
                     << " height:" << height << " stream:" << stream);
        sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
        return ret;
    }
    ret = CopyG2G(gvaAddr, tmpHbm, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyG2G ret: " << ret << " dest:"<< gvaAddr << " srcVa:" << tmpHbm
                                               << " length:" << length);
        sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
    }
    return ret;
}

int32_t HostDataOpSDMA::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                      const ExtOptions &options) noexcept
{
    auto ret = 0;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_LD_TO_GH);
            ret = BatchCopyLD2GH(params.destinations, params.sources, params.dataSizes,
                                 params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_LD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GH_TO_LD);
            ret = BatchCopyGH2LD(params.destinations, params.sources, params.dataSizes,
                                 params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_GH_TO_LD, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_LD_TO_GD);
            ret = BatchCopyLD2GD(params.destinations, params.sources, params.dataSizes,
                                 params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GD_TO_LD);
            ret = BatchCopyGD2LD(params.destinations, params.sources, params.dataSizes,
                                 params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_GD_TO_LD, ret);
            break;
        }
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int HostDataOpSDMA::BatchCopyLD2GH(void **gvaAddrs, const void **deviceAddrs, const uint32_t *counts,
                                   uint32_t batchSize, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }
    if (hybm_gvm_mem_has_registered((uint64_t)deviceAddrs[0], counts[0])) {
        return BatchCopyG2G(gvaAddrs, deviceAddrs, counts, batchSize);
    }
    auto ret = 0;
    uint64_t totalSize = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        totalSize += counts[i];
    }
    auto tmpSdmaMemory = sdmaSwapMemoryAllocator_->Allocate(totalSize);
    void *tmpHbm = tmpSdmaMemory.Address();
    if (tmpHbm == nullptr) {
        BM_LOG_ERROR("Failed to malloc swap length: " << totalSize);
        return BM_MALLOC_FAILED;
    }
    uint64_t offset = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        auto srcAddr = deviceAddrs[i];
        auto destAddr = (void *)((uint64_t) tmpHbm + offset);
        auto count = counts[i];
        offset += count;
        ret = DlAclApi::AclrtMemcpyAsync(destAddr, count, srcAddr, count, ACL_MEMCPY_DEVICE_TO_DEVICE, st);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local device to GVA failed: " << ret << " stream:" << st);
            sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    ret  = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << st);
        sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
        return ret;
    }
    ret = CopyG2G(gvaAddrs[0], tmpHbm, totalSize);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to g2g ret:" << ret << " src:" <<  gvaAddrs[0]
                                          << " tmpHbm:" << tmpHbm << " length: " << totalSize);
    }
    sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
    return ret;
}

int HostDataOpSDMA::BatchCopyGH2LD(void **deviceAddrs, const void **gvaAddrs, const uint32_t *counts,
                                   uint32_t batchSize, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }
    if (hybm_gvm_mem_has_registered((uint64_t)deviceAddrs[0], counts[0])) {
        return BatchCopyG2G(deviceAddrs, gvaAddrs, counts, batchSize);
    }
    auto ret = 0;
    uint64_t totalSize = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        totalSize += counts[i];
    }
    auto tmpSdmaMemory = sdmaSwapMemoryAllocator_->Allocate(totalSize);
    void *tmpHbm = tmpSdmaMemory.Address();
    if (tmpHbm == nullptr) {
        BM_LOG_ERROR("Failed to malloc swap length: " << totalSize);
        return BM_MALLOC_FAILED;
    }
    ret = CopyG2G(tmpHbm, gvaAddrs[0], totalSize);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to g2g ret:" << ret << " src:" <<  gvaAddrs[0]
            << " tmpHbm:" << tmpHbm << " length: " << totalSize);
        sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
        return ret;
    }

    uint64_t offset = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        auto destAddr = deviceAddrs[i];
        auto srcAddr = (void *)((uint64_t) tmpHbm + offset);
        auto count = counts[i];
        offset += count;
        ret = DlAclApi::AclrtMemcpyAsync(destAddr, count, srcAddr, count, ACL_MEMCPY_DEVICE_TO_DEVICE, st);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local device to GVA failed: " << ret << " stream:" << st);
            sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    ret  = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << st);
    }
    sdmaSwapMemoryAllocator_->Release(tmpSdmaMemory);
    return ret;
}

int HostDataOpSDMA::BatchCopyLD2GD(void **gvaAddrs, const void **deviceAddrs, const uint32_t *counts,
                                   uint32_t batchSize, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }
    auto ret = 0;

    for (auto i = 0U; i < batchSize; i++) {
        ret = CopyG2GAsync(gvaAddrs[i], deviceAddrs[i], counts[i]);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local device to GVA failed: " << ret);
            return ret;
        }
    }
    ret = Wait(0);
    if (ret != 0) {
        BM_LOG_ERROR("g2g wait failed: " << ret);
        return ret;
    }
    return BM_OK;
}

int HostDataOpSDMA::BatchCopyGD2LD(void **deviceAddrs, const void **gvaAddrs, const uint32_t *counts,
                                   uint32_t batchSize, void *stream) noexcept
{
    void *st = stream_;
    if (stream != nullptr) {
        st = stream;
    }
    auto ret = 0;

    for (auto i = 0U; i < batchSize; i++) {
        ret = CopyG2GAsync(deviceAddrs[i], gvaAddrs[i], counts[i]);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local device to GVA failed: " << ret);
            return ret;
        }
    }
    ret = Wait(0);
    if (ret != 0) {
        BM_LOG_ERROR("g2g wait failed: " << ret);
        return ret;
    }

    return BM_OK;
}
}
}
