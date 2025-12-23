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
#include "hybm_data_op_device_rdma.h"

#include <sys/mman.h>
#include <cstdint>

#include "dl_acl_api.h"
#include "hybm_def.h"
#include "hybm_define.h"
#include "hybm_logger.h"
#include "hybm_types.h"
#include "hybm_ptracer.h"
#include "hybm_stream_manager.h"

namespace {
constexpr uint64_t RDMA_SWAP_SPACE_SIZE = 1024 * 1024 * 128;
}

namespace ock {
namespace mf {
DataOpDeviceRDMA::DataOpDeviceRDMA(uint32_t rankId, std::shared_ptr<transport::TransportManager> tm) noexcept
    : rankId_{rankId}, transportManager_{std::move(tm)}
{}

int32_t DataOpDeviceRDMA::Initialize() noexcept
{
    if (inited_) {
        return BM_OK;
    }
    auto ret = AllocSwapMemory();
    if (ret != BM_OK) {
        return ret;
    }
    transport::TransportMemoryRegion input;
    input.addr = reinterpret_cast<uint64_t>(rdmaSwapBaseAddr_);
    input.size = RDMA_SWAP_SPACE_SIZE;
    input.flags = transport::REG_MR_FLAG_DRAM;
    if (transportManager_ != nullptr) {
        ret = transportManager_->RegisterMemoryRegion(input);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to register rdma swap memory, size: " << RDMA_SWAP_SPACE_SIZE);
            FreeSwapMemory();
            return BM_MALLOC_FAILED;
        }
    }
    rdmaSwapMemoryAllocator_ = std::make_shared<RbtreeRangePool>((uint8_t *)rdmaSwapBaseAddr_, RDMA_SWAP_SPACE_SIZE);
    inited_ = true;
    return BM_OK;
}

void DataOpDeviceRDMA::UnInitialize() noexcept
{
    FreeSwapMemory();
    inited_ = false;
}

int32_t DataOpDeviceRDMA::AllocSwapMemory()
{
    void *ptr = nullptr;
    auto ret = DlAclApi::AclrtMallocHost(&ptr, RDMA_SWAP_SPACE_SIZE);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to AclrtMallocHost rdma swap memory, size: " << RDMA_SWAP_SPACE_SIZE);
        return BM_MALLOC_FAILED;
    }
    rdmaSwapBaseAddr_ = ptr;
    return BM_OK;
}

void DataOpDeviceRDMA::FreeSwapMemory()
{
    if (rdmaSwapBaseAddr_ != nullptr) {
        if (transportManager_ != nullptr) {
            const auto ret = transportManager_->UnregisterMemoryRegion((uint64_t)rdmaSwapBaseAddr_);
            if (ret != 0) {
                BM_LOG_ERROR("Failed to UnregisterMemoryRegion, ret: " << ret);
            }
        }
        const auto ret = DlAclApi::AclrtFreeHost(rdmaSwapBaseAddr_);
        if (ret != 0) {
            BM_LOG_ERROR("Failed to AclrtFreeHost swap memory, ret: " << ret);
        }
        rdmaSwapBaseAddr_ = nullptr;
    }
}

DataOpDeviceRDMA::~DataOpDeviceRDMA()
{
    FreeSwapMemory();
    inited_ = false;
}

int32_t DataOpDeviceRDMA::DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                                   const ock::mf::ExtOptions &options) noexcept
{
    int ret;
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_RDMA_LH_TO_GH);
            ret = CopyLH2GH(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_RDMA_LH_TO_GH, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_RDMA_LH_TO_GD);
            ret = CopyLH2GD(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_RDMA_LH_TO_GD, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_RDMA_LD_TO_GH);
            ret = CopyLD2GH(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_RDMA_LD_TO_GH, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_RDMA_LD_TO_GD);
            ret = CopyLD2GD(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_RDMA_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_RDMA_GD_TO_GD);
            ret = CopyGD2GD(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_RDMA_GD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_RDMA_GD_TO_GH);
            ret = CopyGD2GH(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_RDMA_GD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_RDMA_GH_TO_GD);
            ret = CopyGH2GD(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_RDMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_RDMA_GH_TO_GH);
            ret = CopyGH2GH(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_RDMA_GH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_RDMA_GH_TO_LH);
            ret = CopyGH2LH(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_RDMA_GH_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_RDMA_GD_TO_LH);
            ret = CopyGD2LH(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_RDMA_GD_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_RDMA_GH_TO_LD);
            ret = CopyGH2LD(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_RDMA_GH_TO_LD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_RDMA_GD_TO_LD);
            ret = CopyGD2LD(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_RDMA_GD_TO_LD, ret);
            break;
        }
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int32_t DataOpDeviceRDMA::CopyLH2LH(const void *srcVA, void *destVA, uint64_t length,
                                    const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyLH2LH] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    auto ret = DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_HOST);
    if (ret != BM_OK) {
        BM_LOG_ERROR("[CopyLH2LH] AclrtMemcpy failed, ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}
int32_t DataOpDeviceRDMA::CopyLD2LD(const void *srcVA, void *destVA, uint64_t length,
                                    const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyLD2LD] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    auto ret = DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_DEVICE_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("[CopyLD2LD] AclrtMemcpy failed, ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

int32_t DataOpDeviceRDMA::CopyLH2LD(const void *srcVA, void *destVA, uint64_t length,
                                    const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyLH2LD] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    auto ret = DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("[CopyLH2LD] AclrtMemcpy failed, ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

int32_t DataOpDeviceRDMA::CopyLD2LH(const void *srcVA, void *destVA, uint64_t length,
                                    const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyLD2LH] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    auto ret = DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != BM_OK) {
        BM_LOG_ERROR("[CopyLD2LH] AclrtMemcpy failed, ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

int32_t DataOpDeviceRDMA::CopyLH2GH(const void *srcVA, void *destVA, uint64_t length,
                                    const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyLH2GH] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    int32_t ret;
    if (options.destRankId == rankId_) {
        ret = CopyLH2LH(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyLH2GH] Failed to copy src to dest", ret);
    } else {
        ret = SafePut(srcVA, destVA, length, options, true);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyLH2GH] Failed to copy src to dest", ret);
    }
    return ret;
}

int32_t DataOpDeviceRDMA::CopyLH2GD(const void *srcVA, void *destVA, uint64_t length,
                                    const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyLH2GD] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    int32_t ret;
    if (options.destRankId == rankId_) {
        ret = CopyLH2LD(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyLH2GD] Failed to copy src to dest", ret);
    } else {
        ret = SafePut(srcVA, destVA, length, options, true);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyLH2GD] Failed to copy src to dest", ret);
    }
    return ret;
}

int32_t DataOpDeviceRDMA::CopyLD2GH(const void *srcVA, void *destVA, uint64_t length,
                                    const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyLD2GH] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    int32_t ret;
    if (options.destRankId == rankId_) {
        ret = CopyLD2LH(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyLD2GH] Failed to copy src to dest", ret);
    } else {
        ret = SafePut(srcVA, destVA, length, options, false);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyLD2GH] Failed to copy src to dest", ret);
    }
    return ret;
}

int32_t DataOpDeviceRDMA::CopyLD2GD(const void *srcVA, void *destVA, uint64_t length,
                                    const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyLD2GD] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    int32_t ret;
    if (options.destRankId == rankId_) {
        ret = CopyLD2LD(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyLD2GD] Failed to copy src to dest", ret);
    } else {
        ret = SafePut(srcVA, destVA, length, options, false);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyLD2GD] Failed to copy src to dest", ret);
    }
    return ret;
}

int32_t DataOpDeviceRDMA::CopyRDMA(const void *srcVA, void *destVA, uint64_t length,
                                   const ock::mf::ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyRDMA] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    auto src = (uint64_t)(ptrdiff_t)srcVA;
    auto dest = (uint64_t)(ptrdiff_t)destVA;
    int ret;
    if (options.srcRankId == rankId_) {
        ret = transportManager_->WriteRemote(options.destRankId, src, dest, length);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyRDMA] Failed to write src to dest", ret);
    } else if (options.destRankId == rankId_) {
        ret = transportManager_->ReadRemote(options.srcRankId, dest, src, length);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyRDMA] Failed to read src to dest", ret);
    } else {
        BM_LOG_ERROR("[CopyRDMA] invalid param, local rank:" << rankId_ << ", srcId: " << options.srcRankId
                                                             << ", dstId: " << options.destRankId);
        return BM_INVALID_PARAM;
    }
    return ret;
}

int32_t DataOpDeviceRDMA::CopyGH2GH(const void *srcVA, void *destVA, uint64_t length,
                                    const ock::mf::ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyGH2GH] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    int ret;
    if (options.srcRankId == rankId_ && options.destRankId == rankId_) {
        ret = CopyLH2LH(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGH2GH] Failed to copy src to dest", ret);
    } else {
        ret = CopyRDMA(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGH2GH] Failed to rdma src to dest", ret);
    }
    return ret;
}

int32_t DataOpDeviceRDMA::CopyGD2GH(const void *srcVA, void *destVA, uint64_t length,
                                    const ock::mf::ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyGD2GH] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    int ret;
    if (options.srcRankId == rankId_ && options.destRankId == rankId_) {
        ret = CopyLD2LH(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGD2GH] Failed to copy src to dest", ret);
    } else {
        ret = CopyRDMA(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGD2GH] Failed to rdma src to dest", ret);
    }
    return ret;
}
int32_t DataOpDeviceRDMA::CopyGH2GD(const void *srcVA, void *destVA, uint64_t length,
                                    const ock::mf::ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyGH2GD] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    int ret;
    if (options.srcRankId == rankId_ && options.destRankId == rankId_) {
        ret = CopyLH2LD(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGH2GD] Failed to copy src to dest", ret);
    } else {
        ret = CopyRDMA(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGH2GD] Failed to rdma src to dest", ret);
    }
    return ret;
}
int32_t DataOpDeviceRDMA::CopyGD2GD(const void *srcVA, void *destVA, uint64_t length,
                                    const ock::mf::ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyGD2GD] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    int ret;
    if (options.srcRankId == rankId_ && options.destRankId == rankId_) {
        ret = CopyLD2LD(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGD2GD] Failed to copy src to dest", ret);
    } else {
        ret = CopyRDMA(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGD2GD] Failed to rdma src to dest", ret);
    }
    return ret;
}

int32_t DataOpDeviceRDMA::CopyGH2LH(const void *srcVA, void *destVA, uint64_t length,
                                    const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyGH2LH] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    int32_t ret;
    if (options.srcRankId == rankId_) {
        ret = CopyLH2LH(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGH2LH] Failed to copy src to dest", ret);
    } else {
        ret = SafeGet(srcVA, destVA, length, options, true);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGH2LH] Failed to copy src to dest", ret);
    }
    return ret;
}

int32_t DataOpDeviceRDMA::CopyGD2LH(const void *srcVA, void *destVA, uint64_t length,
                                    const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyGD2LH] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    int32_t ret;
    if (options.srcRankId == rankId_) {
        ret = CopyLD2LH(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGD2LH] Failed to copy src to dest", ret);
    } else {
        ret = SafeGet(srcVA, destVA, length, options, true);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGD2LH] Failed to copy src to dest", ret);
    }
    return ret;
}

int32_t DataOpDeviceRDMA::CopyGH2LD(const void *srcVA, void *destVA, uint64_t length,
                                    const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyGH2LD] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    int32_t ret;
    if (options.srcRankId == rankId_) {
        ret = CopyLH2LD(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGH2LD] Failed to copy src to dest", ret);
    } else if (transportManager_->QueryHasRegistered(reinterpret_cast<uint64_t>(destVA), length)) {
        ret = CopyRDMA(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGH2LD] Failed to copy src to dest", ret);
    } else {
        ret = SafeGet(srcVA, destVA, length, options, false);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGH2LD] Failed to copy src to dest", ret);
    }
    return ret;
}

int32_t DataOpDeviceRDMA::CopyGD2LD(const void *srcVA, void *destVA, uint64_t length,
                                    const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("[CopyGD2LD] srcVA=" << srcVA << ", destVA=" << destVA << ", length=" << length);
    int32_t ret;
    if (options.srcRankId == rankId_) {
        ret = CopyLD2LD(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGD2LD] Failed to copy src to dest", ret);
    } else if (transportManager_->QueryHasRegistered(reinterpret_cast<uint64_t>(destVA), length)) {
        ret = CopyRDMA(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGD2LD] Failed to copy src to dest", ret);
    } else {
        ret = SafeGet(srcVA, destVA, length, options, false);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGD2LD] Failed to copy src to dest", ret);
    }
    return ret;
}

int32_t DataOpDeviceRDMA::DataCopyAsync(hybm_copy_params &params,
                                        hybm_data_copy_direction direction, const ExtOptions &options) noexcept
{
    BM_LOG_ERROR("DataOpDeviceRDMA::DataCopyAsync Not Supported!");
    return BM_ERROR;
}

int32_t DataOpDeviceRDMA::Wait(int32_t waitId) noexcept
{
    // Since DataOpDeviceRDMA::DataCopyAsync is not supported, Wait should do nothing for now.
    return BM_OK;
}

int32_t DataOpDeviceRDMA::BatchDataCopyDefault(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                               const ExtOptions &options) noexcept
{
    int32_t ret;
    TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_DEFAULT);
    for (uint32_t i = 0; i < params.batchSize; i++) {
        hybm_copy_params pm = {params.sources[i], params.destinations[i], params.dataSizes[i]};
        ret = DataCopy(pm, direction, options);
        if (ret != BM_OK) {
            break;
        }
    }
    TP_TRACE_END(TP_HYBM_RDMA_BATCH_DEFAULT, ret);
    BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[BatchDataCopy] Failed to copy src to dest", ret);
    return BM_OK;
}

int32_t DataOpDeviceRDMA::BatchCopyLH2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyWrite(params, options, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE);
}

int32_t DataOpDeviceRDMA::BatchCopyGD2LH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyRead(params, options, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST);
}

int32_t DataOpDeviceRDMA::BatchDataCopyLocal(hybm_batch_copy_params &params, int32_t direction,
                                             const ock::mf::ExtOptions &options) noexcept
{
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST:
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST:
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST:
            return BatchDataCopyLocalSync(params, ACL_MEMCPY_HOST_TO_HOST, options);
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE:
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE:
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE:
            return BatchDataCopyLocalAsync(params, ACL_MEMCPY_DEVICE_TO_DEVICE, options);
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE:
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE:
            return BatchDataCopyLocalAsync(params, ACL_MEMCPY_HOST_TO_DEVICE, options);
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST:
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST:
            return BatchDataCopyLocalAsync(params, ACL_MEMCPY_DEVICE_TO_HOST, options);
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE:
            return BatchDataCopyLocalBatch(params, ACL_MEMCPY_HOST_TO_DEVICE, options);
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST:
            return BatchDataCopyLocalBatch(params, ACL_MEMCPY_DEVICE_TO_HOST, options);
        default:
            BM_LOG_ERROR("Failed to BatchDataCopyLocal noy support direct:" << direction);
            return -1;
    }
}

int32_t DataOpDeviceRDMA::BatchDataCopyLocalSync(hybm_batch_copy_params &params, int32_t direction,
                                                 const ExtOptions &options) noexcept
{
    for (size_t i = 0; i < params.batchSize; ++i) {
        auto destAddr = params.destinations[i];
        auto srcAddr = params.sources[i];
        auto count = params.dataSizes[i];
        auto ret = DlAclApi::AclrtMemcpy(destAddr, count, srcAddr, count, direction);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local failed: " << ret << " direct:" << direction);
            return BM_DL_FUNCTION_FAILED;
        }
    }
    return BM_OK;
}

int32_t DataOpDeviceRDMA::BatchDataCopyLocalAsync(hybm_batch_copy_params &params, int32_t direction,
                                                  const ExtOptions &options) noexcept
{
    void *st = options.stream;
    auto ret = 0;
    uint32_t batchNum = params.batchSize;
    if (st == nullptr) {
        st = HybmStreamManager::GetThreadAclStream(HybmGetInitDeviceId());
    }

    for (size_t i = 0; i < batchNum; ++i) {
        auto destAddr = params.destinations[i];
        auto srcAddr = params.sources[i];
        auto count = params.dataSizes[i];
        ret = DlAclApi::AclrtMemcpyAsync(destAddr, count, srcAddr, count, direction, st);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local failed: " << ret << " stream:" << reinterpret_cast<uintptr_t>(st)
                << " direct:" << direction << std::hex << " src:" << srcAddr << " dst:" << destAddr);
            return BM_DL_FUNCTION_FAILED;
        }
    }
    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << reinterpret_cast<uintptr_t>(st));
    }
    return ret;
}

int32_t DataOpDeviceRDMA::BatchDataCopyLocalBatch(hybm_batch_copy_params &params, int32_t direction,
                                                  const ExtOptions &options) noexcept
{
    uint32_t batchNum = params.batchSize;
    std::vector<aclrtMemcpyBatchAttr> attrs(batchNum);
    std::vector<size_t> attrsIds(batchNum);
    std::vector<size_t> sizes(batchNum);
    size_t idx = 0;
    auto deviceLoc = aclrtMemLocation{static_cast<uint32_t>(HybmGetInitDeviceId()),
                                      aclrtMemLocationType::ACL_MEM_LOCATION_TYPE_DEVICE};
    auto hostLoc = aclrtMemLocation{0, aclrtMemLocationType::ACL_MEM_LOCATION_TYPE_HOST};
    for (size_t i = 0; i < batchNum; i++) {
        if (direction == ACL_MEMCPY_HOST_TO_DEVICE) {
            attrs[i] = aclrtMemcpyBatchAttr{deviceLoc, hostLoc, {}};
        } else {
            attrs[i] = aclrtMemcpyBatchAttr{hostLoc, deviceLoc, {}};
        }
        attrsIds[i] = idx++;
        sizes[i] = params.dataSizes[i];
    }
    size_t fail_idx = 0;
    auto ret = DlAclApi::AclrtMemcpyBatch(params.destinations, sizes.data(), params.sources, sizes.data(),
                                          sizes.size(), attrs.data(), attrsIds.data(), attrs.size(), &fail_idx);
    if (ret != 0) {
        return BatchDataCopyLocalAsync(params, direction, options);
    }
    return ret;
}

void DataOpDeviceRDMA::ClassifyDataAddr(void **globalAddrs, void **localAddrs, const uint64_t *counts,
                                        uint32_t batchSize, std::unordered_map<uint32_t, CopyDescriptor> &registered,
                                        std::unordered_map<uint32_t, CopyDescriptor> &localed,
                                        std::unordered_map<uint32_t, CopyDescriptor> &notRegistered,
                                        uint32_t globalRankId) noexcept
{
    for (size_t i = 0; i < batchSize; ++i) {
        uint32_t gvaRankId = GetRankIdByGva(reinterpret_cast<uint64_t>(globalAddrs[i]));
        if (gvaRankId == UINT32_MAX) {
            gvaRankId = globalRankId;
        }
        if (gvaRankId == rankId_) {
            auto iter = localed.find(gvaRankId);
            if (iter == localed.end()) {
                CopyDescriptor desc{};
                desc.localAddrs.push_back(localAddrs[i]);
                desc.globalAddrs.push_back(globalAddrs[i]);
                desc.counts.push_back(counts[i]);
                localed.emplace(std::make_pair(gvaRankId, desc));
            } else {
                iter->second.localAddrs.push_back(localAddrs[i]);
                iter->second.globalAddrs.push_back(globalAddrs[i]);
                iter->second.counts.push_back(counts[i]);
            }
        } else if (!transportManager_->QueryHasRegistered((uint64_t)localAddrs[i], counts[i])) {
            auto iter = notRegistered.find(gvaRankId);
            if (iter == notRegistered.end()) {
                CopyDescriptor desc{};
                desc.localAddrs.push_back(localAddrs[i]);
                desc.globalAddrs.push_back(globalAddrs[i]);
                desc.counts.push_back(counts[i]);
                notRegistered.emplace(std::make_pair(gvaRankId, desc));
            } else {
                iter->second.localAddrs.push_back(localAddrs[i]);
                iter->second.globalAddrs.push_back(globalAddrs[i]);
                iter->second.counts.push_back(counts[i]);
            }
        } else {
            auto iter = registered.find(gvaRankId);
            if (iter == registered.end()) {
                CopyDescriptor desc{};
                desc.localAddrs.push_back(localAddrs[i]);
                desc.globalAddrs.push_back(globalAddrs[i]);
                desc.counts.push_back(counts[i]);
                registered.emplace(std::make_pair(gvaRankId, desc));
            } else {
                iter->second.localAddrs.push_back(localAddrs[i]);
                iter->second.globalAddrs.push_back(globalAddrs[i]);
                iter->second.counts.push_back(counts[i]);
            }
        }
    }
}

int32_t DataOpDeviceRDMA::BatchCopyWrite(hybm_batch_copy_params &params, const ExtOptions &options,
                                         hybm_data_copy_direction direction) noexcept
{
    auto ret = 0;
    ExtOptions tmpOptions = options;
    std::unordered_map<uint32_t, CopyDescriptor> localed{};
    std::unordered_map<uint32_t, CopyDescriptor> registered{};
    std::unordered_map<uint32_t, CopyDescriptor> notRegistered{};
    ClassifyDataAddr(params.destinations, params.sources, params.dataSizes, params.batchSize, registered, localed,
                     notRegistered, options.destRankId);

    // 先写异步
    for (auto &it : registered) {
        hybm_batch_copy_params regParams = {it.second.localAddrs.data(), it.second.globalAddrs.data(),
                                            it.second.counts.data(), static_cast<uint32_t>(it.second.counts.size())};
        tmpOptions.destRankId = it.first;

        for (uint32_t i = 0; i < regParams.batchSize; ++i) {
            ret = transportManager_->WriteRemoteAsync(tmpOptions.destRankId, (uint64_t)regParams.sources[i],
                                                      (uint64_t)regParams.destinations[i], regParams.dataSizes[i]);
            BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to write src to dest", ret);
        }
    }
    // 再写本地
    for (auto &it : localed) {
        hybm_batch_copy_params localParams = {it.second.localAddrs.data(), it.second.globalAddrs.data(),
                                              it.second.counts.data(), static_cast<uint32_t>(it.second.counts.size())};
        tmpOptions.destRankId = it.first;
        TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_LOCAL);
        ret = BatchDataCopyLocal(localParams, direction, tmpOptions);
        TP_TRACE_END(TP_HYBM_RDMA_BATCH_LOCAL, ret);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "write local failed:", ret);
    }
    // 再写未注册
    for (auto &it : notRegistered) {
        hybm_batch_copy_params notParams = {it.second.localAddrs.data(), it.second.globalAddrs.data(),
                                            it.second.counts.data(), static_cast<uint32_t>(it.second.counts.size())};
        tmpOptions.destRankId = it.first;
        ret = BatchDataCopyDefault(notParams, direction, tmpOptions);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "write default failed:", ret);
    }
    // 再等异步
    for (auto &it : registered) {
        TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_WAIT_W);
        ret = transportManager_->Synchronize(it.first);
        TP_TRACE_END(TP_HYBM_RDMA_BATCH_WAIT_W, ret);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to Synchronize", ret);
    }
    return BM_OK;
}

int32_t DataOpDeviceRDMA::BatchCopyRead(hybm_batch_copy_params &params, const ExtOptions &options,
                                        hybm_data_copy_direction direction) noexcept
{
    auto ret = 0;
    ExtOptions tmpOptions = options;
    std::unordered_map<uint32_t, CopyDescriptor> localed{};
    std::unordered_map<uint32_t, CopyDescriptor> registered{};
    std::unordered_map<uint32_t, CopyDescriptor> notRegistered{};
    ClassifyDataAddr(params.sources, params.destinations, params.dataSizes, params.batchSize, registered, localed,
                     notRegistered, options.srcRankId);

    // 先写异步
    for (auto &it : registered) {
        hybm_batch_copy_params regParams = {it.second.globalAddrs.data(), it.second.localAddrs.data(),
                                            it.second.counts.data(), static_cast<uint32_t>(it.second.counts.size())};
        tmpOptions.srcRankId = it.first;
        for (uint32_t i = 0; i < regParams.batchSize; ++i) {
            ret = transportManager_->ReadRemoteAsync(tmpOptions.srcRankId, (uint64_t)regParams.destinations[i],
                                                     (uint64_t)regParams.sources[i], regParams.dataSizes[i]);
            BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to read src to dest", ret);
        }
    }
    // 再写本地
    for (auto &it : localed) {
        hybm_batch_copy_params localParams = {it.second.globalAddrs.data(), it.second.localAddrs.data(),
                                              it.second.counts.data(), static_cast<uint32_t>(it.second.counts.size())};
        tmpOptions.destRankId = it.first;
        TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_LOCAL);
        ret = BatchDataCopyLocal(localParams, direction, tmpOptions);
        TP_TRACE_END(TP_HYBM_RDMA_BATCH_LOCAL, ret);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "read local failed:", ret);
    }
    // 再写未注册
    for (auto &it : notRegistered) {
        hybm_batch_copy_params notParams = {it.second.globalAddrs.data(), it.second.localAddrs.data(),
                                            it.second.counts.data(), static_cast<uint32_t>(it.second.counts.size())};
        tmpOptions.srcRankId = it.first;
        ret = BatchDataCopyDefault(notParams, direction, tmpOptions);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "write default failed:", ret);
    }
    // 再等异步
    for (auto &it : registered) {
        TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_WAIT_R);
        ret = transportManager_->Synchronize(it.first);
        TP_TRACE_END(TP_HYBM_RDMA_BATCH_WAIT_R, ret);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to Synchronize", ret);
    }
    return BM_OK;
}

int32_t DataOpDeviceRDMA::BatchCopyLD2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyWrite(params, options, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE);
}

int32_t DataOpDeviceRDMA::BatchCopyLD2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyWrite(params, options, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST);
}

int32_t DataOpDeviceRDMA::BatchCopyGH2LD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyRead(params, options, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE);
}

int32_t DataOpDeviceRDMA::BatchCopyGD2LD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyRead(params, options, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE);
}

int32_t DataOpDeviceRDMA::BatchCopyLH2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyWrite(params, options, HYBM_LOCAL_HOST_TO_GLOBAL_HOST);
}

int32_t DataOpDeviceRDMA::BatchCopyG2G(hybm_batch_copy_params &params, const ExtOptions &options,
                                       hybm_data_copy_direction direction) noexcept
{
    auto ret = 0;
    auto batchSize = params.batchSize;
    ExtOptions tmpOptions = options;
    std::set<uint32_t> asyncWriteRanks{};
    // 先写异步
    for (uint32_t i = 0; i < batchSize; i++) {
        uint32_t srcRankId = GetRankIdByGva(reinterpret_cast<uint64_t>(params.sources[i]));
        uint32_t dstRankId = GetRankIdByGva(reinterpret_cast<uint64_t>(params.destinations[i]));
        tmpOptions.srcRankId = srcRankId;
        tmpOptions.destRankId = dstRankId;

        if (srcRankId == rankId_ && dstRankId == rankId_) {
            hybm_copy_params pm = {params.sources[i], params.destinations[i], params.dataSizes[i]};
            ret = DataCopy(pm, direction, tmpOptions);
            BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "write default failed:", ret);
        } else if (srcRankId == rankId_) {
            ret = transportManager_->WriteRemoteAsync(tmpOptions.destRankId, (uint64_t)params.sources[i],
                                                      (uint64_t)params.destinations[i], params.dataSizes[i]);
            BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to write src to dest", ret);
            asyncWriteRanks.insert(tmpOptions.destRankId);
        } else if (dstRankId == rankId_) {
            ret = transportManager_->ReadRemoteAsync(tmpOptions.srcRankId, (uint64_t)params.destinations[i],
                                                     (uint64_t)params.sources[i], params.dataSizes[i]);
            BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to write src to dest", ret);
            asyncWriteRanks.insert(tmpOptions.srcRankId);
        } else {
            BM_LOG_ERROR("invalid param, local rank:" << rankId_ << ", srcId: " << srcRankId
                                                      << ", dstId: " << dstRankId);
            return BM_ERROR;
        }
    }
    // 再等异步
    for (auto &it : asyncWriteRanks) {
        TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_WAIT_W);
        ret = transportManager_->Synchronize(it);
        TP_TRACE_END(TP_HYBM_RDMA_BATCH_WAIT_W, ret);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to Synchronize", ret);
    }
    return ret;
}

int32_t DataOpDeviceRDMA::SafePut(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options,
                                  bool srcIsHost)
{
    int32_t ret = 0;
    uintptr_t srcBase = reinterpret_cast<uintptr_t>(srcVA);
    uintptr_t destBase = reinterpret_cast<uintptr_t>(destVA);
    uint64_t remainingLength = length;
    uint64_t offset = 0;
    while (remainingLength > 0) {
        uint64_t currentChunkSize = std::min(remainingLength, RDMA_SWAP_SPACE_SIZE);
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(currentChunkSize);
        auto tmpHost = tmpRdmaMemory.Address();
        BM_ASSERT_LOG_AND_RETURN(tmpHost != nullptr, "Failed to malloc temp buffer", BM_MALLOC_FAILED);
        const void *currentSrc = reinterpret_cast<const void *>(srcBase + offset);
        void *currentDest = reinterpret_cast<void *>(destBase + offset);
        if (srcIsHost) {
            ret = CopyLH2LH(currentSrc, tmpHost, currentChunkSize, options);
        } else {
            ret = CopyLD2LH(currentSrc, tmpHost, currentChunkSize, options);
        }
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to tmp", ret);
        ret = CopyRDMA(tmpHost, currentDest, currentChunkSize, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy tmp to dest", ret);
        offset += currentChunkSize;
        remainingLength -= currentChunkSize;
    }
    return 0;
}

int32_t DataOpDeviceRDMA::SafeGet(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options,
                                  bool destIsHost)
{
    int32_t ret = 0;
    uintptr_t srcBase = reinterpret_cast<uintptr_t>(srcVA);
    uintptr_t destBase = reinterpret_cast<uintptr_t>(destVA);
    uint64_t remainingLength = length;
    uint64_t offset = 0;
    while (remainingLength > 0) {
        uint64_t currentChunkSize = std::min(remainingLength, RDMA_SWAP_SPACE_SIZE);
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(currentChunkSize);
        auto tmpHost = tmpRdmaMemory.Address();
        BM_ASSERT_LOG_AND_RETURN(tmpHost != nullptr, "[CopyGD2LH] Failed to malloc temp buffer", BM_MALLOC_FAILED);
        const void *currentSrc = reinterpret_cast<const void *>(srcBase + offset);
        void *currentDest = reinterpret_cast<void *>(destBase + offset);
        ret = CopyRDMA(currentSrc, tmpHost, currentChunkSize, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGD2LH] Failed to copy src to tmp", ret);
        if (destIsHost) {
            ret = CopyLH2LH(tmpHost, currentDest, currentChunkSize, options);
        } else {
            ret = CopyLH2LD(tmpHost, currentDest, currentChunkSize, options);
        }
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[CopyGD2LH] Failed to copy tmp to dest", ret);
        offset += currentChunkSize;
        remainingLength -= currentChunkSize;
    }
    return 0;
}

int32_t DataOpDeviceRDMA::BatchCopyGH2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyG2G(params, options, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST);
}

int32_t DataOpDeviceRDMA::BatchCopyGH2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyG2G(params, options, HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE);
}

int32_t DataOpDeviceRDMA::BatchCopyGH2LH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyRead(params, options, HYBM_GLOBAL_HOST_TO_LOCAL_HOST);
}

int32_t DataOpDeviceRDMA::BatchCopyGD2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyG2G(params, options, HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST);
}

int32_t DataOpDeviceRDMA::BatchCopyGD2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyG2G(params, options, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE);
}

int32_t DataOpDeviceRDMA::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                        const ExtOptions &options) noexcept
{
    auto ret = 0;
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: { // 0
            TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_LH_TO_GH);
            ret = BatchCopyLH2GH(params, options);
            TP_TRACE_END(TP_HYBM_RDMA_BATCH_LH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST: { // 4
            TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_GH_TO_GH);
            ret = BatchCopyGH2GH(params, options);
            TP_TRACE_END(TP_HYBM_RDMA_BATCH_GH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE: { // 5
            TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_GH_TO_GD);
            ret = BatchCopyGH2GD(params, options);
            TP_TRACE_END(TP_HYBM_RDMA_BATCH_GH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST: { // 6
            TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_GH_TO_LH);
            ret = BatchCopyGH2LH(params, options);
            TP_TRACE_END(TP_HYBM_RDMA_BATCH_GH_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST: { // 8
            TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_GD_TO_GH);
            ret = BatchCopyGD2GH(params, options);
            TP_TRACE_END(TP_HYBM_RDMA_BATCH_GD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE: { // 9
            TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_GD_TO_GD);
            ret = BatchCopyGD2GD(params, options);
            TP_TRACE_END(TP_HYBM_RDMA_BATCH_GD_TO_GD, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE: { // 1
            TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_LH_TO_GD);
            ret = BatchCopyLH2GD(params, options);
            TP_TRACE_END(TP_HYBM_RDMA_BATCH_LH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST: { // 10
            TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_GD_TO_LH);
            ret = BatchCopyGD2LH(params, options);
            TP_TRACE_END(TP_HYBM_RDMA_BATCH_GD_TO_LH, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: { // 2
            TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_LD_TO_GH);
            ret = BatchCopyLD2GH(params, options);
            TP_TRACE_END(TP_HYBM_RDMA_BATCH_LD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: {  // 7
            TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_GH_TO_LD);
            ret = BatchCopyGH2LD(params, options);
            TP_TRACE_END(TP_HYBM_RDMA_BATCH_GH_TO_LD, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE: { // 3
            TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_LD_TO_GD);
            ret = BatchCopyLD2GD(params, options);
            TP_TRACE_END(TP_HYBM_RDMA_BATCH_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: { // 11
            TP_TRACE_BEGIN(TP_HYBM_RDMA_BATCH_GD_TO_LD);
            ret = BatchCopyGD2LD(params, options);
            TP_TRACE_END(TP_HYBM_RDMA_BATCH_GD_TO_LD, ret);
            break;
        }
        default: {
            ret = BM_ERROR;
            BM_LOG_ERROR("unexcepted direction:" << direction);
            break;
        }
    }
    return ret;
}
} // namespace mf
} // namespace ock