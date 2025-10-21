/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_data_op_sdma.h"

#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "hybm_ptracer.h"
#include "hybm_gvm_user.h"
#include "hybm_data_op.h"
#include "hybm_stream_manager.h"

namespace ock {
namespace mf {
constexpr uint64_t HBM_SWAP_SPACE_SIZE = 128 * 1024 * 1024;

HostDataOpSDMA::HostDataOpSDMA() noexcept = default;

HostDataOpSDMA::~HostDataOpSDMA()
{
    HostDataOpSDMA::UnInitialize();
}

int32_t HostDataOpSDMA::Initialize() noexcept
{
    if (inited_) {
        return BM_OK;
    }

    if (HybmGvmHasInited()) {
        auto ret = DlAclApi::AclrtMalloc(&sdmaSwapMemAddr_, HBM_SWAP_SPACE_SIZE, 0);
        if (ret != 0 || !sdmaSwapMemAddr_) {
            BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }

        sdmaSwapMemoryAllocator_ = std::make_shared<RbtreeRangePool>((uint8_t *) sdmaSwapMemAddr_, HBM_SWAP_SPACE_SIZE);
        ret = hybm_gvm_mem_register((uint64_t)sdmaSwapMemAddr_, HBM_SWAP_SPACE_SIZE, (uint64_t)sdmaSwapMemAddr_);
        if (ret != BM_OK) {
            DlAclApi::AclrtFree(&sdmaSwapMemAddr_);
            sdmaSwapMemAddr_ = nullptr;
            BM_LOG_ERROR("hybm_gvm_mem_register failed: " << ret << " addr:" << sdmaSwapMemAddr_);
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
    inited_ = false;
}

int32_t HostDataOpSDMA::DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                                 const ExtOptions &options) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    if (options.flags & ASYNC_COPY_FLAG) {
        return DataCopyAsync(params, direction, options);
    }
    int ret;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LD_TO_GD);
            ret = CopyLD2GD(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_LD);
            ret = CopyGD2LD(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_LD, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LH_TO_GD);
            ret = CopyLH2GD(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_LH);
            ret = CopyGD2LH(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_GD);
            ret = CopyGD2GD(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_GH);
            ret = CopyGD2GH(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_GD);
            ret = CopyGH2GD(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_GD);
            ret = CopyGH2GH(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LD_TO_GH);
            ret = CopyLD2GH(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LD_TO_GH, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LH_TO_GH);
            ret = CopyLH2GH(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_LH);
            ret = CopyGH2LH(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_LD);
            ret = CopyGH2LD(params.dest, params.src, params.dataSize, options.stream);
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
    void *st = stream;

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
    void *st = stream;

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

int HostDataOpSDMA::CopyLH2GD2d(void* gvaAddr, const void* hostAddr, hybm_copy_2d_params &params,
                                void* stream) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, params.width * params.height, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtMemcpy2d(copyDevice, params.width, hostAddr, params.spitch, params.width, params.height,
                                  ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR("copy2d host data to temp copy memory on local device failed: " << ret
                     << " spitch: " << params.spitch << " dpitch: " << params.dpitch << " width: " << params.width
                     << " height:" << params.height);
        DlAclApi::AclrtFree(copyDevice);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyLD2GD2d(gvaAddr, copyDevice, params, stream);
    if (result != BM_OK) {
        DlAclApi::AclrtFree(copyDevice);
        return result;
    }

    DlAclApi::AclrtFree(copyDevice);
    return BM_OK;
}

int HostDataOpSDMA::CopyLD2GD2d(void *gvaAddr, const void *deviceAddr, hybm_copy_2d_params &params,
                                void *stream) noexcept
{
    void *st = stream;

    int ret = BM_OK;
    for (uint64_t i = 0; i < params.height; ++i) {
        void *dstAddr = reinterpret_cast<void *>((uint64_t) gvaAddr + i * params.dpitch);
        void *srcAddr = reinterpret_cast<void *>((uint64_t) deviceAddr + i * params.spitch);
        auto asyncRet = DlAclApi::AclrtMemcpyAsync(dstAddr, params.width, srcAddr, params.width,
                                                   ACL_MEMCPY_DEVICE_TO_DEVICE, st);
        if (asyncRet != 0) {
            BM_LOG_ERROR("copy2d memory on gva to device failed:: " << asyncRet
                         << " dpitch: " << params.dpitch << " spitch: " << params.spitch << " width: " << params.width
                         << " height:" << params.height << " stream:" << st);
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

int HostDataOpSDMA::CopyGD2LH2d(void *hostAddr, const void *gvaAddr, hybm_copy_2d_params &params, void *stream) noexcept
{
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, params.width * params.height, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyGD2LD2d(copyDevice, gvaAddr, params, stream);
    if (result != BM_OK) {
        DlAclApi::AclrtFree(copyDevice);
        return result;
    }

    ret = DlAclApi::AclrtMemcpy2d(hostAddr, params.dpitch, copyDevice, params.width, params.width, params.height,
                                  ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != 0) {
        BM_LOG_ERROR("copy data on temp DEVICE to GVA failed: " << ret
                     << " spitch: " << params.spitch << " width: " << params.width << " height:" << params.height);
        DlAclApi::AclrtFree(copyDevice);
        return BM_DL_FUNCTION_FAILED;
    }

    DlAclApi::AclrtFree(copyDevice);
    return BM_OK;
}

int HostDataOpSDMA::CopyGD2LD2d(void *deviceAddr, const void *gvaAddr, hybm_copy_2d_params &params,
                                void *stream) noexcept
{
    void *st = stream;

    int ret = BM_OK;
    for (uint64_t i = 0; i < params.height; ++i) {
        void *dstAddr = reinterpret_cast<void *>((uint64_t) deviceAddr + i * params.dpitch);
        void *srcAddr = reinterpret_cast<void *>((uint64_t) gvaAddr + i * params.spitch);
        auto asyncRet = DlAclApi::AclrtMemcpyAsync(dstAddr, params.width, srcAddr, params.width,
                                                   ACL_MEMCPY_DEVICE_TO_DEVICE, st);
        if (asyncRet != 0) {
            BM_LOG_ERROR("copy2d memory on gva to device failed:: " << asyncRet
                         << " spitch: " << params.spitch << " dpitch: " << params.dpitch << " width: " << params.width
                         << " height:" << params.height << " stream:" << st);
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

int HostDataOpSDMA::DataCopy2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction,
                               const ExtOptions &options) noexcept
{
    if (!inited_) {
        BM_LOG_ERROR("host data operator sdma is not inited yet.");
        return BM_NOT_INITIALIZED;
    }

    int ret;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = CopyLD2GD2d(params.dest, params.src, params, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE:
            ret = CopyGD2LD2d(params.dest, params.src, params, options.stream);
            break;
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE:
            ret = CopyLH2GD2d(params.dest, params.src, params, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST:
            ret = CopyGD2LH2d(params.dest, params.src, params, options.stream);
            break;
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE:
            ret = CopyLD2GD2d(params.dest, params.src, params, options.stream);
            break;
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST:
            ret = CopyLD2GH2d(params.dest, params.src, params, options.stream);
            break;
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE:
            ret = CopyGH2LD2d(params.dest, params.src, params, options.stream);
            break;

        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int32_t HostDataOpSDMA::DataCopyAsync(hybm_copy_params &params,
                                      hybm_data_copy_direction direction, const ExtOptions &options) noexcept
{
    int ret;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LD_TO_GD);
            ret = CopyLD2GD(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_LD);
            ret = CopyGD2LD(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_LD, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LH_TO_GD);
            ret = CopyLH2GD(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_LH);
            ret = CopyGD2LH(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_GD);
            ret = CopyGD2GD(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_GH);
            ret = CopyGD2GH(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_GD);
            ret = CopyGH2GD(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_GD);
            ret = CopyGH2GH(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LH_TO_GH);
            ret = CopyLH2GH(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_LH);
            ret = CopyGH2LH(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_LH, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LD_TO_GH_ASYNC);
            ret = CopyLD2GHAsync(params.dest, params.src, params.dataSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_LD_TO_GH_ASYNC, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_LD_ASYNC);
            ret = CopyGH2LDAsync(params.dest, params.src, params.dataSize, options.stream);
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
    auto hybmStream = HybmStreamManager::GetThreadHybmStream(HybmGetInitedLogicDeviceId(), 0, 0);
    BM_ASSERT_RETURN(hybmStream != nullptr, BM_ERROR);
    if (hybmStream != nullptr) {
        return hybmStream->Synchronize();
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
        return ret;
    }
    // GD2GH
    ret = CopyG2G(destVA, tmpHbm, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyG2G ret: " << ret << " dest:"<< tmpHbm << " srcVa:" << srcVA
            << " length:" << length);
    }
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
        return ret;
    }
    // GD2GH
    ret = CopyG2G(destVA, tmpHbm, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyG2G ret: " << ret << " dest:"<< tmpHbm << " srcVa:" << srcVA
            << " length:" << length);
    }
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
        return ret;
    }
    // GD2LD
    ret = CopyGD2LD(destVA, tmpHbm, length, stream);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyLD2GD ret: " << ret << " dest:"<< destVA << " srcVa:" << tmpHbm
            << " length:" << length);
    }
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
        return ret;
    }
    // GD2LH
    ret = CopyGD2LH(destVA, tmpHbm, length, stream);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyLD2GD ret: " << ret << " dest:"<< destVA << " srcVa:" << tmpHbm
            << " length:" << length);
    }
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
    auto hybmStream = HybmStreamManager::GetThreadHybmStream(HybmGetInitedLogicDeviceId(), 0, 0);
    if (hybmStream == nullptr) {
        BM_LOG_ERROR("Failed to get thread local hybmStream");
        return;
    }
    task.type = STREAM_TASK_TYPE_SDMA;
    rtStarsMemcpyAsyncSqe_t *const sqe = &(task.sqe.memcpyAsyncSqe);
    sqe->header.type = RT_STARS_SQE_TYPE_SDMA;
    sqe->header.ie = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.pre_p = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.wr_cqe = hybmStream->GetWqeFlag();
    sqe->header.rt_stream_id = hybmStream->GetId();
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
    auto hybmStream = HybmStreamManager::GetThreadHybmStream(HybmGetInitedLogicDeviceId(), 0, 0);
    BM_ASSERT_RETURN(hybmStream != nullptr, BM_ERROR);
    sqe->length = count;
    sqe->src_addr_low =
            static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(srcVA)) & 0x00000000FFFFFFFFU);
    sqe->src_addr_high = static_cast<uint32_t>(
            (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(srcVA)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);
    sqe->dst_addr_low =
            static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(destVA)) & 0x00000000FFFFFFFFU);
    sqe->dst_addr_high = static_cast<uint32_t>(
            (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(destVA)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);

    auto ret = hybmStream->SubmitTasks(task);
    BM_ASSERT_RETURN(ret == 0, BM_ERROR);

    ret = hybmStream->Synchronize();
    BM_ASSERT_RETURN(ret == 0, BM_ERROR);
    return BM_OK;
}

int HostDataOpSDMA::CopyG2G2d(void* destVA, const void* srcVA, hybm_copy_2d_params &params) noexcept
{
    StreamTask task{};
    InitG2GStreamTask(task);
    rtStarsMemcpyAsyncSqe_t *const sqe = &(task.sqe.memcpyAsyncSqe);
    auto hybmStream = HybmStreamManager::GetThreadHybmStream(HybmGetInitedLogicDeviceId(), 0, 0);
    BM_ASSERT_RETURN(hybmStream != nullptr, BM_ERROR);
    for (size_t i = 0; i < params.height; ++i) {
        void *dst = reinterpret_cast<void *>((uint64_t) destVA + i * params.dpitch);
        void *src = reinterpret_cast<void *>((uint64_t) srcVA + i * params.spitch);
        sqe->length = params.width;
        sqe->src_addr_low =
                static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(src)) & 0x00000000FFFFFFFFU);
        sqe->src_addr_high = static_cast<uint32_t>(
                (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(src)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);
        sqe->dst_addr_low =
                static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(dst)) & 0x00000000FFFFFFFFU);
        sqe->dst_addr_high = static_cast<uint32_t>(
                (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(dst)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);
        auto ret = hybmStream->SubmitTasks(task);
        BM_ASSERT_LOG_AND_RETURN(ret == 0, "Failed to submit g2g task ret:" << ret, BM_ERROR);
    }

    auto ret = hybmStream->Synchronize();
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "Failed to wait g2g task ret:" << ret, BM_ERROR);
    return BM_OK;
}

int HostDataOpSDMA::CopyG2GAsync(void *destVA, const void *srcVA, size_t count) noexcept
{
    BM_LOG_DEBUG("CopyG2GAsync src:" << srcVA << " destVA:" << destVA << " length:" << count);
    StreamTask task{};
    InitG2GStreamTask(task);
    rtStarsMemcpyAsyncSqe_t *const sqe = &(task.sqe.memcpyAsyncSqe);
    auto hybmStream = HybmStreamManager::GetThreadHybmStream(HybmGetInitedLogicDeviceId(), 0, 0);
    BM_ASSERT_RETURN(hybmStream != nullptr, BM_ERROR);
    sqe->length = count;
    sqe->src_addr_low =
            static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(srcVA)) & 0x00000000FFFFFFFFU);
    sqe->src_addr_high = static_cast<uint32_t>(
            (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(srcVA)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);
    sqe->dst_addr_low =
            static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(destVA)) & 0x00000000FFFFFFFFU);
    sqe->dst_addr_high = static_cast<uint32_t>(
            (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(destVA)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);

    TP_TRACE_BEGIN(TP_HYBM_SDMA_SUBMIT_G2G_TASK);
    auto ret = hybmStream->SubmitTasks(task);
    TP_TRACE_END(TP_HYBM_SDMA_SUBMIT_G2G_TASK, ret);
    BM_ASSERT_RETURN(ret == 0, BM_ERROR);
    return BM_OK;
}

int HostDataOpSDMA::BatchCopyG2G(void **destVAs, void **srcVAs,
                                 const uint64_t *counts, uint32_t batchSize) noexcept
{
    int32_t ret = 0;
    int32_t asyncRet = 0;
    uint64_t src = 0U;
    uint64_t dest = 0U;
    uint64_t len = 0U;

    auto asyncFunc = [&]() {
        asyncRet = CopyG2GAsync(reinterpret_cast<void *>(dest), reinterpret_cast<void *>(src), len);
        if (asyncRet != 0) {
            BM_LOG_ERROR("BatchCopyG2G failed:" << asyncRet << " src:" << src << " dest:" << dest << " length:" << len);
            ret = asyncRet;
        }
        return;
    };

    for (auto i = 0U; i < batchSize; i++) {
        uint64_t srcAddr = reinterpret_cast<uint64_t>(srcVAs[i]);
        uint64_t destAddr = reinterpret_cast<uint64_t>(destVAs[i]);
        uint64_t count = counts[i];

        uint64_t retAddr = hybm_gvm_mem_has_registered(srcAddr, count);
        srcAddr = (retAddr > 0 ? retAddr : srcAddr);
        retAddr = hybm_gvm_mem_has_registered(destAddr, count);
        destAddr = (retAddr > 0 ? retAddr : destAddr);

        if (len > 0 && src + len == srcAddr && dest + len == destAddr) {
            len += count;
            continue;
        }

        if (len > 0) {
            asyncFunc();
        }
        src = srcAddr;
        dest = destAddr;
        len = count;
    }
    asyncFunc();

    TP_TRACE_BEGIN(TP_HYBM_SDMA_WAIT);
    asyncRet = Wait(0);
    TP_TRACE_END(TP_HYBM_SDMA_WAIT, ret);
    if (asyncRet != 0) {
        BM_LOG_ERROR("BatchCopyG2G wait copy failed:" << asyncRet);
        ret = asyncRet;
    }
    return ret;
}

int HostDataOpSDMA::CopyGH2LD2d(void *deviceAddr, const void *gvaAddr, hybm_copy_2d_params &params,
                                void *stream) noexcept
{
    if (params.spitch != params.width) {
        BM_LOG_ERROR("Invalid param spitch: " << params.spitch << " width: " << params.width);
        return BM_INVALID_PARAM;
    }
    if (hybm_gvm_mem_has_registered((uint64_t)deviceAddr, params.width)) {
        return CopyG2G2d(deviceAddr, gvaAddr, params);
    }
    uint64_t length = params.height * params.width;
    auto tmpSdmaMemory = sdmaSwapMemoryAllocator_->Allocate(length);
    void *tmpHbm = tmpSdmaMemory.Address();
    if (tmpHbm == nullptr) {
        BM_LOG_ERROR("Failed to malloc swap length: " << length << " width: " << params.width
            << " height: " << params.height);
        return BM_MALLOC_FAILED;
    }
    auto ret = CopyG2G(tmpHbm, gvaAddr, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyG2G ret: " << ret << " dest:"<< tmpHbm << " srcVa:" << gvaAddr
                                               << " length:" << length);
        return ret;
    }

    ret = CopyGD2LD2d(deviceAddr, tmpHbm, params, stream);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyGD2LD2d ret: " << ret << " dest:"<< deviceAddr << " srcVa:" << tmpHbm
                     << " spitch: " << params.spitch << " dpitch: " << params.dpitch << " width: " << params.width
                     << " height:" << params.height << " stream:" << stream);
    }
    return ret;
}

int HostDataOpSDMA::CopyLD2GH2d(void *gvaAddr, const void *deviceAddr, hybm_copy_2d_params &params,
                                void *stream) noexcept
{
    if (params.dpitch != params.width) {
        BM_LOG_ERROR("Invalid param dpitch: " << params.dpitch << " width: " << params.width);
        return BM_INVALID_PARAM;
    }
    if (hybm_gvm_mem_has_registered((uint64_t)deviceAddr, params.width)) {
        return CopyG2G2d(gvaAddr, deviceAddr, params);
    }
    uint64_t length = params.height * params.width;
    auto tmpSdmaMemory = sdmaSwapMemoryAllocator_->Allocate(length);
    void *tmpHbm = tmpSdmaMemory.Address();
    if (tmpHbm == nullptr) {
        BM_LOG_ERROR("Failed to malloc swap length: " << length << " width: " << params.width
            << " height: " << params.height);
        return BM_MALLOC_FAILED;
    }

    auto ret = CopyLD2GD2d(tmpHbm, deviceAddr, params, stream);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyLD2GD2d ret: " << ret << " dest:"<< deviceAddr << " srcVa:" << tmpHbm
                     << " spitch: " << params.spitch << " dpitch: " << params.dpitch << " width: " << params.width
                     << " height:" << params.height << " stream:" << stream);
        return ret;
    }
    ret = CopyG2G(gvaAddr, tmpHbm, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to CopyG2G ret: " << ret << " dest:"<< gvaAddr << " srcVa:" << tmpHbm
                                               << " length:" << length);
    }
    return ret;
}

int32_t HostDataOpSDMA::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                      const ExtOptions &options) noexcept
{
    auto ret = 0;
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE: {
            ret = BatchCopyLH2GD(params.destinations, params.sources, params.dataSizes,
                                 params.batchSize, options.stream);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST: {
            ret = BatchCopyGD2LH(params.destinations, params.sources, params.dataSizes,
                                 params.batchSize, options.stream);
            break;
        }
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
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_LH_TO_GH);
            ret = BatchCopyLH2GH(params.destinations, params.sources, params.dataSizes,
                                 params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_LH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GH_TO_LH);
            ret = BatchCopyGH2LH(params.destinations, params.sources, params.dataSizes,
                                 params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_GH_TO_LH, ret);
            break;
        }
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int HostDataOpSDMA::BatchCopyLD2GH(void **gvaAddrs, void **deviceAddrs, const uint64_t *counts,
                                   uint32_t batchSize, void *stream) noexcept
{
    void *st = stream;
    if (hybm_gvm_mem_has_registered((uint64_t)deviceAddrs[0], counts[0])) {
        return BatchCopyG2G(gvaAddrs, deviceAddrs, counts, batchSize);
    }
    auto ret = 0;
    uint64_t totalSize = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        totalSize += counts[i];
        BM_ASSERT_RETURN(totalSize <= HBM_SWAP_SPACE_SIZE, BM_INVALID_PARAM);
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
            return BM_DL_FUNCTION_FAILED;
        }
    }

    ret  = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << st);
        return ret;
    }
    ret = CopyG2G(gvaAddrs[0], tmpHbm, totalSize);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to g2g ret:" << ret << " src:" <<  gvaAddrs[0]
                                          << " tmpHbm:" << tmpHbm << " length: " << totalSize);
    }
    return ret;
}

int HostDataOpSDMA::BatchCopyGH2LD(void **deviceAddrs, void **gvaAddrs, const uint64_t *counts,
                                   uint32_t batchSize, void *stream) noexcept
{
    void *st = stream;
    if (hybm_gvm_mem_has_registered((uint64_t)deviceAddrs[0], counts[0])) {
        return BatchCopyG2G(deviceAddrs, gvaAddrs, counts, batchSize);
    }
    auto ret = 0;
    uint64_t totalSize = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        totalSize += counts[i];
        BM_ASSERT_RETURN(totalSize <= HBM_SWAP_SPACE_SIZE, BM_INVALID_PARAM);
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
            return BM_DL_FUNCTION_FAILED;
        }
    }

    ret  = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << st);
    }
    return ret;
}

int HostDataOpSDMA::BatchCopyLD2GD(void **gvaAddrs, void **deviceAddrs, const uint64_t *counts,
                                   uint32_t batchSize, void *stream) noexcept
{
    BM_LOG_DEBUG("[DEBUG]copy memory on local device to GVA count:" << counts[0] << " destAddr:" << deviceAddrs[0]);
    if (hybm_gvm_mem_has_registered((uint64_t)deviceAddrs[0], counts[0])) {
        return BatchCopyG2G(gvaAddrs, deviceAddrs, counts, batchSize);
    }
    auto ret = 0;

    for (auto i = 0U; i < batchSize; i++) {
        ret = DlAclApi::AclrtMemcpyAsync(gvaAddrs[i], counts[i], deviceAddrs[i], counts[i],
                                         ACL_MEMCPY_DEVICE_TO_DEVICE, stream);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on GVA to local device failed: " << ret << " stream:" << stream);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    ret = DlAclApi::AclrtSynchronizeStream(stream);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << stream);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

int HostDataOpSDMA::BatchCopyGD2LD(void **deviceAddrs, void **gvaAddrs, const uint64_t *counts,
                                   uint32_t batchSize, void *stream) noexcept
{
    if (hybm_gvm_mem_has_registered((uint64_t)deviceAddrs[0], counts[0])) {
        return BatchCopyG2G(deviceAddrs, gvaAddrs, counts, batchSize);
    }
    auto ret = 0;

    for (auto i = 0U; i < batchSize; i++) {
        ret = DlAclApi::AclrtMemcpyAsync(deviceAddrs[i], counts[i], gvaAddrs[i], counts[i],
                                         ACL_MEMCPY_DEVICE_TO_DEVICE, stream);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on GVA to local device failed: " << ret << " stream:" << stream);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    ret = DlAclApi::AclrtSynchronizeStream(stream);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << stream);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

int HostDataOpSDMA::BatchCopyLH2GH(void **gvaAddrs, void **hostAddrs, const uint64_t *counts,
                                   uint32_t batchSize, void *stream) noexcept
{
    auto ret = 0;

    for (auto i = 0U; i < batchSize; i++) {
        ret = CopyLH2GH(gvaAddrs[i], hostAddrs[i], counts[i], stream);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local host to GVA failed: " << ret);
            return ret;
        }
    }
    return BM_OK;
}

int HostDataOpSDMA::BatchCopyGH2LH(void **hostAddrs, void **gvaAddrs, const uint64_t *counts,
                                   uint32_t batchSize, void *stream) noexcept
{
    auto ret = 0;

    for (auto i = 0U; i < batchSize; i++) {
        ret = CopyGH2LH(hostAddrs[i], gvaAddrs[i],  counts[i], stream);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on GVA to local host failed: " << ret);
            return ret;
        }
    }
    return BM_OK;
}

int HostDataOpSDMA::BatchCopyLH2GD(void **gvaAddrs, void **hostAddrs, const uint64_t *counts, uint32_t batchSize,
                                   void *stream) noexcept
{
    auto ret = 0;

    for (auto i = 0U; i < batchSize; i++) {
        ret = CopyLH2GD(gvaAddrs[i], hostAddrs[i], counts[i], stream);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local host to GVA failed: " << ret);
            return ret;
        }
    }
    return BM_OK;
}

int HostDataOpSDMA::BatchCopyGD2LH(void **hostAddrs, void **gvaAddrs, const uint64_t *counts, uint32_t batchSize,
                                   void *stream) noexcept
{
    auto ret = 0;

    for (auto i = 0U; i < batchSize; i++) {
        ret = CopyGD2LH(hostAddrs[i], gvaAddrs[i], counts[i], stream);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on GVA to local host failed: " << ret);
            return ret;
        }
    }
    return BM_OK;
}
}
}
