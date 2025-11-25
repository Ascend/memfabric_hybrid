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
#include "hybm_data_op_sdma.h"

#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "hybm_ptracer.h"
#include "hybm_gvm_user.h"
#include "hybm_data_op.h"
#include "hybm_gva.h"
#include "hybm_stream_manager.h"

namespace ock {
namespace mf {
constexpr uint64_t HBM_SWAP_SPACE_SIZE = 128 * 1024 * 1024;

HostDataOpSDMA::HostDataOpSDMA() noexcept = default;

HostDataOpSDMA::~HostDataOpSDMA()
{
    HostDataOpSDMA::UnInitialize();
}

#ifdef USE_VMM
int32_t HostDataOpSDMA::Initialize() noexcept
{
    if (inited_) {
        return BM_OK;
    }

    inited_ = true;
    return BM_OK;
}

void HostDataOpSDMA::UnInitialize() noexcept
{
    if (!inited_) {
        return;
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
            ret = CopyG2G(params.dest, params.src, params.dataSize);
            TP_TRACE_END(TP_HYBM_SDMA_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_LD);
            ret = CopyG2G(params.dest, params.src, params.dataSize);
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
            ret = CopyG2G(params.dest, params.src, params.dataSize);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_GH);
            ret = CopyG2G(params.dest, params.src, params.dataSize);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_GD);
            ret = CopyG2G(params.dest, params.src, params.dataSize);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_GD);
            ret = CopyG2G(params.dest, params.src, params.dataSize);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LD_TO_GH);
            ret = CopyG2G(params.dest, params.src, params.dataSize);
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
            ret = CopyG2G(params.dest, params.src, params.dataSize);
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

    auto result = CopyG2G(gvaAddr, copyDevice, count);
    if (result != BM_OK) {
        DlAclApi::AclrtFree(copyDevice);
        return result;
    }

    DlAclApi::AclrtFree(copyDevice);
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

    auto result = CopyG2G(copyDevice, gvaAddr, count);
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

int32_t HostDataOpSDMA::DataCopyAsync(hybm_copy_params &params,
                                      hybm_data_copy_direction direction, const ExtOptions &options) noexcept
{
    int ret;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_LD_TO_GD);
            ret = CopyG2GAsync(params.dest, params.src, params.dataSize);
            TP_TRACE_END(TP_HYBM_SDMA_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_LD);
            ret = CopyG2GAsync(params.dest, params.src, params.dataSize);
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
            ret = CopyG2GAsync(params.dest, params.src, params.dataSize);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GD_TO_GH);
            ret = CopyG2GAsync(params.dest, params.src, params.dataSize);
            TP_TRACE_END(TP_HYBM_SDMA_GD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_GD);
            ret = CopyG2GAsync(params.dest, params.src, params.dataSize);
            TP_TRACE_END(TP_HYBM_SDMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_GD);
            ret = CopyG2GAsync(params.dest, params.src, params.dataSize);
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
            ret = CopyG2GAsync(params.dest, params.src, params.dataSize);
            TP_TRACE_END(TP_HYBM_SDMA_LD_TO_GH_ASYNC, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_GH_TO_LD_ASYNC);
            ret = CopyG2GAsync(params.dest, params.src, params.dataSize);
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

int HostDataOpSDMA::CopyLH2GH(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept
{
    // local host到dram池的拷贝
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, length, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtMemcpy(copyDevice, length, srcVA, length, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR("copy host data to temp copy memory on local device failed: " << ret);
        DlAclApi::AclrtFree(copyDevice);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyG2G(destVA, copyDevice, length);
    if (result != BM_OK) {
        DlAclApi::AclrtFree(copyDevice);
        return result;
    }

    DlAclApi::AclrtFree(copyDevice);
    return ret;
}

int HostDataOpSDMA::CopyGH2LH(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept
{
    // dram池的拷贝到local host
    void *copyDevice;
    auto ret = DlAclApi::AclrtMalloc(&copyDevice, length, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    auto result = CopyG2G(copyDevice, srcVA, length);
    if (result != BM_OK) {
        DlAclApi::AclrtFree(copyDevice);
        return result;
    }

    ret = DlAclApi::AclrtMemcpy(destVA, length, copyDevice, length, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != 0) {
        BM_LOG_ERROR("copy host data to temp copy memory on local device failed: " << ret);
        DlAclApi::AclrtFree(copyDevice);
        return BM_DL_FUNCTION_FAILED;
    }

    DlAclApi::AclrtFree(copyDevice);
    return ret;
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
            ret = BatchCopyG2G(params.destinations, params.sources, params.dataSizes, params.batchSize);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_LD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GH_TO_LD);
            ret = BatchCopyG2G(params.destinations, params.sources, params.dataSizes, params.batchSize);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_GH_TO_LD, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_LD_TO_GD);
            ret = BatchCopyG2G(params.destinations, params.sources, params.dataSizes, params.batchSize);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GD_TO_LD);
            ret = BatchCopyG2G(params.destinations, params.sources, params.dataSizes, params.batchSize);
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
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_G_TO_G);
            ret = BatchCopyG2G(params.destinations, params.sources, params.dataSizes, params.batchSize);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_G_TO_G, ret);
    }
    return ret;
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
#else
int32_t HostDataOpSDMA::Initialize() noexcept
{
    if (inited_) {
        return BM_OK;
    }

    auto ret = DlAclApi::AclrtMalloc(&sdmaSwapMemAddr_, HBM_SWAP_SPACE_SIZE, 0);
    if (ret != 0 || !sdmaSwapMemAddr_) {
        BM_LOG_ERROR("allocate temp copy memory on local device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    sdmaSwapMemoryAllocator_ = std::make_shared<RbtreeRangePool>((uint8_t *) sdmaSwapMemAddr_, HBM_SWAP_SPACE_SIZE);

    if (HybmGvmHasInited()) {
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
    return ret;
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
    auto ret = CopyG2GAsync(destVA, srcVA, count);
    BM_ASSERT_RETURN(ret == 0, BM_ERROR);
    auto hybmStream = HybmStreamManager::GetThreadHybmStream(HybmGetInitedLogicDeviceId(), 0, 0);
    BM_ASSERT_RETURN(hybmStream != nullptr, BM_ERROR);
    ret = hybmStream->Synchronize();
    BM_ASSERT_RETURN(ret == 0, BM_ERROR);
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

    auto asyncFunc = [this, &dest, &src, &len, &asyncRet, &ret]() {
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

int32_t HostDataOpSDMA::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                      const ExtOptions &options) noexcept
{
    auto ret = 0;
    switch (direction) {
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST: { // 4
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GH_TO_GH);
            ret =
                BatchCopyGH2GH(params.destinations, params.sources, params.dataSizes, params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_GH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE: { // 5
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GH_TO_GD);
            ret =
                BatchCopyGH2GD(params.destinations, params.sources, params.dataSizes, params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_GH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST: { // 8
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GD_TO_GH);
            ret =
                BatchCopyGD2GH(params.destinations, params.sources, params.dataSizes, params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_GD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE: { // 9
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GD_TO_GD);
            ret =
                BatchCopyGD2GD(params.destinations, params.sources, params.dataSizes, params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_GD_TO_GD, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE: { // 1
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_LH_TO_GD);
            ret =
                BatchCopyLH2GD(params.destinations, params.sources, params.dataSizes, params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_LH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST: { // 10
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GD_TO_LH);
            ret =
                BatchCopyGD2LH(params.destinations, params.sources, params.dataSizes, params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_GD_TO_LH, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: { // 2
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_LD_TO_GH);
            ret =
                BatchCopyLD2GH(params.destinations, params.sources, params.dataSizes, params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_LD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: { // 7
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GH_TO_LD);
            ret =
                BatchCopyGH2LD(params.destinations, params.sources, params.dataSizes, params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_GH_TO_LD, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE: { // 3
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_LD_TO_GD);
            ret =
                BatchCopyLD2GD(params.destinations, params.sources, params.dataSizes, params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: { // 11
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GD_TO_LD);
            ret =
                BatchCopyGD2LD(params.destinations, params.sources, params.dataSizes, params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_GD_TO_LD, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: { // 0
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_LH_TO_GH);
            ret =
                BatchCopyLH2GH(params.destinations, params.sources, params.dataSizes, params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_LH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST: { // 6
            TP_TRACE_BEGIN(TP_HYBM_SDMA_BATCH_GH_TO_LH);
            ret =
                BatchCopyGH2LH(params.destinations, params.sources, params.dataSizes, params.batchSize, options.stream);
            TP_TRACE_END(TP_HYBM_SDMA_BATCH_GH_TO_LH, ret);
            break;
        }
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

void HostDataOpSDMA::ClassifyDataAddr(void **globalAddrs, void **localAddrs, const uint64_t *counts, uint32_t batchSize,
                                      CopyDescriptor &registered, CopyDescriptor &notRegistered) noexcept
{
    for (size_t i = 0; i < batchSize; ++i) {
        if (hybm_gvm_mem_has_registered((uint64_t)localAddrs[i], counts[i])) {
            registered.localAddrs.push_back(localAddrs[i]);
            registered.globalAddrs.push_back(globalAddrs[i]);
            registered.counts.push_back(counts[i]);
        } else {
            notRegistered.localAddrs.push_back(localAddrs[i]);
            notRegistered.globalAddrs.push_back(globalAddrs[i]);
            notRegistered.counts.push_back(counts[i]);
        }
    }
}

int HostDataOpSDMA::BatchWriteNotRegisterData(CopyDescriptor &notRegistered, void *stream, uint32_t direction) noexcept
{
    void *st = stream;
    auto ret = 0;

    // 分批处理：每批最大不超过 HBM_SWAP_SPACE_SIZE
    size_t notBatchSize = notRegistered.counts.size();
    uint64_t batchOffset = 0; // 当前处理的 notRegistered 索引
    while (batchOffset < notBatchSize) {
        // 计算当前批次能拷贝的最大数据量
        uint64_t currentBatchSize = 0;
        size_t batchEnd = batchOffset;
        while (batchEnd < notBatchSize && currentBatchSize + notRegistered.counts[batchEnd] <= HBM_SWAP_SPACE_SIZE) {
            currentBatchSize += notRegistered.counts[batchEnd];
            ++batchEnd;
        }

        // 如果连一个都放不下，说明单个 count 太大
        if (currentBatchSize == 0) {
            BM_LOG_ERROR("Single count exceeds HBM_SWAP_SPACE_SIZE: " << notRegistered.counts[batchOffset] << " > "
                                                                      << HBM_SWAP_SPACE_SIZE);
            return BM_INVALID_PARAM;
        }

        // 分配当前批次的临时 HBM 内存
        auto tmpSdmaMemory = sdmaSwapMemoryAllocator_->Allocate(currentBatchSize);
        void *tmpHbm = tmpSdmaMemory.Address();
        if (tmpHbm == nullptr) {
            BM_LOG_ERROR("Failed to malloc swap length: " << currentBatchSize);
            return BM_MALLOC_FAILED;
        }

        // 拷贝当前批次数据到tmp HBM
        uint64_t offsetInBatch = 0;
        for (size_t i = batchOffset; i < batchEnd; ++i) {
            auto localAddr = notRegistered.localAddrs[i];
            auto tmpDevAddr = (void *)((uint64_t)tmpHbm + offsetInBatch);
            auto count = notRegistered.counts[i];
            if (direction == ACL_MEMCPY_DEVICE_TO_DEVICE || direction == ACL_MEMCPY_HOST_TO_DEVICE) {
                ret = DlAclApi::AclrtMemcpyAsync(tmpDevAddr, count, localAddr, count, direction, st);
            } else {
                ret = BM_INVALID_PARAM;
                BM_LOG_ERROR("unexcepted direct:" << direction);
            }
            if (ret != 0) {
                BM_LOG_ERROR("copy memory on local device to GVA failed: " << ret << " stream:" << st << " at index "
                                                                           << i);
                return BM_DL_FUNCTION_FAILED;
            }
            offsetInBatch += count;
        }
        ret = DlAclApi::AclrtSynchronizeStream(st);
        if (ret != 0) {
            BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << st);
            return ret;
        }

        // tmp copy 到 gva
        uint64_t buffCopyOffset = 0;
        for (size_t i = batchOffset; i < batchEnd; ++i) {
            auto gva = notRegistered.globalAddrs[i];
            auto tmpHbmAddr = (void *)(reinterpret_cast<uint64_t>(tmpHbm) + buffCopyOffset);
            auto count = notRegistered.counts[i];
            ret = CopyG2GAsync(gva, tmpHbmAddr, count);
            if (ret != 0) {
                BM_LOG_ERROR("Failed to g2g ret:" << ret << " src:" << gva << " tmpHbmOff:" << buffCopyOffset
                                                  << " length: " << count);
                return ret;
            }
            buffCopyOffset += count;
        }

        auto hybmStream = HybmStreamManager::GetThreadHybmStream(HybmGetInitedLogicDeviceId(), 0, 0);
        BM_ASSERT_RETURN(hybmStream != nullptr, BM_ERROR);
        ret = hybmStream->Synchronize();
        BM_ASSERT_RETURN(ret == 0, BM_ERROR);

        // 下一次迭代
        batchOffset = batchEnd;
    }
    return ret;
}

int HostDataOpSDMA::BatchReadNotRegisterData(CopyDescriptor &notRegistered, void *stream, uint32_t direction) noexcept
{
    void *st = stream;
    auto ret = 0;

    // 分批处理：每批最大不超过 HBM_SWAP_SPACE_SIZE
    size_t notBatchSize = notRegistered.counts.size();
    uint64_t batchOffset = 0; // 当前处理的 notRegistered 索引
    while (batchOffset < notBatchSize) {
        // 计算当前批次能拷贝的最大数据量
        uint64_t currentBatchSize = 0;
        size_t batchEnd = batchOffset;
        while (batchEnd < notBatchSize && currentBatchSize + notRegistered.counts[batchEnd] <= HBM_SWAP_SPACE_SIZE) {
            currentBatchSize += notRegistered.counts[batchEnd];
            ++batchEnd;
        }

        // 如果连一个都放不下，说明单个 count 太大
        if (currentBatchSize == 0) {
            BM_LOG_ERROR("Single count exceeds HBM_SWAP_SPACE_SIZE: " << notRegistered.counts[batchOffset] << " > "
                                                                      << HBM_SWAP_SPACE_SIZE);
            return BM_INVALID_PARAM;
        }

        // 分配当前批次的临时 HBM 内存
        auto tmpSdmaMemory = sdmaSwapMemoryAllocator_->Allocate(currentBatchSize);
        void *tmpHbm = tmpSdmaMemory.Address();
        if (tmpHbm == nullptr) {
            BM_LOG_ERROR("Failed to malloc swap length: " << currentBatchSize);
            return BM_MALLOC_FAILED;
        }

        // 先copy到tmp内存
        uint64_t buffCopyOffset = 0;
        for (size_t i = batchOffset; i < batchEnd; ++i) {
            auto gva = notRegistered.globalAddrs[i];
            auto tmpHbmAddr = (void *)(reinterpret_cast<uint64_t>(tmpHbm) + buffCopyOffset);
            auto count = notRegistered.counts[i];
            ret = CopyG2GAsync(tmpHbmAddr, gva, count);
            if (ret != 0) {
                BM_LOG_ERROR("Failed to g2g ret:" << ret << " src:" << gva << " tmpHbmOff:" << buffCopyOffset
                                                  << " length: " << count);
                return ret;
            }
            buffCopyOffset += count;
        }

        auto hybmStream = HybmStreamManager::GetThreadHybmStream(HybmGetInitedLogicDeviceId(), 0, 0);
        BM_ASSERT_RETURN(hybmStream != nullptr, BM_ERROR);
        ret = hybmStream->Synchronize();
        BM_ASSERT_RETURN(ret == 0, BM_ERROR);

        // 从tmp copy到目标位置
        uint64_t offsetInBatch = 0;
        for (size_t i = batchOffset; i < batchEnd; ++i) {
            auto localAddr = notRegistered.localAddrs[i];
            auto tmpDevAddr = (void *)((uint64_t)tmpHbm + offsetInBatch);
            auto count = notRegistered.counts[i];
            if (direction == ACL_MEMCPY_DEVICE_TO_DEVICE || direction == ACL_MEMCPY_DEVICE_TO_HOST) {
                ret = DlAclApi::AclrtMemcpyAsync(localAddr, count, tmpDevAddr, count, direction, st);
            } else {
                ret = BM_INVALID_PARAM;
                BM_LOG_ERROR("unexcepted direct:" << direction);
            }
            if (ret != 0) {
                BM_LOG_ERROR("copy memory on local device to GVA failed: " << ret << " stream:" << st << " at index "
                                                                           << i);
                return BM_DL_FUNCTION_FAILED;
            }
            offsetInBatch += count;
        }
        ret = DlAclApi::AclrtSynchronizeStream(st);
        if (ret != 0) {
            BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << st);
            return ret;
        }

        // 下一次迭代
        batchOffset = batchEnd;
    }
    return ret;
}

int HostDataOpSDMA::BatchCopyLD2GH(void **destinations, void **sources, const uint64_t *counts, uint32_t batchSize,
                                   void *stream) noexcept
{
    auto ret = 0;
    CopyDescriptor registered{};
    CopyDescriptor notRegistered{};
    ClassifyDataAddr(destinations, sources, counts, batchSize, registered, notRegistered);

    if (!registered.localAddrs.empty()) {
        ret = BatchCopyG2G(registered.globalAddrs.data(), registered.localAddrs.data(), registered.counts.data(),
                           registered.counts.size());
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "batch write g2g failed", ret);
    }

    return BatchWriteNotRegisterData(notRegistered, stream, ACL_MEMCPY_DEVICE_TO_DEVICE);
}

int HostDataOpSDMA::BatchCopyGH2LD(void **destinations, void **sources, const uint64_t *counts,
                                   uint32_t batchSize, void *stream) noexcept
{
    auto ret = 0;
    CopyDescriptor registered{};
    CopyDescriptor notRegistered{};
    ClassifyDataAddr(sources, destinations, counts, batchSize, registered, notRegistered);

    if (!registered.localAddrs.empty()) {
        ret = BatchCopyG2G(registered.localAddrs.data(), registered.globalAddrs.data(), registered.counts.data(),
                           registered.counts.size());
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "batch write g2g failed", ret);
    }
    return BatchReadNotRegisterData(notRegistered, stream, ACL_MEMCPY_DEVICE_TO_DEVICE);
}

int HostDataOpSDMA::BatchCopyLD2GD(void **destinations, void **sources, const uint64_t *counts, uint32_t batchSize,
                                   void *stream) noexcept
{
    auto ret = 0;
    CopyDescriptor registered{};
    CopyDescriptor notRegistered{};
    ClassifyDataAddr(destinations, sources, counts, batchSize, registered, notRegistered);
    if (!registered.localAddrs.empty()) {
        ret = BatchCopyG2G(registered.globalAddrs.data(), registered.localAddrs.data(), registered.counts.data(),
                           registered.counts.size());
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "batch write g2g failed", ret);
    }

    auto notRegisteredBatchSize = notRegistered.counts.size();
    for (auto i = 0U; i < notRegisteredBatchSize; i++) {
        auto gva = notRegistered.globalAddrs[i];
        auto local = notRegistered.localAddrs[i];
        auto count = notRegistered.counts[i];
        ret = DlAclApi::AclrtMemcpyAsync(gva, count, local, count, ACL_MEMCPY_DEVICE_TO_DEVICE, stream);
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

int HostDataOpSDMA::BatchCopyGD2LD(void **destinations, void **sources, const uint64_t *counts, uint32_t batchSize,
                                   void *stream) noexcept
{
    auto ret = 0;
    CopyDescriptor registered{};
    CopyDescriptor notRegistered{};
    ClassifyDataAddr(sources, destinations, counts, batchSize, registered, notRegistered);
    if (!registered.localAddrs.empty()) {
        ret = BatchCopyG2G(registered.localAddrs.data(), registered.globalAddrs.data(), registered.counts.data(),
                           registered.counts.size());
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "batch write g2g failed", ret);
    }

    auto notRegisteredBatchSize = notRegistered.counts.size();
    for (auto i = 0U; i < notRegisteredBatchSize; i++) {
        auto gva = notRegistered.globalAddrs[i];
        auto local = notRegistered.localAddrs[i];
        auto count = notRegistered.counts[i];
        ret = DlAclApi::AclrtMemcpyAsync(local, count, gva, count, ACL_MEMCPY_DEVICE_TO_DEVICE, stream);
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

int HostDataOpSDMA::BatchCopyLH2GH(void **destinations, void **sources, const uint64_t *counts, uint32_t batchSize,
                                   void *stream) noexcept
{
    auto ret = 0;

    CopyDescriptor registered{};
    CopyDescriptor notRegistered{};
    ClassifyDataAddr(destinations, sources, counts, batchSize, registered, notRegistered);
    if (!registered.localAddrs.empty()) {
        ret = BatchCopyG2G(registered.globalAddrs.data(), registered.localAddrs.data(), registered.counts.data(),
                           registered.counts.size());
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "batch write g2g failed", ret);
    }

    return BatchWriteNotRegisterData(notRegistered, stream, ACL_MEMCPY_HOST_TO_DEVICE);
}

int HostDataOpSDMA::BatchCopyGH2LH(void **destinations, void **sources, const uint64_t *counts, uint32_t batchSize,
                                   void *stream) noexcept
{
    auto ret = 0;

    CopyDescriptor registered{};
    CopyDescriptor notRegistered{};
    ClassifyDataAddr(sources, destinations, counts, batchSize, registered, notRegistered);
    if (!registered.localAddrs.empty()) {
        ret = BatchCopyG2G(registered.localAddrs.data(), registered.globalAddrs.data(), registered.counts.data(),
                           registered.counts.size());
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "batch write g2g failed", ret);
    }

    return BatchReadNotRegisterData(notRegistered, stream, ACL_MEMCPY_DEVICE_TO_HOST);
}

int HostDataOpSDMA::BatchCopyLH2GD(void **destinations, void **sources, const uint64_t *counts, uint32_t batchSize,
                                   void *stream) noexcept
{
    auto ret = 0;

    CopyDescriptor registered{};
    CopyDescriptor notRegistered{};
    ClassifyDataAddr(destinations, sources, counts, batchSize, registered, notRegistered);
    if (!registered.localAddrs.empty()) {
        ret = BatchCopyG2G(registered.globalAddrs.data(), registered.localAddrs.data(), registered.counts.data(),
                           registered.counts.size());
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "batch write g2g failed", ret);
    }

    return BatchWriteNotRegisterData(notRegistered, stream, ACL_MEMCPY_HOST_TO_DEVICE);
}

int HostDataOpSDMA::BatchCopyGD2LH(void **destinations, void **sources, const uint64_t *counts, uint32_t batchSize,
                                   void *stream) noexcept
{
    auto ret = 0;

    CopyDescriptor registered{};
    CopyDescriptor notRegistered{};
    ClassifyDataAddr(sources, destinations, counts, batchSize, registered, notRegistered);
    if (!registered.localAddrs.empty()) {
        ret = BatchCopyG2G(registered.localAddrs.data(), registered.globalAddrs.data(), registered.counts.data(),
                           registered.counts.size());
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "batch write g2g failed", ret);
    }

    return BatchReadNotRegisterData(notRegistered, stream, ACL_MEMCPY_DEVICE_TO_HOST);
}

int HostDataOpSDMA::BatchCopyGH2GH(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                                   void *stream) noexcept
{
    return BatchCopyG2G(destinations, sources, counts, batchSize);
}

int HostDataOpSDMA::BatchCopyGH2GD(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                                   void *stream) noexcept
{
    return BatchCopyG2G(destinations, sources, counts, batchSize);
}

int HostDataOpSDMA::BatchCopyGD2GH(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                                   void *stream) noexcept
{
    return BatchCopyG2G(destinations, sources, counts, batchSize);
}

int HostDataOpSDMA::BatchCopyGD2GD(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                                   void *stream) noexcept
{
    return BatchCopyG2G(destinations, sources, counts, batchSize);
}
#endif
}
}
