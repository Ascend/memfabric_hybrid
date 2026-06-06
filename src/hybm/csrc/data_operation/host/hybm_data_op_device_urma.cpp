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
#include "hybm_data_op_device_urma.h"

#include <sys/mman.h>
#include <cstdint>

#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "hybm_def.h"
#include "hybm_define.h"
#include "hybm_logger.h"
#include "hybm_types.h"
#include "hybm_ptracer.h"
#include "hybm_gva.h"
#include "hybm_stream_manager.h"
#include "hybm_va_manager.h"

namespace {
constexpr uint64_t URMA_SWAP_SPACE_SIZE = 1024 * 1024 * 128;
}

namespace ock {
namespace mf {

// clang-format off
static hybm_mem_type HybmDirectionSrcMemType[HYBM_DATA_COPY_DIRECTION_BUTT] = {
    HYBM_MEM_TYPE_HOST,
    HYBM_MEM_TYPE_HOST,
    HYBM_MEM_TYPE_DEVICE,
    HYBM_MEM_TYPE_DEVICE,
    HYBM_MEM_TYPE_HOST,
    HYBM_MEM_TYPE_HOST,
    HYBM_MEM_TYPE_HOST,
    HYBM_MEM_TYPE_HOST,
    HYBM_MEM_TYPE_DEVICE,
    HYBM_MEM_TYPE_DEVICE,
    HYBM_MEM_TYPE_DEVICE,
    HYBM_MEM_TYPE_DEVICE,
    HYBM_MEM_TYPE_BUTT
};
static hybm_mem_type HybmDirectionDestMemType[HYBM_DATA_COPY_DIRECTION_BUTT] = {
    HYBM_MEM_TYPE_HOST,
    HYBM_MEM_TYPE_DEVICE,
    HYBM_MEM_TYPE_HOST,
    HYBM_MEM_TYPE_DEVICE,
    HYBM_MEM_TYPE_HOST,
    HYBM_MEM_TYPE_DEVICE,
    HYBM_MEM_TYPE_HOST,
    HYBM_MEM_TYPE_DEVICE,
    HYBM_MEM_TYPE_HOST,
    HYBM_MEM_TYPE_DEVICE,
    HYBM_MEM_TYPE_HOST,
    HYBM_MEM_TYPE_DEVICE,
    HYBM_MEM_TYPE_BUTT
};
// clang-format on

DataOpDeviceURMA::DataOpDeviceURMA(uint32_t rankId, std::shared_ptr<transport::TransportManager> tm) noexcept
    : rankId_{rankId}, transportManager_{std::move(tm)}
{}

Result DataOpDeviceURMA::Initialize() noexcept
{
    if (inited_) {
        return BM_OK;
    }
    auto ret = AllocSwapMemory();
    if (ret != BM_OK) {
        return ret;
    }
    transport::TransportMemoryRegion input;
    input.addr = reinterpret_cast<uint64_t>(urmaSwapBaseAddr_);
    input.size = URMA_SWAP_SPACE_SIZE;
    input.flags = transport::REG_MR_FLAG_ACL_DRAM;
    if (transportManager_ != nullptr) {
        ret = transportManager_->RegisterMemoryRegion(input);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to register urma swap memory, size: " << URMA_SWAP_SPACE_SIZE);
            FreeSwapMemory();
            return BM_MALLOC_FAILED;
        }
    }
    urmaSwapMemoryAllocator_ = std::make_shared<RbtreeRangePool>((uint8_t *)urmaSwapBaseAddr_, URMA_SWAP_SPACE_SIZE);
    inited_ = true;
    return BM_OK;
}

void DataOpDeviceURMA::UnInitialize() noexcept
{
    FreeSwapMemory();
    inited_ = false;
}

void DataOpDeviceURMA::TransformVa(void *&src, void *&dst, hybm_data_copy_direction direction) noexcept
{
    // DRAM需要输入host va, HBM需要输入device va, transport才能识别
    uint64_t out;
    uint32_t oType = (HybmDirectionSrcMemType[direction] == HYBM_MEM_TYPE_HOST) ? HVM_HVA : HVM_DVA;
    out = HybmVaManager::GetInstance().TransformVa(reinterpret_cast<uint64_t>(src), HVM_GVA, oType);
    if (out != 0) {
        src = reinterpret_cast<void *>(out);
    }

    oType = (HybmDirectionDestMemType[direction] == HYBM_MEM_TYPE_HOST) ? HVM_HVA : HVM_DVA;
    out = HybmVaManager::GetInstance().TransformVa(reinterpret_cast<uint64_t>(dst), HVM_GVA, oType);
    if (out != 0) {
        dst = reinterpret_cast<void *>(out);
    }
}

Result DataOpDeviceURMA::AllocSwapMemory()
{
    void *ptr = nullptr;
    auto ret = DlAclApi::AclrtMallocHost(&ptr, URMA_SWAP_SPACE_SIZE);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to AclrtMallocHost urma swap memory, size: " << URMA_SWAP_SPACE_SIZE);
        return BM_MALLOC_FAILED;
    }

    void *output;
    ret = DlHalApi::HalHostRegister(ptr, URMA_SWAP_SPACE_SIZE, HOST_MEM_MAP_DEV, HybmGetInitedLogicDeviceId(), &output);
    if (ret != 0) {
        BM_LOG_ERROR("Register swap mem failed, addr: " << ptr << " ret: " << ret);
        auto ret2 = DlAclApi::AclrtFreeHost(ptr);
        if (ret2 != 0) {
            BM_LOG_ERROR("Failed to AclrtFreeHost swap memory, ret: " << ret2);
        }
        return ret;
    }
    ret =
        HybmVaManager::GetInstance().AddVaInfo({0, reinterpret_cast<uint64_t>(output), reinterpret_cast<uint64_t>(ptr),
                                                URMA_SWAP_SPACE_SIZE, HYBM_MEM_TYPE_HOST},
                                               rankId_);
    if (ret != 0) {
        BM_LOG_ERROR("add va info failed, va:" << ptr << " ret:" << ret);
        FreeSwapMemory();
        return ret;
    }

    urmaSwapBaseAddr_ = ptr;
    return BM_OK;
}

void DataOpDeviceURMA::FreeSwapMemory()
{
    if (urmaSwapBaseAddr_ != nullptr) {
        if (transportManager_ != nullptr) {
            const auto ret = transportManager_->UnregisterMemoryRegion((uint64_t)urmaSwapBaseAddr_);
            if (ret != 0) {
                BM_LOG_ERROR("Failed to UnregisterMemoryRegion, ret: " << ret);
            }
        }
        DlHalApi::HalHostUnregisterEx(urmaSwapBaseAddr_, HybmGetInitDeviceId(), HOST_MEM_MAP_DEV);
        const auto ret = DlAclApi::AclrtFreeHost(urmaSwapBaseAddr_);
        if (ret != 0) {
            BM_LOG_ERROR("Failed to AclrtFreeHost swap memory, ret: " << ret);
        }
        HybmVaManager::GetInstance().RemoveOneVaInfo(reinterpret_cast<uint64_t>(urmaSwapBaseAddr_), HVM_HVA);
        urmaSwapBaseAddr_ = nullptr;
    }
}

DataOpDeviceURMA::~DataOpDeviceURMA()
{
    FreeSwapMemory();
    inited_ = false;
}

Result DataOpDeviceURMA::DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                                  const ock::mf::ExtOptions &options) noexcept
{
    Result ret;
    // Selective transform: only convert local-side addresses; remote side stays in GVA.
    {
        const bool srcLocal = (options.srcRankId == rankId_);
        const bool dstLocal = (options.destRankId == rankId_);
        if (srcLocal && dstLocal) {
            TransformVa(params.src, params.dest, direction);
        } else if (srcLocal) {
            void *tmpDst = nullptr;
            TransformVa(params.src, tmpDst, direction);
        } else if (dstLocal) {
            void *tmpSrc = nullptr;
            TransformVa(tmpSrc, params.dest, direction);
        }
    }
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_URMA_LH_TO_GH);
            ret = CopyLH2GH(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_URMA_LH_TO_GH, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_URMA_LH_TO_GD);
            ret = CopyLH2GD(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_URMA_LH_TO_GD, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_URMA_LD_TO_GH);
            ret = CopyLD2GH(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_URMA_LD_TO_GH, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_URMA_LD_TO_GD);
            ret = CopyLD2GD(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_URMA_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_URMA_GD_TO_GD);
            ret = CopyGD2GD(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_URMA_GD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_URMA_GD_TO_GH);
            ret = CopyGD2GH(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_URMA_GD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_URMA_GH_TO_GD);
            ret = CopyGH2GD(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_URMA_GH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_URMA_GH_TO_GH);
            ret = CopyGH2GH(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_URMA_GH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_URMA_GH_TO_LH);
            ret = CopyGH2LH(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_URMA_GH_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_URMA_GD_TO_LH);
            ret = CopyGD2LH(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_URMA_GD_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_URMA_GH_TO_LD);
            ret = CopyGH2LD(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_URMA_GH_TO_LD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_URMA_GD_TO_LD);
            ret = CopyGD2LD(params.src, params.dest, params.dataSize, options);
            TP_TRACE_END(TP_HYBM_URMA_GD_TO_LD, ret);
            break;
        }
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

Result DataOpDeviceURMA::CopyLH2LH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    auto ret = DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_HOST);
    if (ret != BM_OK) {
        BM_LOG_ERROR("AclrtMemcpy failed, ret: " << ret << " Src=" << VaToInfo(srcVA) << " dest=" << VaToInfo(destVA)
                                                 << " length=" << length);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}
Result DataOpDeviceURMA::CopyLD2LD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    auto ret = DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_DEVICE_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("AclrtMemcpy failed, ret: " << ret << " Src=" << VaToInfo(srcVA) << " dest=" << VaToInfo(destVA)
                                                 << " length=" << length);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

Result DataOpDeviceURMA::CopyLH2LD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    auto ret = DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("AclrtMemcpy failed, ret: " << ret << " Src=" << VaToInfo(srcVA) << " dest=" << VaToInfo(destVA)
                                                 << " length=" << length);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

Result DataOpDeviceURMA::CopyLD2LH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    auto ret = DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != BM_OK) {
        BM_LOG_ERROR("AclrtMemcpy failed, ret: " << ret << " Src=" << VaToInfo(srcVA) << " dest=" << VaToInfo(destVA)
                                                 << " length=" << length);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

Result DataOpDeviceURMA::CopyLH2GH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    Result ret;
    if (options.destRankId == rankId_) {
        ret = CopyLH2LH(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    } else {
        ret = SafePut(srcVA, destVA, length, options, true);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    }
    return ret;
}

Result DataOpDeviceURMA::CopyLH2GD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    Result ret;
    if (options.destRankId == rankId_) {
        ret = CopyLH2LD(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    } else {
        ret = SafePut(srcVA, destVA, length, options, true);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    }
    return ret;
}

Result DataOpDeviceURMA::CopyLD2GH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    Result ret;
    if (options.destRankId == rankId_) {
        ret = CopyLD2LH(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    } else {
        ret = SafePut(srcVA, destVA, length, options, false);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    }
    return ret;
}

Result DataOpDeviceURMA::CopyLD2GD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    Result ret;
    if (options.destRankId == rankId_) {
        ret = CopyLD2LD(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    } else {
        ret = SafePut(srcVA, destVA, length, options, false);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    }
    return ret;
}

Result DataOpDeviceURMA::CopyURMA(const void *srcVA, void *destVA, uint64_t length,
                                  const ock::mf::ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    auto src = (uint64_t)(ptrdiff_t)srcVA;
    auto dest = (uint64_t)(ptrdiff_t)destVA;
    Result ret;
    if (options.srcRankId == rankId_) {
        ret = transportManager_->WriteRemote(options.destRankId, src, dest, length);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to write src to dest", ret);
    } else if (options.destRankId == rankId_) {
        ret = transportManager_->ReadRemote(options.srcRankId, dest, src, length);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to read src to dest", ret);
    } else {
        BM_LOG_ERROR("Invalid param, local rank:" << rankId_ << ", srcId: " << options.srcRankId
                                                  << ", dstId: " << options.destRankId);
        return BM_INVALID_PARAM;
    }
    return ret;
}

Result DataOpDeviceURMA::CopyGH2GH(const void *srcVA, void *destVA, uint64_t length,
                                   const ock::mf::ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    Result ret;
    if (options.srcRankId == rankId_ && options.destRankId == rankId_) {
        ret = CopyLH2LH(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    } else {
        ret = CopyURMA(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to urma src to dest", ret);
    }
    return ret;
}

Result DataOpDeviceURMA::CopyGD2GH(const void *srcVA, void *destVA, uint64_t length,
                                   const ock::mf::ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    Result ret;
    if (options.srcRankId == rankId_ && options.destRankId == rankId_) {
        ret = CopyLD2LH(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    } else {
        ret = CopyURMA(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to urma src to dest", ret);
    }
    return ret;
}
Result DataOpDeviceURMA::CopyGH2GD(const void *srcVA, void *destVA, uint64_t length,
                                   const ock::mf::ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    Result ret;
    if (options.srcRankId == rankId_ && options.destRankId == rankId_) {
        ret = CopyLH2LD(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    } else {
        ret = CopyURMA(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to urma src to dest", ret);
    }
    return ret;
}

Result DataOpDeviceURMA::CopyGD2GD(const void *srcVA, void *destVA, uint64_t length,
                                   const ock::mf::ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    Result ret;
    if (options.srcRankId == rankId_ && options.destRankId == rankId_) {
        ret = CopyLD2LD(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    } else {
        ret = CopyURMA(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to urma src to dest", ret);
    }
    return ret;
}

Result DataOpDeviceURMA::CopyGH2LH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    Result ret;
    if (options.srcRankId == rankId_) {
        ret = CopyLH2LH(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    } else {
        ret = SafeGet(srcVA, destVA, length, options, true);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    }
    return ret;
}

Result DataOpDeviceURMA::CopyGD2LH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    Result ret;
    if (options.srcRankId == rankId_) {
        ret = CopyLD2LH(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    } else {
        ret = SafeGet(srcVA, destVA, length, options, true);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    }
    return ret;
}

Result DataOpDeviceURMA::CopyGH2LD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    Result ret;
    if (options.srcRankId == rankId_) {
        ret = CopyLH2LD(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    } else {
        ret = SafeGet(srcVA, destVA, length, options, false);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    }
    return ret;
}

Result DataOpDeviceURMA::CopyGD2LD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept
{
    BM_LOG_DEBUG("SrcVA=" << VaToInfo(srcVA) << ", destVA=" << VaToInfo(destVA) << ", length=" << length);
    Result ret;
    if (options.srcRankId == rankId_) {
        ret = CopyLD2LD(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    } else {
        ret = SafeGet(srcVA, destVA, length, options, false);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy src to dest", ret);
    }
    return ret;
}

Result DataOpDeviceURMA::DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                                       const ExtOptions &options) noexcept
{
    BM_LOG_ERROR("DataOpDeviceURMA::DataCopyAsync Not Supported!");
    return BM_ERROR;
}

Result DataOpDeviceURMA::Wait(int32_t waitId) noexcept
{
    // Since DataOpDeviceURMA::DataCopyAsync is not supported, Wait should do nothing for now.
    return BM_OK;
}

Result DataOpDeviceURMA::BatchDataCopyDefault(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                              const ExtOptions &options) noexcept
{
    Result ret;
    TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_DEFAULT);
    for (uint32_t i = 0; i < params.batchSize; i++) {
        hybm_copy_params pm = {params.sources[i], params.destinations[i], params.dataSizes[i]};
        ret = DataCopy(pm, direction, options);
        if (ret != BM_OK) {
            break;
        }
    }
    TP_TRACE_END(TP_HYBM_URMA_BATCH_DEFAULT, ret);
    BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "[BatchDataCopy] Failed to copy src to dest", ret);
    return BM_OK;
}

Result DataOpDeviceURMA::BatchCopyLH2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyWrite(params, options, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE);
}

Result DataOpDeviceURMA::BatchCopyGD2LH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyRead(params, options, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST);
}

Result DataOpDeviceURMA::BatchDataCopyLocal(hybm_batch_copy_params &params, int32_t direction,
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
            BM_LOG_ERROR("Failed to BatchDataCopyLocal not support direct:" << direction);
            return -1;
    }
}

Result DataOpDeviceURMA::BatchDataCopyLocalSync(hybm_batch_copy_params &params, int32_t direction,
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

Result DataOpDeviceURMA::BatchDataCopyLocalAsync(hybm_batch_copy_params &params, int32_t direction,
                                                 const ExtOptions &options) noexcept
{
    void *st = options.stream;
    auto ret = 0;
    uint32_t batchNum = params.batchSize;
    if (st == nullptr) {
        st = HybmStreamManager::GetThreadAclStream();
    }

    for (size_t i = 0; i < batchNum; ++i) {
        auto destAddr = params.destinations[i];
        auto srcAddr = params.sources[i];
        auto count = params.dataSizes[i];
        ret = DlAclApi::AclrtMemcpyAsync(destAddr, count, srcAddr, count, direction, st);
        if (ret != 0) {
            (void)DlAclApi::AclrtSynchronizeStream(st);
            BM_LOG_ERROR("copy memory on local failed: " << ret << " stream:" << reinterpret_cast<uintptr_t>(st)
                                                         << " direct:" << direction << std::hex << " src:" << srcAddr
                                                         << " dst:" << destAddr);
            return BM_DL_FUNCTION_FAILED;
        }
    }
    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << reinterpret_cast<uintptr_t>(st));
    }
    return ret;
}

Result DataOpDeviceURMA::BatchDataCopyLocalBatch(hybm_batch_copy_params &params, int32_t direction,
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
    auto ret = DlAclApi::AclrtMemcpyBatch(params.destinations, sizes.data(), params.sources, sizes.data(), sizes.size(),
                                          attrs.data(), attrsIds.data(), attrs.size(), &fail_idx);
    if (ret != 0) {
        return BatchDataCopyLocalAsync(params, direction, options);
    }
    return ret;
}

void DataOpDeviceURMA::ClassifyDataAddr(void **globalAddrs, void **localAddrs, const uint64_t *counts,
                                        uint32_t batchSize, std::unordered_map<uint32_t, CopyDescriptor> &registered,
                                        std::unordered_map<uint32_t, CopyDescriptor> &localed,
                                        std::unordered_map<uint32_t, CopyDescriptor> &notRegistered,
                                        uint32_t globalRankId) noexcept
{
    for (size_t i = 0; i < batchSize; ++i) {
        if (globalRankId == rankId_) {
            auto iter = localed.find(globalRankId);
            if (iter == localed.end()) {
                CopyDescriptor desc{};
                desc.localAddrs.push_back(localAddrs[i]);
                desc.globalAddrs.push_back(globalAddrs[i]);
                desc.counts.push_back(counts[i]);
                localed.emplace(std::make_pair(globalRankId, desc));
            } else {
                iter->second.localAddrs.push_back(localAddrs[i]);
                iter->second.globalAddrs.push_back(globalAddrs[i]);
                iter->second.counts.push_back(counts[i]);
            }
        } else if (!transportManager_->QueryHasRegistered((uint64_t)localAddrs[i], counts[i])) {
            auto iter = notRegistered.find(globalRankId);
            if (iter == notRegistered.end()) {
                CopyDescriptor desc{};
                desc.localAddrs.push_back(localAddrs[i]);
                desc.globalAddrs.push_back(globalAddrs[i]);
                desc.counts.push_back(counts[i]);
                notRegistered.emplace(std::make_pair(globalRankId, desc));
            } else {
                iter->second.localAddrs.push_back(localAddrs[i]);
                iter->second.globalAddrs.push_back(globalAddrs[i]);
                iter->second.counts.push_back(counts[i]);
            }
        } else {
            auto iter = registered.find(globalRankId);
            if (iter == registered.end()) {
                CopyDescriptor desc{};
                desc.localAddrs.push_back(localAddrs[i]);
                desc.globalAddrs.push_back(globalAddrs[i]);
                desc.counts.push_back(counts[i]);
                registered.emplace(std::make_pair(globalRankId, desc));
            } else {
                iter->second.localAddrs.push_back(localAddrs[i]);
                iter->second.globalAddrs.push_back(globalAddrs[i]);
                iter->second.counts.push_back(counts[i]);
            }
        }
    }
}

Result DataOpDeviceURMA::BatchCopyWrite(hybm_batch_copy_params &params, const ExtOptions &options,
                                        hybm_data_copy_direction direction) noexcept
{
    auto ret = 0;
    ExtOptions tmpOptions = options;
    std::unordered_map<uint32_t, CopyDescriptor> localed{};
    std::unordered_map<uint32_t, CopyDescriptor> registered{};
    std::unordered_map<uint32_t, CopyDescriptor> notRegistered{};
    ClassifyDataAddr(params.destinations, params.sources, params.dataSizes, params.batchSize, registered, localed,
                     notRegistered, options.destRankId);

    // 先写异步（batch）
    std::set<uint32_t> asyncSubmittedRanks{};
    for (auto &it : registered) {
        tmpOptions.destRankId = it.first;

        ret = transportManager_->WriteRemoteBatchAsync(it.first, it.second);
        if (ret != BM_OK) {
            for (uint32_t r : asyncSubmittedRanks) {
                (void)transportManager_->Synchronize(r);
            }
            BM_LOG_ERROR("Failed to write src to dest");
            return ret;
        }
        asyncSubmittedRanks.insert(it.first);
    }
    // 再写本地
    for (auto &it : localed) {
        hybm_batch_copy_params localParams = {it.second.localAddrs.data(), it.second.globalAddrs.data(),
                                              it.second.counts.data(), static_cast<uint32_t>(it.second.counts.size())};
        tmpOptions.destRankId = it.first;
        TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_LOCAL);
        ret = BatchDataCopyLocal(localParams, direction, tmpOptions);
        TP_TRACE_END(TP_HYBM_URMA_BATCH_LOCAL, ret);
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
        TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_WAIT_W);
        ret = transportManager_->Synchronize(it.first);
        TP_TRACE_END(TP_HYBM_URMA_BATCH_WAIT_W, ret);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to Synchronize", ret);
    }
    return BM_OK;
}

Result DataOpDeviceURMA::BatchCopyRead(hybm_batch_copy_params &params, const ExtOptions &options,
                                       hybm_data_copy_direction direction) noexcept
{
    auto ret = 0;
    ExtOptions tmpOptions = options;
    std::unordered_map<uint32_t, CopyDescriptor> localed{};
    std::unordered_map<uint32_t, CopyDescriptor> registered{};
    std::unordered_map<uint32_t, CopyDescriptor> notRegistered{};
    ClassifyDataAddr(params.sources, params.destinations, params.dataSizes, params.batchSize, registered, localed,
                     notRegistered, options.srcRankId);

    // 先读异步（batch）
    std::set<uint32_t> asyncSubmittedRanks{};
    for (auto &it : registered) {
        tmpOptions.srcRankId = it.first;

        ret = transportManager_->ReadRemoteBatchAsync(it.first, it.second);
        if (ret != BM_OK) {
            for (uint32_t r : asyncSubmittedRanks) {
                (void)transportManager_->Synchronize(r);
            }
            BM_LOG_ERROR("Failed to read src to dest");
            return ret;
        }
        asyncSubmittedRanks.insert(it.first);
    }
    // 再写本地
    for (auto &it : localed) {
        hybm_batch_copy_params localParams = {it.second.globalAddrs.data(), it.second.localAddrs.data(),
                                              it.second.counts.data(), static_cast<uint32_t>(it.second.counts.size())};
        tmpOptions.destRankId = it.first;
        TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_LOCAL);
        ret = BatchDataCopyLocal(localParams, direction, tmpOptions);
        TP_TRACE_END(TP_HYBM_URMA_BATCH_LOCAL, ret);
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
        TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_WAIT_R);
        ret = transportManager_->Synchronize(it.first);
        TP_TRACE_END(TP_HYBM_URMA_BATCH_WAIT_R, ret);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to Synchronize", ret);
    }
    return BM_OK;
}

Result DataOpDeviceURMA::BatchCopyLD2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyWrite(params, options, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE);
}

Result DataOpDeviceURMA::BatchCopyLD2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyWrite(params, options, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST);
}

Result DataOpDeviceURMA::BatchCopyGH2LD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyRead(params, options, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE);
}

Result DataOpDeviceURMA::BatchCopyGD2LD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyRead(params, options, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE);
}

Result DataOpDeviceURMA::BatchCopyLH2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyWrite(params, options, HYBM_LOCAL_HOST_TO_GLOBAL_HOST);
}

Result DataOpDeviceURMA::BatchCopyG2G(hybm_batch_copy_params &params, const ExtOptions &options,
                                      hybm_data_copy_direction direction) noexcept
{
    const auto srcRankId = options.srcRankId;
    const auto dstRankId = options.destRankId;
    const auto batchSize = params.batchSize;

    // Separate local items from remote items, grouping all remote items into one batch.
    CopyDescriptor remoteDesc;

    for (uint32_t i = 0; i < batchSize; i++) {
        if (srcRankId == rankId_ && dstRankId == rankId_) {
            hybm_copy_params pm = {params.sources[i], params.destinations[i], params.dataSizes[i]};
            const auto ret = DataCopy(pm, direction, options);
            BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "write default failed:", ret);
        } else if (srcRankId == rankId_) {
            // write: local=src, global=dst
            remoteDesc.localAddrs.push_back(params.sources[i]);
            remoteDesc.globalAddrs.push_back(params.destinations[i]);
            remoteDesc.counts.push_back(params.dataSizes[i]);
        } else if (dstRankId == rankId_) {
            // read: local=dst, global=src
            remoteDesc.localAddrs.push_back(params.destinations[i]);
            remoteDesc.globalAddrs.push_back(params.sources[i]);
            remoteDesc.counts.push_back(params.dataSizes[i]);
        } else {
            BM_LOG_ERROR("invalid param, local rank:" << rankId_ << ", srcId: " << srcRankId
                                                      << ", dstId: " << dstRankId);
            return BM_ERROR;
        }
    }

    // Issue a single batch async call for all remote items
    if (!remoteDesc.counts.empty()) {
        const uint32_t remoteRank = (srcRankId == rankId_) ? dstRankId : srcRankId;

        Result ret = BM_OK;
        if (srcRankId == rankId_) {
            ret = transportManager_->WriteRemoteBatchAsync(remoteRank, remoteDesc);
        } else {
            ret = transportManager_->ReadRemoteBatchAsync(remoteRank, remoteDesc);
        }
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to batch write/read src to dest");
            return ret;
        }
        TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_WAIT_W);
        ret = transportManager_->Synchronize(remoteRank);
        TP_TRACE_END(TP_HYBM_URMA_BATCH_WAIT_W, ret);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to Synchronize", ret);
    }

    return BM_OK;
}

Result DataOpDeviceURMA::SafePut(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options,
                                 bool srcIsHost)
{
    Result ret = 0;
    uintptr_t srcBase = reinterpret_cast<uintptr_t>(srcVA);
    uintptr_t destBase = reinterpret_cast<uintptr_t>(destVA);
    uint64_t remainingLength = length;
    uint64_t offset = 0;
    if (transportManager_->QueryHasRegistered(srcBase, length)) {
        ret = CopyURMA(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy urma", ret);
        return ret;
    }
    while (remainingLength > 0) {
        uint64_t currentChunkSize = std::min(remainingLength, URMA_SWAP_SPACE_SIZE);
        auto tmpRdmaMemory = urmaSwapMemoryAllocator_->Allocate(currentChunkSize);
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
        ret = CopyURMA(tmpHost, currentDest, currentChunkSize, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy tmp to dest", ret);
        offset += currentChunkSize;
        remainingLength -= currentChunkSize;
    }
    return 0;
}

Result DataOpDeviceURMA::SafeGet(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options,
                                 bool destIsHost)
{
    Result ret = 0;
    uintptr_t srcBase = reinterpret_cast<uintptr_t>(srcVA);
    uintptr_t destBase = reinterpret_cast<uintptr_t>(destVA);
    uint64_t remainingLength = length;
    uint64_t offset = 0;
    if (transportManager_->QueryHasRegistered(destBase, length)) {
        ret = CopyURMA(srcVA, destVA, length, options);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to copy urma", ret);
        return ret;
    }
    while (remainingLength > 0) {
        uint64_t currentChunkSize = std::min(remainingLength, URMA_SWAP_SPACE_SIZE);
        auto tmpRdmaMemory = urmaSwapMemoryAllocator_->Allocate(currentChunkSize);
        auto tmpHost = tmpRdmaMemory.Address();
        BM_ASSERT_LOG_AND_RETURN(tmpHost != nullptr, "[CopyGD2LH] Failed to malloc temp buffer", BM_MALLOC_FAILED);
        const void *currentSrc = reinterpret_cast<const void *>(srcBase + offset);
        void *currentDest = reinterpret_cast<void *>(destBase + offset);
        ret = CopyURMA(currentSrc, tmpHost, currentChunkSize, options);
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

Result DataOpDeviceURMA::BatchCopyGH2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyG2G(params, options, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST);
}

Result DataOpDeviceURMA::BatchCopyGH2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyG2G(params, options, HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE);
}

Result DataOpDeviceURMA::BatchCopyGH2LH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyRead(params, options, HYBM_GLOBAL_HOST_TO_LOCAL_HOST);
}

Result DataOpDeviceURMA::BatchCopyGD2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyG2G(params, options, HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST);
}

Result DataOpDeviceURMA::BatchCopyGD2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept
{
    return BatchCopyG2G(params, options, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE);
}

Result DataOpDeviceURMA::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                       const ExtOptions &options) noexcept
{
    auto ret = 0;
    {
        const bool srcLocal = (options.srcRankId == rankId_);
        const bool dstLocal = (options.destRankId == rankId_);
        for (uint32_t i = 0; i < params.batchSize; i++) {
            if (srcLocal && dstLocal) {
                TransformVa(params.sources[i], params.destinations[i], direction);
            } else if (srcLocal) {
                void *tmpDst = nullptr;
                TransformVa(params.sources[i], tmpDst, direction);
            } else if (dstLocal) {
                void *tmpSrc = nullptr;
                TransformVa(tmpSrc, params.destinations[i], direction);
            }
            // neither local: both addresses remain as GVA → skip TransformVa entirely.
        }
    }
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: { // 0
            TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_LH_TO_GH);
            ret = BatchCopyLH2GH(params, options);
            TP_TRACE_END(TP_HYBM_URMA_BATCH_LH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST: { // 4
            TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_GH_TO_GH);
            ret = BatchCopyGH2GH(params, options);
            TP_TRACE_END(TP_HYBM_URMA_BATCH_GH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE: { // 5
            TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_GH_TO_GD);
            ret = BatchCopyGH2GD(params, options);
            TP_TRACE_END(TP_HYBM_URMA_BATCH_GH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST: { // 6
            TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_GH_TO_LH);
            ret = BatchCopyGH2LH(params, options);
            TP_TRACE_END(TP_HYBM_URMA_BATCH_GH_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST: { // 8
            TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_GD_TO_GH);
            ret = BatchCopyGD2GH(params, options);
            TP_TRACE_END(TP_HYBM_URMA_BATCH_GD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE: { // 9
            TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_GD_TO_GD);
            ret = BatchCopyGD2GD(params, options);
            TP_TRACE_END(TP_HYBM_URMA_BATCH_GD_TO_GD, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE: { // 1
            TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_LH_TO_GD);
            ret = BatchCopyLH2GD(params, options);
            TP_TRACE_END(TP_HYBM_URMA_BATCH_LH_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST: { // 10
            TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_GD_TO_LH);
            ret = BatchCopyGD2LH(params, options);
            TP_TRACE_END(TP_HYBM_URMA_BATCH_GD_TO_LH, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: { // 2
            TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_LD_TO_GH);
            ret = BatchCopyLD2GH(params, options);
            TP_TRACE_END(TP_HYBM_URMA_BATCH_LD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: { // 7
            TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_GH_TO_LD);
            ret = BatchCopyGH2LD(params, options);
            TP_TRACE_END(TP_HYBM_URMA_BATCH_GH_TO_LD, ret);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE: { // 3
            TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_LD_TO_GD);
            ret = BatchCopyLD2GD(params, options);
            TP_TRACE_END(TP_HYBM_URMA_BATCH_LD_TO_GD, ret);
            break;
        }
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE: { // 11
            TP_TRACE_BEGIN(TP_HYBM_URMA_BATCH_GD_TO_LD);
            ret = BatchCopyGD2LD(params, options);
            TP_TRACE_END(TP_HYBM_URMA_BATCH_GD_TO_LD, ret);
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
