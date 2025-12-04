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
#include "hybm_data_op.h"
#include "hybm_gva.h"
#include "hybm_stream_manager.h"

namespace ock {
namespace mf {
constexpr uint64_t HBM_SWAP_SPACE_SIZE = 128 * 1024 * 1024;
thread_local HybmStreamPtr HostDataOpSDMA::stream_ = nullptr;
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
    BM_ASSERT_RETURN(PrepareThreadLocalStream() == BM_OK, BM_ERROR);
    if (stream_ != nullptr) {
        return stream_->Synchronize();
    }
    return BM_OK;
}

void HostDataOpSDMA::CleanUp() noexcept
{
    WriteGuard lockGuard(lock_);
    for (auto &it : streamMask_) {
        it.second = true;
    }
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

bool HostDataOpSDMA::IsResetStream() noexcept
{
    uint64_t tid = static_cast<uint64_t>(syscall(SYS_gettid));
    ReadGuard lockGuard(lock_);
    auto it = streamMask_.find(tid);
    if (it == streamMask_.end()) {
        return true;
    }
    return it->second;
}

int HostDataOpSDMA::PrepareThreadLocalStream() noexcept
{
    if (stream_ != nullptr && !IsResetStream()) {
        return BM_OK;
    }

    stream_ = HybmStreamManager::GetThreadHybmStream(HybmGetInitedLogicDeviceId(), 0, 0);
    if (stream_ == nullptr) {
        BM_LOG_ERROR("HybmStream init failed");
        return BM_ERROR;
    }
    uint64_t tid = static_cast<uint64_t>(syscall(SYS_gettid));
    WriteGuard lockGuard(lock_);
    streamMask_[tid] = false;
   
    BM_LOG_INFO("PrepareThreadLocalStream success, tid:" << tid);
    return BM_OK;
}

void HostDataOpSDMA::InitG2GStreamTask(StreamTask &task) noexcept
{
    if (PrepareThreadLocalStream() != BM_OK) {
        BM_LOG_ERROR("Failed to get thread local hybmStream");
        return;
    }
    task.type = STREAM_TASK_TYPE_SDMA;
    rtStarsMemcpyAsyncSqe_t *const sqe = &(task.sqe.memcpyAsyncSqe);
    sqe->header.type = RT_STARS_SQE_TYPE_SDMA;
    sqe->header.ie = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.pre_p = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.wr_cqe = stream_->GetWqeFlag();
    sqe->header.rt_stream_id = stream_->GetId();
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
    BM_ASSERT_RETURN(PrepareThreadLocalStream() == BM_OK, BM_ERROR);
    sqe->length = count;
    sqe->src_addr_low =
            static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(srcVA)) & 0x00000000FFFFFFFFU);
    sqe->src_addr_high = static_cast<uint32_t>(
            (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(srcVA)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);
    sqe->dst_addr_low =
            static_cast<uint32_t>(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(destVA)) & 0x00000000FFFFFFFFU);
    sqe->dst_addr_high = static_cast<uint32_t>(
            (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(destVA)) & 0xFFFFFFFF00000000U) >> UINT32_BIT_NUM);

    auto ret = stream_->SubmitTasks(task);
    BM_ASSERT_RETURN(ret == 0, BM_ERROR);

    ret = stream_->Synchronize();
    BM_ASSERT_RETURN(ret == 0, BM_ERROR);
    return BM_OK;
}

int HostDataOpSDMA::CopyG2GAsync(void *destVA, const void *srcVA, size_t count) noexcept
{
    BM_LOG_DEBUG("CopyG2GAsync src:" << srcVA << " destVA:" << destVA << " length:" << count);
    StreamTask task{};
    InitG2GStreamTask(task);
    rtStarsMemcpyAsyncSqe_t *const sqe = &(task.sqe.memcpyAsyncSqe);
    BM_ASSERT_RETURN(PrepareThreadLocalStream() == BM_OK, BM_ERROR);
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
    auto ret = stream_->SubmitTasks(task);
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
}
}
