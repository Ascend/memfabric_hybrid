/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#include "device_sdma_transport_manager.h"

#include <unistd.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <numeric>

#include "hybm_common_include.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "dl_rt_api.h"
#include "dl_op_api.h"
#include "hybm_ptracer.h"
#include "device_rdma_common.h"
#include "device_rdma_helper.h"
#include "fixed_ranks_qp_manager.h"
#include "bipartite_ranks_qp_manager.h"
#include "joinable_ranks_qp_manager.h"
#include "hybm_gva.h"
#include "hybm_va_manager.h"
#include "mf_scope_guard.h"

namespace ock {
namespace mf {

namespace transport {
namespace device {

SdmaOpResInfo SdmaTransportManager::opResInfo_ = {};
void *SdmaTransportManager::opResInfoDevicePtr_ = nullptr;
std::vector<HostStreamInfo> SdmaTransportManager::streams_;

Result SdmaTransportManager::OpenDevice(const TransportOptions &options)
{
    BM_LOG_DEBUG(rankId_ << " start to init sdma with " << options);

    std::unique_lock<std::mutex> unique_lock(mutex_);
    rankId_ = options.rankId;
    rankCount_ = options.rankCount;

    // 创建host测stream（910B/910_93按照最大AIV核数申请）
    auto ret = CreateStarsStreams(MAX_AIV_PER_NPU);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "create stars stream failed, ret=" << ret, ret);

    // 申请AICPU和AIV的共享内存
    constexpr size_t workspaceSize = 16 * 1024; // 16KB
    ret = MallocSdmaWorkspace(workspaceSize);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "malloc sdma workspace failed, ret=" << ret, ret);

    // 创建Notify并保存id到共享内存
    ret = CreateNotifyIds();
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "create notify ids failed, ret=" << ret, ret);

    // 申请的资源H2D
    ret = CopyHostOpResToDevice();
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "copy host op res to device failed, ret=" << ret, ret);

    // 调用内置aicpu算子
    ret = LaunchSdmaAicpuKernel(reinterpret_cast<uint64_t>(opResInfoDevicePtr_), opResInfo_.workspaceAddr);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "launch sdma aicpu kernel failed, ret=" << ret, ret);

    BM_LOG_DEBUG(rankId_ << " init sdma success.");
    inited_ = true;
    return BM_OK;
}

Result SdmaTransportManager::CreateNotifyIds()
{
    uint32_t notifyIds[MAX_AIV_PER_NPU] = {0};

    // 获取NotifyId存储区的起始地址
    uint32_t *notifyIdBase =
        reinterpret_cast<uint32_t *>(reinterpret_cast<uint8_t *>(sdmaWorkspaceAddr_) + STARS_NOTIFY_ADDR_OFFSET);
    int ret = 0;
    for (size_t i = 0; i < MAX_AIV_PER_NPU; i++) {
        notifyArr_[i] = nullptr;
        ret = DlAclApi::AclrtCreateNotify(&notifyArr_[i], 0);
        BM_ASSERT_LOG_AND_RETURN(ret == 0, "create notify failed, ret=" << ret, ret);
        ret = DlAclApi::AclrtGetNotifyId(notifyArr_[i], &notifyIds[i]);
        BM_ASSERT_LOG_AND_RETURN(ret == 0, "get notify id failed, ret=" << ret, ret);
    }
    ret = DlAclApi::AclrtMemcpy(notifyIdBase, MAX_AIV_PER_NPU * sizeof(uint32_t), notifyIds,
                                MAX_AIV_PER_NPU * sizeof(uint32_t), ACL_MEMCPY_HOST_TO_DEVICE);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "memcpy notify id failed, ret=" << ret, ret);

    return BM_OK;
}

Result SdmaTransportManager::CreateStarsStreams(int32_t channel_num)
{
    int32_t userId = -1;
    auto ret = DlAclApi::AclrtGetDevice(&userId);
    BM_ASSERT_LOG_AND_RETURN(ret == 0 && userId >= 0,
                             "AclrtGetDevice() return=" << ret << ", output deviceId=" << userId,
                             BM_DL_FUNCTION_FAILED);

    int64_t dieId = -1;
    constexpr int infoTypePhyDieId = 19;
    ret = DlAclApi::RtGetDeviceInfo(userId, 0, infoTypePhyDieId, &dieId);

    deviceId_ = static_cast<uint32_t>(dieId);

    streams_.resize(channel_num);
    for (int32_t i = 0; i < channel_num; i++) {
        streams_[i].stream = 0;

        void *stream = nullptr;
        constexpr uint32_t configValue = 0x00000020U;
        ret = DlAclApi::AclrtCreateStreamWithConfig(&stream, 0, configValue);
        BM_ASSERT_LOG_AND_RETURN(ret == 0, "create stream " << i << " failed, ret=" << ret, ret);
        streams_[i].stream = reinterpret_cast<uint64_t>(stream);

        int32_t streamId = 0;
        ret = DlAclApi::AclrtStreamGetId(stream, &streamId);
        if (ret != 0) {
            BM_LOG_ERROR(rankId_ << " get stream id " << i << " failed");
            DlAclApi::AclrtDestroyStream(stream);
            return BM_ERROR;
        }
        uint32_t sqId = 0;
        ret = DlRtApi::RtStreamGetSqid(stream, &sqId);
        if (ret != 0) {
            BM_LOG_ERROR(rankId_ << " get sqid " << i << " failed");
            DlAclApi::AclrtDestroyStream(stream);
            return BM_ERROR;
        }
        uint32_t cqId = 0;
        uint32_t logicCqId = 0;
        ret = DlRtApi::RtStreamGetCqid(stream, &cqId, &logicCqId);
        if (ret != 0) {
            BM_LOG_ERROR(rankId_ << " get cqid " << i << " failed");
            DlAclApi::AclrtDestroyStream(stream);
            return BM_ERROR;
        }
        void *ctx = nullptr;
        ret = DlAclApi::AclrtGetCurrentContext(&ctx);
        if (ret != 0) {
            BM_LOG_ERROR(rankId_ << " get context " << i << " failed");
            DlAclApi::AclrtDestroyStream(stream);
            return BM_ERROR;
        }
        streams_[i].ctx = reinterpret_cast<uint64_t>(ctx);
        streams_[i].streamId = streamId;
        streams_[i].sqId = sqId;
        streams_[i].cqId = cqId;
        streams_[i].logicCqId = logicCqId;
        streams_[i].devId = dieId;
        BM_LOG_DEBUG(rankId_ << "create stream " << i << "," << streams_[i].stream << "," << streams_[i].devId << ","
                             << streamId << "," << sqId << "," << cqId << "," << logicCqId << "," << streams_[i].ctx);
    }
    BM_LOG_INFO(rankId_ << " create " << channel_num << " stars streams success.");
    return BM_OK;
}

Result SdmaTransportManager::MallocSdmaWorkspace(size_t workspaceSize)
{
    void *sdmaWorkspace;
    auto ret = DlAclApi::AclrtMalloc(&sdmaWorkspace, workspaceSize, 0);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "malloc sdma workspace failed, ret=" << ret, ret);
    ret = DlAclApi::AclrtMemset(sdmaWorkspace, workspaceSize, 0, workspaceSize);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "memset sdma workspace failed, ret=" << ret, ret);
    sdmaWorkspaceAddr_ = reinterpret_cast<uint64_t>(sdmaWorkspace);
    BM_LOG_INFO(rankId_ << " malloc sdmaWorkspace success.");
    return BM_OK;
}

uint64_t SdmaTransportManager::GetSdmaWorkSpaceAddr() const
{
    return sdmaWorkspaceAddr_;
}

Result SdmaTransportManager::CopyHostOpResToDevice()
{
    void *streamsDevicePtr = nullptr;
    size_t streamsSize = streams_.size() * sizeof(HostStreamInfo);
    auto ret = DlAclApi::AclrtMalloc(&streamsDevicePtr, streamsSize, 0);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "malloc streams device ptr failed, ret=" << ret, ret);
    ret = DlAclApi::AclrtMemcpy(streamsDevicePtr, streamsSize, streams_.data(), streamsSize, ACL_MEMCPY_HOST_TO_DEVICE);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "memcpy streams to device failed, ret=" << ret, ret);

    opResInfo_.size = streams_.size();
    opResInfo_.streamsAddr = reinterpret_cast<uint64_t>(streamsDevicePtr);
    opResInfo_.workspaceAddr = sdmaWorkspaceAddr_;

    size_t opResSize = sizeof(opResInfo_);
    ret = DlAclApi::AclrtMalloc(&opResInfoDevicePtr_, opResSize, 0);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "malloc op res device ptr failed, ret=" << ret, ret);
    ret = DlAclApi::AclrtMemset(opResInfoDevicePtr_, opResSize, 0, opResSize);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "memset op res device ptr failed, ret=" << ret, ret);
    ret = DlAclApi::AclrtMemcpy(opResInfoDevicePtr_, opResSize, &opResInfo_, opResSize, ACL_MEMCPY_HOST_TO_DEVICE);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "memcpy op res to device failed, ret=" << ret, ret);

    BM_LOG_INFO(rankId_ << " copy op res to device success.");
    return BM_OK;
}

Result SdmaTransportManager::CreateAclTensor(const std::vector<uint64_t> &host_data, const std::vector<int64_t> &shape,
                                             void **device_addr, aclDataType data_type, aclTensor **tensor)
{
    *tensor = nullptr;
    *device_addr = nullptr;
    auto size = std::accumulate(shape.begin(), shape.end(), static_cast<int64_t>(1),
                                [](int64_t a, int64_t b) { return a * b; }) *
                sizeof(uint64_t);
    auto ret = DlAclApi::AclrtMalloc(device_addr, size, 0);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "malloc device memory failed, ret=" << ret, ret);
    auto dev_guard = make_scope_guard(*device_addr, [](void *addr) { DlAclApi::AclrtFree(addr); }); // 托管device_addr
    ret = DlAclApi::AclrtMemcpy(*device_addr, size, host_data.data(), size, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR(rankId_ << " memcpy host data to device failed, ret=" << ret);
        *device_addr = nullptr;
        return ret;
    }

    std::vector<int64_t> strides(shape.size(), 1);
    for (int64_t i = shape.size() - 2; i >= 0; i--) {
        strides[i] = shape[i + 1] * strides[i + 1];
    }

    *tensor = DlOpApi::AclCreateTensor(shape.data(), shape.size(), data_type, strides.data(), 0,
                                       aclFormat::ACL_FORMAT_ND, shape.data(), shape.size(), *device_addr);
    if (*tensor == nullptr) {
        BM_LOG_ERROR(rankId_ << " create acl tensor failed");
        *device_addr = nullptr;
        return BM_ERROR;
    }
    auto tensor_guard = make_scope_guard(*tensor, [](aclTensor *t) { DlOpApi::AclDestroyTensor(t); });

    // 放弃所有权，交给外部接管
    dev_guard.release();
    tensor_guard.release();
    return BM_OK;
}

Result SdmaTransportManager::LaunchSdmaAicpuKernel(uint64_t streams_addr, uint64_t sdmaWorkspaceAddr)
{
    void *aicpuStream = nullptr;
    static uint32_t ACL_STREAM_FAST_LAUNCH = 1U;
    static uint32_t ACL_STREAM_FAST_SYNC = 2U;
    auto ret = DlAclApi::AclrtCreateStreamWithConfig(&aicpuStream, 0, ACL_STREAM_FAST_LAUNCH | ACL_STREAM_FAST_SYNC);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "create aicpu stream failed, ret=" << ret, ret);
    auto stream_guard = make_scope_guard(aicpuStream, [](void *stm) { DlAclApi::AclrtDestroyStream(stm); });
    aclrtStreamAttrValue value;
    value.failureMode = 1; // 遇错即停
    ret = DlAclApi::AclrtSetStreamAttribute(aicpuStream, ACL_STREAM_ATTR_FAILURE_MODE, &value);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "set aicpu stream attribute failed, ret=" << ret, ret);

    // 构造输入输出
    std::vector<int64_t> in_shape = {2};
    std::vector<int64_t> out_shape = {1};
    std::vector<uint64_t> in_host_data = {streams_addr, sdmaWorkspaceAddr};
    std::vector<uint64_t> out_host_data = {0};

    // 创建输入aclTensor
    void *inDeviceAddr = nullptr;
    aclTensor *input = nullptr;
    ret = CreateAclTensor(in_host_data, in_shape, &inDeviceAddr, aclDataType::ACL_UINT64, &input);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "create input acl tensor failed, ret=" << ret, ret);
    auto input_dev_guard = make_scope_guard(inDeviceAddr, [](void *addr) { DlAclApi::AclrtFree(addr); });
    auto input_tensor_guard = make_scope_guard(input, [](aclTensor *t) { DlOpApi::AclDestroyTensor(t); });

    // 创建输出aclTensor
    void *outDeviceAddr = nullptr;
    aclTensor *output = nullptr;
    ret = CreateAclTensor(out_host_data, out_shape, &outDeviceAddr, aclDataType::ACL_UINT64, &output);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "create output acl tensor failed, ret=" << ret, ret);
    auto output_dev_guard = make_scope_guard(outDeviceAddr, [](void *addr) { DlAclApi::AclrtFree(addr); });
    auto output_tensor_guard = make_scope_guard(output, [](aclTensor *t) { DlOpApi::AclDestroyTensor(t); });

    // 调用aclnn二段式接口
    uint64_t workspaceSize = 0;
    aclOpExecutor *executor = nullptr;
    ret = DlOpApi::AclnnShmemSdmaStarsQueryGetWorkspaceSize(input, output, &workspaceSize, &executor);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "query workspace size failed, ret=" << ret, ret);

    void *workspace = nullptr;
    auto workspace_guard = make_scope_guard(static_cast<void *>(nullptr), [](void *) {});
    if (workspaceSize > 0) {
        ret = DlAclApi::AclrtMalloc(&workspace, workspaceSize, 0);
        BM_ASSERT_LOG_AND_RETURN(ret == 0, "malloc workspace failed, ret=" << ret, ret);
        workspace_guard = make_scope_guard(workspace, [](void *addr) { DlAclApi::AclrtFree(addr); });
    }
    ret = DlOpApi::AclnnShmemSdmaStarsQuery(workspace, workspaceSize, executor, aicpuStream);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "query failed, ret=" << ret, ret);
    ret = DlAclApi::AclrtSynchronizeStream(aicpuStream);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "synchronize stream failed, ret=" << ret, ret);
    return BM_OK;
}

Result SdmaTransportManager::CloseDevice()
{
    if (!inited_) {
        BM_LOG_WARN(rankId_ << " sdma not initialized");
        return BM_OK;
    }

    if (opResInfo_.streamsAddr) {
        (void)DlAclApi::AclrtFree(reinterpret_cast<void *>(opResInfo_.streamsAddr));
        opResInfo_.streamsAddr = 0;
    }

    if (opResInfoDevicePtr_) {
        (void)DlAclApi::AclrtFree(opResInfoDevicePtr_);
        opResInfoDevicePtr_ = nullptr;
    }

    if (opResInfo_.workspaceAddr) {
        (void)DlAclApi::AclrtFree(reinterpret_cast<void *>(opResInfo_.workspaceAddr));
    }

    for (auto &stream : streams_) {
        if (stream.stream) {
            (void)DlAclApi::AclrtDestroyStream(reinterpret_cast<void *>(stream.stream));
        }
    }

    for (size_t i = 0; i < MAX_AIV_PER_NPU; i++) {
        if (notifyArr_[i]) {
            (void)DlAclApi::AclrtDestroyNotify(notifyArr_[i]);
        }
    }

    streams_.clear();
    opResInfo_ = {};
    inited_ = false;
    BM_LOG_INFO(rankId_ << " sdma transport finalize.");
    return BM_OK;
}
} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock
