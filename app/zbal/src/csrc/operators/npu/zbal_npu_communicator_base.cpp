/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <ctime>
#include "zbal_npu_communicator_base.h"
#include "zbal_comm_host_device_struct.h"
#include "zbal_bootstrap_default.h"
#include "zbal_trace_viewer_dumper.h"
#include "zbal_npu_operators.h"
#include "dl_cann_api.h"
#include "acl/acl.h"

namespace zbal {
namespace operators {

using namespace underapi;

#define TRACE_GET_FRAME_ID(record)  (((record) & 0x3F00000000000000ULL) >> ZBAL_PROFILING_FRAME_SHIFT)
#define TRACE_GET_TIMESTAMP(record) (((record) & ~(0xFFULL << ZBAL_PROFILING_FRAME_SHIFT)) / ZBAL_CYCLE_UNIT)
#define TRACE_GET_BE(record)        (((record) & 0x4000000000000000ULL) == 0)

uint64_t NpuCommunicatorBase::opRunTimes_ = 0;

NpuCommunicatorBase::NpuCommunicatorBase(const CommGroupOptions &options, bool isWorldGroup,
                                         const CommunicatorPtr &worldGroup)
    : Communicator(options, isWorldGroup, worldGroup)
{}

ZResult NpuCommunicatorBase::Initialize() noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (initialized_) {
        return Z_OK;
    }

    /* make sure ConstructCommGroupInfo is called before this */
    (void)DlCannApi::AclrtMemset(reinterpret_cast<void *>(groupInfo_.myAddressExchangeGva),
                                 groupInfo_.sizeForExchangeAddress, 0, groupInfo_.sizeForExchangeAddress);

    /* get ffts address */
    uint32_t len = 0;
    auto result = DlCannApi::RtGetC2cCtrlAddr(&groupInfo_.fftsConfig, &len);
    if (result != Z_OK) {
        ZBAL_LOG_ERROR("get c2c ctrl addr failed, result: " << result);
        return Z_FFTS_INIT_FAILED;
    }

    result = SetupProfMemory();
    if (result != Z_OK) {
        ZBAL_LOG_ERROR("Setup host memory for perf failed");
        return result;
    }

    /* do control path barrier to ensure all ranks finished the memset */
    auto bootstrap = bootstrap::Bootstrap::Get();
    ZBAL_ASSERT_RETURN(bootstrap != nullptr, Z_NOT_BOOTSTRAPPED);

    result = bootstrap->SubGroupBarrier(options_.name, options_.groupSize, options_.myGroupRank);
    if (result != Z_OK) {
        ZBAL_LOG_ERROR("Subgroup barrier failed at control path, result: " << result);
        return Z_FFTS_INIT_FAILED;
    }

    initialized_ = true;

    return Z_OK;
}

void NpuCommunicatorBase::UnInitialize() noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return;
    }

    DumpProfilingTrace();
    DestroyProfMemory();

    initialized_ = false;
}

void NpuCommunicatorBase::ConstructCommGroupInfo(const CommGroupOptions &options) noexcept
{
    groupInfo_.groupIndex = options.groupIndex;
    groupInfo_.groupSize = options.groupSize;
    groupInfo_.myGroupRank = options.myGroupRank;
    groupInfo_.myMetaGva = options.myMetaGva;
    groupInfo_.myParamDataGva = options.myParamDataGva;
    groupInfo_.myAddressExchangeGva = options.myAddressExchangeGva;
    groupInfo_.sizeForCommGroupInfo = options.sizeForCommGroupInfo;
    groupInfo_.sizeForParam = options.sizeForParam;
    groupInfo_.sizeForExchangeAddress = options.sizeForExchangeAddress;
    groupInfo_.fftsConfig = options.fftsConfig;
    groupInfo_.localDeviceMemSize = options.localDeviceMemSize;
    groupInfo_.dataOpType = options.dataOpType;

    uint64_t curGroupSymbolIndex = options.groupIndex + 1;
    ZBAL_ASSERT(curGroupSymbolIndex <= COMM_GROUP_COUNT_CAP_MAX);
    groupInfo_.waitSymbol = (curGroupSymbolIndex << COMM_GROUP_SYMBOL_SHIFT);
}

ZResult NpuCommunicatorBase::AssignGatherGroupId(AutoReleaseGroupId &id) noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        ZBAL_LOG_ERROR("Assign group id failed, as communicator is not initialized");
        return Z_NOT_INITIALIZED;
    }

    uniqueGroupId_.MoveIdAndGatheredInfo(id);

    /* assign peer info from group id class */
    auto &gatheredGroupInfo = uniqueGroupId_.GatheredGroupInfo();
    ZBAL_ASSERT_RETURN(gatheredGroupInfo.size() == groupInfo_.groupSize, Z_ERROR);
    ZBAL_ASSERT_RETURN(gatheredGroupInfo.size() <= ZBAL_MAX_RANKS, Z_ERROR);

    for (uint64_t i = 0; i < gatheredGroupInfo.size(); ++i) {
        groupInfo_.peerGroupRank2WorldRank[i] = gatheredGroupInfo[i].myWorldRankId;
    }

    /* copy group info to meta area of communicator from host to device */
    ZBAL_ASSERT_RETURN(sizeof(CommGroupInfo) == groupInfo_.sizeForCommGroupInfo, Z_ERROR);
    auto result = DlCannApi::AclrtMemcpy(reinterpret_cast<void *>(groupInfo_.myMetaGva), sizeof(CommGroupInfo),
                                         &groupInfo_, sizeof(CommGroupInfo), ACL_MEMCPY_HOST_TO_DEVICE);
    if (result != Z_OK) {
        ZBAL_LOG_ERROR("CommGroupInfo h2d copy failed, result: " << result);
        return Z_COMM_GROUP_H2D_FAILED;
    }

    ZBAL_LOG_DEBUG("Dump groupId_ " << uniqueGroupId_ << ", groupInfo_: " << groupInfo_);
    return Z_OK;
}

ZResult NpuCommunicatorBase::SetupProfMemory()
{
    /* check if perf is enabled or not */
    if (!EnvHelper::PROF_ENABLED) {
        groupInfo_.devMemoryForProfiling = 0;
        ZBAL_LOG_DEBUG("Perf tracing is not enabled");
        return Z_OK;
    }

    /* check and set tracing max count */
    uint64_t traceCap = static_cast<uint64_t>(EnvHelper::PROF_TRACING_MAX_COUNT);
    constexpr uint64_t PROF_TRACING_COUNT_DEFAULT = 20480;
    if (traceCap < PROF_TRACING_COUNT_DEFAULT || traceCap >= 10 * PROF_TRACING_COUNT_DEFAULT) {
        traceCap = PROF_TRACING_COUNT_DEFAULT;
        EnvHelper::PROF_TRACING_MAX_COUNT = PROF_TRACING_COUNT_DEFAULT;
        ZBAL_LOG_INFO("Perf tracing max count " << EnvHelper::PROF_TRACING_MAX_COUNT << " is not valid, set to "
                                                << PROF_TRACING_COUNT_DEFAULT);
    }

    /* calculate memory size required */
    uint64_t traceMemorySize = traceCap * ZBAL_MAX_AIV_SIZE_PER_NPU * sizeof(uint64_t);
    ZBAL_LOG_DEBUG("Perf tracing max count " << traceCap << ", memory consumption " << traceMemorySize << " bytes");

    /* allocate host memory */
    void *hostMemory = nullptr;
    auto result = DlCannApi::AclrtMallocHost(&hostMemory, traceMemorySize);
    if (result != Z_OK) {
        ZBAL_LOG_ERROR("Failed to allocate host memory for perf, size " << traceMemorySize << " bytes");
        return result;
    }

    result = DlCannApi::AclrtMemset(hostMemory, traceMemorySize, 0, traceMemorySize);
    if (result != Z_OK) {
        ZBAL_LOG_ERROR("Failed to reset perf memory to 0, ret=" << result);
        DlCannApi::AclrtFreeHost(hostMemory);
        return result;
    }

    /* register host memory to device */
    void *devMappedPtr = nullptr;
    result = DlCannApi::AclrtHostRegister(hostMemory, traceMemorySize, &devMappedPtr);
    if (result != Z_OK) {
        ZBAL_LOG_ERROR("Failed to register host memory to device, result: " << result);
        DlCannApi::AclrtFreeHost(hostMemory);
        return result;
    }

    ZBAL_LOG_DEBUG("Allocate and register perf memory successfully, hostPtr: " << std::hex << hostMemory << ", devPtr: "
                                                                               << std::hex << devMappedPtr);
    perfHostMemory_ = hostMemory;
    groupInfo_.hostMemoryForProfiling = reinterpret_cast<uintptr_t>(hostMemory);
    groupInfo_.devMemoryForProfiling = reinterpret_cast<uintptr_t>(devMappedPtr);
    groupInfo_.tracePointPerCore = traceCap;
    return Z_OK;
}

void NpuCommunicatorBase::DestroyProfMemory()
{
    if (perfHostMemory_ == nullptr) {
        return;
    }

    auto result = DlCannApi::AclrtHostUnRegister(perfHostMemory_);
    if (result != Z_OK) {
        ZBAL_LOG_DEBUG("UnRegister host memory failed, result: " << result);
    }

    perfHostMemory_ = nullptr;
}

void NpuCommunicatorBase::DumpProfilingTrace() noexcept
{
    const CommGroupInfo &meta = GetMetaInfo();
    if (meta.hostMemoryForProfiling == 0) {
        return;
    }

    const uint32_t rank = meta.peerGroupRank2WorldRank[meta.myGroupRank];
    std::ostringstream oss;
    oss << "trace_view_" << rank << "_" << meta.groupIndex << "_" << Func::GetCurrentDateTime() << ".json";
    const std::string pid = "RANK_" + std::to_string(rank);
    TraceViewerFormatFileWriter writer(EnvHelper::PROF_DIR, oss.str(), pid);
    if (writer.Open() != Z_OK) {
        ZBAL_LOG_ERROR("rank " << rank << " print profiling, open dump file failed.");
        return;
    }

    uint64_t *hostPerf = reinterpret_cast<uint64_t *>(meta.hostMemoryForProfiling);
    for (uint64_t i = 0; i < ZBAL_MAX_AIV_SIZE_PER_NPU; i++) {
        uint64_t *coreBlock = hostPerf + i * meta.tracePointPerCore;
        std::string tid = "aiv " + std::to_string(i);

        for (uint64_t j = ZBAL_PROFILING_DEVICE_TRACE_OFF; j < meta.tracePointPerCore; j++) {
            uint64_t record = coreBlock[j];
            bool begin = TRACE_GET_BE(record);
            uint64_t ts = TRACE_GET_TIMESTAMP(record);
            uint64_t frameId = TRACE_GET_FRAME_ID(record);
            if (frameId == 0) { // skip tailing empty trace
                break;
            }
            if (frameId >= g_profName.size()) {
                ZBAL_LOG_WARN("rank " << rank << " core " << i << " has unknown record:" << record);
                break;
            }

            std::string name = g_profName[frameId].first;
            TraceElement trace(name.c_str(), pid.c_str(), tid.c_str(), begin ? TEP_BEGIN : TEP_END, ts);

            if (frameId == ZBAL_PROF_LINENO && begin) { // read next at most 6 as attr
                while (true) {
                    record = coreBlock[++j];
                    frameId = TRACE_GET_FRAME_ID(record);
                    if (frameId == ZBAL_PROF_LINENO) {
                        --j;
                        break;
                    } else {
                        trace.Append(record);
                    }
                }
            }

            writer.Append(trace);
        }
    }
    writer.Close();
    ZBAL_LOG_WARN("dump trace " << meta.groupIndex << " on rank " << rank << " finished.");
}

void NpuCommunicatorBase::SignalDumpTrace() noexcept
{
    DumpProfilingTrace();
}

int32_t NpuCommunicatorBase::AllReduce(const void *send_buff, void *recv_buff, void *buffer, size_t count,
                                       size_t buf_cnt, zbal_datatype_t data_type, zbal_reduce_op_t op,
                                       aclrtStream stream) noexcept
{
    return ZBALOpAllReduce(send_buff, recv_buff, buffer, count, buf_cnt, data_type, stream, op,
                           const_cast<CommGroupInfo &>(GetMetaInfo()));
}

int32_t NpuCommunicatorBase::ReduceScatter(const void *send_buff, void *recv_buff, size_t recv_count,
                                           zbal_datatype_t data_type, zbal_reduce_op_t op, aclrtStream stream) noexcept
{
    return ZBALOpReduceScatter(send_buff, recv_buff, recv_count, data_type, stream, op,
                               const_cast<CommGroupInfo &>(GetMetaInfo()));
}

int32_t NpuCommunicatorBase::AllGather(const void *send_buff, void *recv_buff, size_t send_count,
                                       zbal_datatype_t data_type, aclrtStream stream) noexcept
{
    return ZBALOpAllGather(send_buff, recv_buff, send_count, data_type, stream,
                           const_cast<CommGroupInfo &>(GetMetaInfo()));
}

int32_t NpuCommunicatorBase::AlltoAllV(const void *sendBuff, void *recvBuff, void *sendCumSum, void *recvSplitCounts,
                                       void *elements, zbal_datatype_t dataType, aclrtStream stream) noexcept
{
    return ZBALOpAlltoAllV(sendBuff, recvBuff, sendCumSum, recvSplitCounts, elements, dataType, stream,
                           const_cast<CommGroupInfo &>(GetMetaInfo()));
}

int32_t NpuCommunicatorBase::Broadcast(const void *buf, uint64_t data_count, zbal_datatype_t dataType, uint16_t root,
                                       aclrtStream stream) noexcept
{
    auto result = ZBALOpBroadcast(buf, data_count, dataType, root, stream, const_cast<CommGroupInfo &>(GetMetaInfo()));
    return result;
}

int32_t NpuCommunicatorBase::Barrier(aclrtStream stream) noexcept
{
    return ZBALOpBarrier(stream, const_cast<CommGroupInfo &>(GetMetaInfo()));
}

int32_t NpuCommunicatorBase::Send(const void *sendBuff, size_t sendCount, zbal_datatype_t dataType, uint32_t peer,
                                  aclrtStream stream) noexcept
{
    return ZBALOpSend(sendBuff, sendCount, dataType, peer, stream, const_cast<CommGroupInfo &>(GetMetaInfo()));
}

int32_t NpuCommunicatorBase::Recv(const void *recvBuff, size_t recvCount, zbal_datatype_t dataType, uint32_t peer,
                                  aclrtStream stream) noexcept
{
    return ZBALOpRecv(recvBuff, recvCount, dataType, peer, stream, const_cast<CommGroupInfo &>(GetMetaInfo()));
}

int32_t NpuCommunicatorBase::DispatchNormalNotify(const zbal_tensor_info_t *sendTokensPerExpert, int64_t sendCount,
                                                  int64_t topKNum, const zbal_tensor_info_t *recvBuff,
                                                  const zbal_tensor_info_t *totalRecvTokens,
                                                  const zbal_tensor_info_t *recvTokensPerExpert,
                                                  const zbal_tensor_info_t *pushTargetOffset,
                                                  const zbal_tensor_info_t *balanceMatrix, aclrtStream stream,
                                                  int64_t flags) noexcept
{
    float factorHigh = Func::GetEnv("DEEPEP_BALANCE_FACTOR_HIGH", 1.2);
    float factorLow = Func::GetEnv("DEEPEP_BALANCE_FACTOR_LOW", 1.0);
    ZBAL_VALIDATE_RETURN(factorHigh > 1.1, "balance factor high need be large than 1.1", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(factorLow > 0.9, "balance factor low need be large than 0.9", Z_INVALID_PARAM);
    return ZBALOpNotifyDispatch(sendTokensPerExpert, sendCount, topKNum, recvBuff, totalRecvTokens, recvTokensPerExpert,
                                pushTargetOffset, balanceMatrix, factorHigh, factorLow, stream, GetMetaInfo(), flags);
}

int32_t NpuCommunicatorBase::DispatchNormalLayout(
    const zbal_tensor_info_t *topkIndex, int64_t tokens, int64_t expertNum, int64_t topkNum,
    const zbal_tensor_info_t *tokensPerRank, const zbal_tensor_info_t *tokensPerExpert,
    const zbal_tensor_info_t *isTokenInRank, const zbal_tensor_info_t *sendTokensIndex,
    const zbal_tensor_info_t *notifySendData, aclrtStream stream, int64_t flags) noexcept
{
    return ZBALOpDispatchLayout(topkIndex, tokens, expertNum, topkNum, tokensPerRank, tokensPerExpert, isTokenInRank,
                                sendTokensIndex, notifySendData, stream, GetMetaInfo(), flags);
}

int32_t NpuCommunicatorBase::DispatchNormal(const zbal_tensor_info_t *srcTokens, const zbal_tensor_info_t *topkIndex,
                                            const zbal_tensor_info_t *sendTokensIndex,
                                            const zbal_tensor_info_t *pushTargetOffset,
                                            const zbal_tensor_info_t *balanceMatrix, int64_t expertNum,
                                            zbal_quant_mode_t quantMode, const zbal_tensor_info_t *destTokens,
                                            const zbal_tensor_info_t *destScale, aclrtStream stream,
                                            int64_t flags) noexcept
{
    bool enableBalance = Func::GetEnv("DEEPEP_ENABLE_REBALANCE", 0) > 0;
    return ZBALOpDispatchNormal(srcTokens, topkIndex, sendTokensIndex, pushTargetOffset, balanceMatrix, expertNum,
                                quantMode, destTokens, destScale, enableBalance, stream, GetMetaInfo(), flags);
}

int32_t NpuCommunicatorBase::CombineNormal(const zbal_tensor_info_t *srcTokens,
                                           const zbal_tensor_info_t *srcTokensPerEp,
                                           const zbal_tensor_info_t *topKWeight, const zbal_tensor_info_t *topkIndex,
                                           const zbal_tensor_info_t *sendTokensIndex,
                                           const zbal_tensor_info_t *balanceMatrix, uint16_t expertNum,
                                           const zbal_tensor_info_t *destTokens, aclrtStream stream,
                                           int64_t flags) noexcept
{
    bool enableBalance = Func::GetEnv("DEEPEP_ENABLE_REBALANCE", 0) > 0;
    return ZBALOpCombineNormal(srcTokens, srcTokensPerEp, topKWeight, topkIndex, sendTokensIndex, balanceMatrix,
                               expertNum, destTokens, enableBalance, stream, GetMetaInfo(), flags);
}

int32_t NpuCommunicatorBase::DispatchLowLatency(
    const zbal_tensor_info_t *x, const zbal_tensor_info_t *expertIds, int64_t moeExpertNum, int64_t sharedExpertNum,
    int64_t sharedExpertRankNum, int64_t quantMode, int64_t globalBs, int64_t magicVal, int64_t expertTokenNumsType,
    const zbal_tensor_info_t *expandXOut, const zbal_tensor_info_t *dynamicScalesOut,
    const zbal_tensor_info_t *expandIdxOut, const zbal_tensor_info_t *expertTokenNumsOut,
    const zbal_tensor_info_t *epRecvCountsOut, const zbal_tensor_info_t *putOffset,
    const zbal_tensor_info_t *putOffsetStatus, aclrtStream stream, int64_t flags) noexcept
{
    return ZBALOpDispatchLowLatency(x, expertIds, moeExpertNum, sharedExpertNum, sharedExpertRankNum, quantMode,
                                    globalBs, magicVal, expertTokenNumsType, expandXOut, dynamicScalesOut, expandIdxOut,
                                    expertTokenNumsOut, epRecvCountsOut, putOffset, putOffsetStatus, stream,
                                    GetMetaInfo(), flags);
}

int32_t NpuCommunicatorBase::CombineLowLatency(const zbal_tensor_info_t *expandX, const zbal_tensor_info_t *expertIds,
                                               const zbal_tensor_info_t *expertIdx,
                                               const zbal_tensor_info_t *epSendCounts,
                                               const zbal_tensor_info_t *expertScales, const zbal_tensor_info_t *xOut,
                                               int64_t moeExpertNum, aclrtStream stream, int64_t flags) noexcept
{
    return ZBALOpCombineLowLatency(expandX, expertIds, expertIdx, epSendCounts, expertScales, xOut, moeExpertNum,
                                   stream, GetMetaInfo(), flags);
}

} // namespace operators
} // namespace zbal
