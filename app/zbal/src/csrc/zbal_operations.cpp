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
#include "zbal_common_includes.h"
#include "zbal_communicator.h"
#include "zbal_init_state.h"

using namespace zbal;
using namespace zbal::operators;

#ifdef __cplusplus
extern "C" {
#endif

ZBAL_API int32_t zbal_comm_create(zbal_comm_options_t *options, zbal_comm_t *comm)
{
    ZBAL_VALIDATE_RETURN(options != nullptr, "Create communicator failed as options is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(comm != nullptr, "Create communicator failed as comm is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(options->name != nullptr, "Create communicator failed as name is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(strlen(options->name) != 0, "Create communicator failed as name is empty", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(strlen(options->name) < ZBAL_COMM_NAME_MAX,
                         "Create communicator failed as name is too long, which should be less than "
                             << ZBAL_COMM_NAME_MAX,
                         Z_INVALID_PARAM);

    ZBAL_LOG_INFO("options dump, " << (*options));

    auto &state = ZBALInitState::Instance();

    if (!state.Bootstrapped()) {
        ZBAL_LOG_ERROR("Create communicator failed as not bootstrapped");
        return Z_NOT_BOOTSTRAPPED;
    } else if (options->isWorldGroup == 1 && state.ext_.worldSize != options->groupSize) {
        ZBAL_LOG_ERROR("Create communicator failed as world size "
                       << options->groupSize << " is not equal to bootstrap's world size " << state.ext_.worldSize);
        return Z_NOT_BOOTSTRAPPED;
    } else if (options->isWorldGroup == 0 && state.ext_.worldSize < options->groupSize) {
        ZBAL_LOG_ERROR("Create communicator failed as world size "
                       << options->groupSize << " is bigger than bootstrap's world size " << state.ext_.worldSize);
        return Z_NOT_BOOTSTRAPPED;
    }

    /* create one comm */
    auto result = Communicator::Create(*options, comm, state.ext_);
    if (result != Z_OK) {
        return result;
    }

    /* update init state */
    state.CommunicatorCreated(1);

    return Z_OK;
}

ZBAL_API int32_t zbal_comm_get_property(zbal_comm_t comm, zbal_comm_property_t *property)
{
    ZBAL_VALIDATE_RETURN(comm != nullptr, "Get property failed as comm is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(property != nullptr, "Get property failed as property is null", Z_INVALID_PARAM);

    return Communicator::GetCommProperty(comm, property);
}

ZBAL_API zbal_comm_t zbal_comm_get_global()
{
    zbal_comm_t comm = nullptr;
    auto result = Communicator::GetGlobalComm(&comm);
    if (result != Z_OK) {
        return reinterpret_cast<uintptr_t>(nullptr);
    }
    return comm;
}

ZBAL_API zbal_comm_t zbal_comm_get_by_name(const char *name)
{
    ZBAL_VALIDATE_RETURN(name != nullptr, "Get communicator failed as name is null", nullptr);

    zbal_comm_t comm = nullptr;
    auto result = Communicator::Lookup(std::string(name), &comm);
    if (result != Z_OK) {
        return nullptr;
    }

    return comm;
}

ZBAL_API int32_t zbal_comm_destroy(zbal_comm_t comm, uint32_t flags)
{
    ZBAL_VALIDATE_RETURN(comm != nullptr, "Destroy communicator failed as comm is null", Z_INVALID_PARAM);

    /* destroy one */
    auto result = Communicator::Destroy(comm, flags);
    if (result != Z_OK) {
        return result;
    }

    /* update init state */
    ZBALInitState::Instance().CommunicatorDestroy(1);

    return Z_OK;
}

ZBAL_API void zbal_comm_destroy_all(uint32_t flags)
{
    (void)flags;
    ZBAL_LOG_INFO("destroy all comm group");
    Communicator::DestroyAll();
}

ZBAL_API int32_t zbal_all_reduce(const void *send_buff, void *recv_buff, void *buffer, size_t count,
                                 zbal_datatype_t data_type, zbal_reduce_op_t op, zbal_comm_t comm, aclrtStream stream)
{
    if (send_buff == nullptr) {
        return Z_OK;
    }
    ZBAL_VALIDATE_RETURN(recv_buff != nullptr, "AllReduce failed as recv_buff is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(count > 0, "AllReduce failed as count " << count << " is invalid", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(data_type >= 0 && data_type < ZBAL_DATA_TYPE_BUTT,
                         "AllReduce failed as data_type " << data_type << " is invalid", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(op >= 0 && op < ZBAL_REDUCE_BUTT, "AllReduce failed as op " << op << " is invalid",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(comm != nullptr, "AllReduce failed as comm is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(buffer != nullptr, "Allreduce tmp buffer is null", Z_INVALID_PARAM);

    /* covert inner object ptr and execute op */
    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->AllReduce(send_buff, recv_buff, buffer, count, data_type, op, stream);
}

ZBAL_API int32_t zbal_reduce_scatter(const void *send_buff, void *recv_buff, size_t recv_count,
                                     zbal_datatype_t data_type, zbal_reduce_op_t op, zbal_comm_t comm,
                                     aclrtStream stream)
{
    if (send_buff == nullptr) {
        return Z_OK;
    }
    ZBAL_VALIDATE_RETURN(recv_buff != nullptr, "ReduceScatter failed as recv_buff is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(recv_count > 0, "ReduceScatter failed as recv_count " << recv_count << " is invalid",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(data_type >= 0 && data_type < ZBAL_DATA_TYPE_BUTT,
                         "ReduceScatter failed as data_type " << data_type << " is invalid", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(op >= 0 && op < ZBAL_REDUCE_BUTT, "ReduceScatter failed as op " << op << " is invalid",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(comm != nullptr, "ReduceScatter failed as comm is null", Z_INVALID_PARAM);

    /* covert inner object ptr and execute op */
    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->ReduceScatter(send_buff, recv_buff, recv_count, data_type, op, stream);
}

ZBAL_API int32_t zbal_all_gather(const void *send_buff, void *recv_buff, size_t send_count, zbal_datatype_t data_type,
                                 zbal_comm_t comm, aclrtStream stream)
{
    if (send_buff == nullptr) {
        return Z_OK;
    }
    ZBAL_VALIDATE_RETURN(recv_buff != nullptr, "AllGather failed as recv_buff is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(send_count > 0, "AllGather failed as send_count " << send_count << " is invalid",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(data_type >= 0 && data_type < ZBAL_DATA_TYPE_BUTT,
                         "AllGather failed as data_type " << data_type << " is invalid", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(comm != nullptr, "AllGather failed as comm is null", Z_INVALID_PARAM);

    /* covert inner object ptr and execute op */
    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->AllGather(send_buff, recv_buff, send_count, data_type, stream);
}

ZBAL_API int32_t zbal_barrier(zbal_comm_t comm, aclrtStream stream)
{
    ZBAL_VALIDATE_RETURN(comm != nullptr, "barrier failed as comm is null", Z_INVALID_PARAM);

    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->Barrier(stream);
}

ZBAL_API int32_t zbal_all_to_all_v(const void *sendBuff, void *recvBuff, void *sendCumSum, void *recvSplitCounts,
                                   void *elements, zbal_datatype_t dataType, zbal_comm_t comm, aclrtStream stream)
{
    ZBAL_VALIDATE_RETURN(sendBuff != nullptr, "AlltoAllv failed as send/recv buff is nullptr", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(sendCumSum != nullptr, "AlltoAllv failed as input split counts buff is nullptr",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(recvSplitCounts != nullptr, "AlltoAllv failed as output split counts buff is nullptr",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(elements != nullptr, "AlltoAllv failed as elements buff is nullptr", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(dataType >= 0 && dataType < ZBAL_DATA_TYPE_BUTT, "alltoallv data type invalid.",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(comm != nullptr, "alltoallv failed as comm is null", Z_INVALID_PARAM);

    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->AlltoAllV(sendBuff, recvBuff, sendCumSum, recvSplitCounts, elements, dataType, stream);
}

ZBAL_API int32_t zbal_broadcast(const void *buf, uint64_t data_count, zbal_datatype_t dataType, uint16_t root,
                                zbal_comm_t comm, aclrtStream stream)
{
    if (buf == nullptr) {
        return Z_OK;
    }
    ZBAL_VALIDATE_RETURN(dataType >= 0 && dataType < ZBAL_DATA_TYPE_BUTT, "broadcast data type invalid.",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(comm != nullptr, "broadcast failed as comm is null", Z_INVALID_PARAM);

    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->Broadcast(buf, data_count, dataType, root, stream);
}

ZBAL_API int32_t zbal_scatter(const void *sendBuff, void *recvBuff, uint64_t data_count, zbal_datatype_t dataType,
                              uint16_t root, zbal_comm_t comm, aclrtStream stream)
{
    ZBAL_VALIDATE_RETURN(recvBuff != nullptr, "Scatter failed as recv buff is nullptr", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(dataType >= 0 && dataType < ZBAL_DATA_TYPE_BUTT, "scatter data type invalid.",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(comm != nullptr, "scatter failed as comm is null", Z_INVALID_PARAM);

    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->Scatter(sendBuff, recvBuff, data_count, dataType, root, stream);
}

ZBAL_API int32_t zbal_send(const void *sendBuff, zbal_datatype_t dataType, uint32_t peer,
                           zbal_comm_t comm, aclrtStream stream)
{
    ZBAL_VALIDATE_RETURN(sendBuff != nullptr, "send failed as send buff is nullptr", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(dataType >= 0 && dataType < ZBAL_DATA_TYPE_BUTT, "send data type invalid.", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(comm != nullptr, "send failed as comm is null", Z_INVALID_PARAM);

    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->Send(sendBuff, dataType, peer, stream);
}

ZBAL_API int32_t zbal_recv(const void *recvBuff, size_t recvCount, zbal_datatype_t dataType, uint32_t peer,
                           zbal_comm_t comm, aclrtStream stream)
{
    ZBAL_VALIDATE_RETURN(recvBuff != nullptr, "recv failed as recv buff is nullptr", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(dataType >= 0 && dataType < ZBAL_DATA_TYPE_BUTT, "recv data type invalid.", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(comm != nullptr, "recv failed as comm is null", Z_INVALID_PARAM);

    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->Recv(recvBuff, recvCount, dataType, peer, stream);
}

ZBAL_API int32_t zbal_dispatch_normal_notify(const zbal_tensor_info_t *sendTokensPerExpert, int64_t sendCount,
                                             int64_t topKNum, const zbal_tensor_info_t *recvBuff,
                                             const zbal_tensor_info_t *totalRecvTokens,
                                             const zbal_tensor_info_t *recvTokensPerExpert,
                                             const zbal_tensor_info_t *pushTargetOffset,
                                             const zbal_tensor_info_t *balanceMatrix, zbal_comm_t comm,
                                             aclrtStream stream, int64_t flags)
{
    ZBAL_VALIDATE_RETURN(sendTokensPerExpert != nullptr, "NotifyDispatch failed as sendTokensPerExpert is null",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(sendCount > 0, "NotifyDispatch failed as sendCount " << sendCount << " is invalid",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(topKNum > 0, "NotifyDispatch failed as topKNum " << topKNum << " is invalid", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(recvBuff != nullptr, "NotifyDispatch failed as recvBuff is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(totalRecvTokens != nullptr, "NotifyDispatch failed as totalRecvTokens is null",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(recvTokensPerExpert != nullptr, "NotifyDispatch failed as recvTokensPerExpert is null",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(pushTargetOffset != nullptr, "NotifyDispatch failed as pushTargetOffset is null",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(balanceMatrix != nullptr, "NotifyDispatch failed as balanceMatrix is null", Z_INVALID_PARAM);

    /* covert inner object ptr and execute op */
    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->DispatchNormalNotify(sendTokensPerExpert, sendCount, topKNum, recvBuff, totalRecvTokens,
                                           recvTokensPerExpert, pushTargetOffset, balanceMatrix, stream, flags);
}

ZBAL_API int32_t zbal_dispatch_normal_layout(const zbal_tensor_info_t *topkIndex, int64_t tokens, int64_t expertNum,
                                             int64_t topkNum, const zbal_tensor_info_t *tokensPerRank,
                                             const zbal_tensor_info_t *tokensPerExpert,
                                             const zbal_tensor_info_t *sendTokensIndex,
                                             const zbal_tensor_info_t *notifySendData, zbal_comm_t comm,
                                             aclrtStream stream, int64_t flags)
{
    ZBAL_VALIDATE_RETURN(topkIndex != nullptr, "DispatchLayout failed as topkIndex is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(tokensPerRank != nullptr, "DispatchLayout failed as tokensPerRank is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(tokensPerExpert != nullptr, "DispatchLayout failed as tokensPerExpert is null",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(sendTokensIndex != nullptr, "DispatchLayout failed as sendTokensIndex is null",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(notifySendData != nullptr, "DispatchLayout failed as notifySendData is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(tokens >= 0, "DispatchLayout failed as tokens " << tokens << " is invalid", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(expertNum > 0, "DispatchLayout failed as expertNum " << expertNum << " is invalid",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(topkNum > 0, "DispatchLayout failed as topkNum " << topkNum << " is invalid", Z_INVALID_PARAM);

    /* covert inner object ptr and execute op */
    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->DispatchNormalLayout(topkIndex, tokens, expertNum, topkNum, tokensPerRank, tokensPerExpert,
                                           sendTokensIndex, notifySendData, stream, flags);
}

ZBAL_API int32_t zbal_dispatch_normal(const zbal_tensor_info_t *srcTokens, const zbal_tensor_info_t *topkIndex,
                                      const zbal_tensor_info_t *sendTokensIndex,
                                      const zbal_tensor_info_t *pushTargetOffset,
                                      const zbal_tensor_info_t *balanceMatrix, int64_t expertNum,
                                      zbal_quant_mode_t quantMode, const zbal_tensor_info_t *destTokens,
                                      const zbal_tensor_info_t *destScale, zbal_comm_t comm, aclrtStream stream,
                                      int64_t flags)
{
    ZBAL_VALIDATE_RETURN(srcTokens != nullptr, "DispatchNormal failed as srcTokens is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(topkIndex != nullptr, "DispatchNormal failed as topkIndex is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(sendTokensIndex != nullptr, "DispatchNormal failed as sendTokensIndex is null",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(pushTargetOffset != nullptr, "DispatchNormal failed as pushTargetOffset is null",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(balanceMatrix != nullptr, "DispatchNormal failed as balanceMatrix is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(sendTokensIndex != nullptr, "DispatchNormal failed as sendTokensIndex is null",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(destTokens != nullptr, "DispatchNormal failed as destTokens is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(destScale != nullptr, "DispatchNormal failed as destScale is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(expertNum > 0, "DispatchNormal failed as expertNum " << expertNum << " is invalid",
                         Z_INVALID_PARAM);

    /* covert inner object ptr and execute op */
    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->DispatchNormal(srcTokens, topkIndex, sendTokensIndex, pushTargetOffset, balanceMatrix, expertNum,
                                     quantMode, destTokens, destScale, stream, flags);
}

ZBAL_API int32_t zbal_combine_normal(const zbal_tensor_info_t *srcTokens, const zbal_tensor_info_t *srcTokensPerEp,
                                     const zbal_tensor_info_t *topKWeight, const zbal_tensor_info_t *topkIndex,
                                     const zbal_tensor_info_t *sendTokensIndex, const zbal_tensor_info_t *balanceMatrix,
                                     uint16_t expertNum, const zbal_tensor_info_t *destTokens, zbal_comm_t comm,
                                     aclrtStream stream, int64_t flags)
{
    ZBAL_VALIDATE_RETURN(srcTokens != nullptr, "CombineNormal failed as srcTokens is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(srcTokensPerEp != nullptr, "CombineNormal failed as srcTokensPerEp is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(topKWeight != nullptr, "CombineNormal failed as topKWeight is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(topkIndex != nullptr, "CombineNormal failed as topkIndex is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(sendTokensIndex != nullptr, "CombineNormal failed as sendTokensIndex is null",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(balanceMatrix != nullptr, "CombineNormal failed as balanceMatrix is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(destTokens != nullptr, "CombineNormal failed as destTokens is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(expertNum > 0, "CombineNormal failed as expertNum " << expertNum << " is invalid",
                         Z_INVALID_PARAM);

    /* covert inner object ptr and execute op */
    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->CombineNormal(srcTokens, srcTokensPerEp, topKWeight, topkIndex, sendTokensIndex, balanceMatrix,
                                    expertNum, destTokens, stream, flags);
}

ZBAL_API int32_t zbal_dispatch_low_latency(
    const zbal_tensor_info_t *x, const zbal_tensor_info_t *expertIds, int64_t moeExpertNum, int64_t sharedExpertNum,
    int64_t sharedExpertRankNum, int64_t quantMode, int64_t globalBs, int64_t magicVal, int64_t expertTokenNumsType,
    const zbal_tensor_info_t *expandXOut, const zbal_tensor_info_t *dynamicScalesOut,
    const zbal_tensor_info_t *expandIdxOut, const zbal_tensor_info_t *expertTokenNumsOut,
    const zbal_tensor_info_t *epRecvCountsOut, const zbal_tensor_info_t *putOffset,
    const zbal_tensor_info_t *putOffsetStatus, zbal_comm_t comm, aclrtStream stream, int64_t flags)
{
    ZBAL_VALIDATE_RETURN(x != nullptr, "DispatchLowlatency failed as x is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(expertIds != nullptr, "DispatchLowlatency failed as expertIds is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(moeExpertNum > 0,
                         "DispatchLowlatency failed as moeExpertNum " << moeExpertNum << " is invalid",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(sharedExpertNum >= 0,
                         "DispatchLowlatency failed as sharedExpertNum " << sharedExpertNum << " is invalid",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(sharedExpertRankNum >= 0,
                         "DispatchLowlatency failed as sharedExpertRankNum " << sharedExpertRankNum << " is invalid",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(globalBs >= 0, "DispatchLowlatency failed as globalBs " << globalBs << " is invalid",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(expandXOut != nullptr, "DispatchLowlatency failed as expandXOut is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(dynamicScalesOut != nullptr, "DispatchLowlatency failed as dynamicScalesOut is null",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(expandIdxOut != nullptr, "DispatchLowlatency failed as expandIdxOut is invalid",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(expertTokenNumsOut != nullptr, "DispatchLowlatency failed as expertTokenNumsOut is null",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(epRecvCountsOut != nullptr, "DispatchLowlatency failed as epRecvCountsOut is invalid",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(putOffset != nullptr, "DispatchLowlatency failed as putOffset is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(putOffsetStatus != nullptr, "DispatchLowlatency failed as putOffsetStatus is null",
                         Z_INVALID_PARAM);
    /* covert inner object ptr and execute op */
    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->DispatchLowLatency(x, expertIds, moeExpertNum, sharedExpertNum, sharedExpertRankNum, quantMode,
                                         globalBs, magicVal, expertTokenNumsType, expandXOut, dynamicScalesOut,
                                         expandIdxOut, expertTokenNumsOut, epRecvCountsOut, putOffset, putOffsetStatus,
                                         stream, flags);
}

ZBAL_API int32_t zbal_combine_low_latency(const zbal_tensor_info_t *expandX, const zbal_tensor_info_t *expertIds,
                                          const zbal_tensor_info_t *expertIdx, const zbal_tensor_info_t *epSendCounts,
                                          const zbal_tensor_info_t *expertScales, const zbal_tensor_info_t *xOut,
                                          int64_t moeExpertNum, zbal_comm_t comm, aclrtStream stream, int64_t flags)
{
    ZBAL_VALIDATE_RETURN(expandX != nullptr, "CombineLowlatency failed as expandX is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(expertIds != nullptr, "CombineLowlatency failed as expertIds is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(expertIdx != nullptr, "CombineLowlatency failed as expertIdx is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(epSendCounts != nullptr, "CombineLowlatency failed as epSendCounts is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(expertScales != nullptr, "CombineLowlatency failed as expertScales is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(xOut != nullptr, "CombineLowlatency failed as xOut is null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(moeExpertNum > 0, "CombineLowlatency failed as moeExpertNum " << moeExpertNum << " is invalid",
                         Z_INVALID_PARAM);
    /* covert inner object ptr and execute op */
    auto innerComm = reinterpret_cast<Communicator *>(comm);
    return innerComm->CombineLowLatency(expandX, expertIds, expertIdx, epSendCounts, expertScales, xOut, moeExpertNum,
                                        stream, flags);
}

#ifdef __cplusplus
}
#endif