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
#ifndef ZBAL_COMMUNICATOR_DUMMY_H
#define ZBAL_COMMUNICATOR_DUMMY_H

#include "zbal_communicator.h"

namespace zbal {
namespace operators {

/**
 * This is for unit test, a dummy communicator doesn't depend on any hardware
 */
class CommunicatorDummy : public Communicator {
public:
    CommunicatorDummy(const CommGroupOptions &options, bool isWorldGroup, const CommunicatorPtr &worldGroup)
        : Communicator(options, isWorldGroup, worldGroup)
    {}

    ~CommunicatorDummy() override = default;

    ZResult Initialize() noexcept override
    {
        return Z_OK;
    }

    void UnInitialize() noexcept override {}

    void ConstructCommGroupInfo(const CommGroupOptions &opt) noexcept override {}

    void DumpProfilingTrace() noexcept override {}

    void SignalDumpTrace() noexcept override {}

    ZResult AssignGatherGroupId(AutoReleaseGroupId &id) noexcept override
    {
        return Z_OK;
    }

    int32_t AllReduce(const void *send_buff, void *recv_buff, void *buffer, size_t count, size_t buf_cnt,
                      zbal_datatype_t data_type, zbal_reduce_op_t op, aclrtStream stream) noexcept override
    {
        return Z_OK;
    }

    int32_t ReduceScatter(const void *send_buff, void *recv_buff, size_t recv_count, zbal_datatype_t data_type,
                          zbal_reduce_op_t op, aclrtStream stream) noexcept override
    {
        return Z_OK;
    }

    int32_t AllGather(const void *send_buff, void *recv_buff, size_t send_count, zbal_datatype_t data_type,
                      aclrtStream stream) noexcept override
    {
        return Z_OK;
    }

    int32_t AlltoAllBase(const void *sendBuff, void *recvBuff, uint64_t data_count, zbal_datatype_t dataType,
                         aclrtStream stream) noexcept
    {
        return Z_OK;
    }

    int32_t AlltoAllV(const void *sendBuff, void *recvBuff, void *sendCumSum, void *recvSplitCounts, void *elements,
                      zbal_datatype_t dataType, aclrtStream stream) noexcept
    {
        return Z_OK;
    }

    int32_t Broadcast(const void *buf, uint64_t data_count, zbal_datatype_t dataType, uint16_t root,
                      aclrtStream stream) noexcept
    {
        return Z_OK;
    }

    int32_t Scatter(const void *sendBuff, void *recvBuff, uint64_t data_count, zbal_datatype_t dataType, uint16_t root,
                    aclrtStream stream) noexcept
    {
        return Z_OK;
    }

    int32_t Barrier(aclrtStream stream) noexcept
    {
        return Z_OK;
    }

    int32_t Send(const void *sendBuff, size_t sendCount, zbal_datatype_t dataType, uint32_t peer,
                 aclrtStream stream) noexcept
    {
        return Z_OK;
    }

    int32_t Recv(const void *recvBuff, size_t recvCount, zbal_datatype_t dataType, uint32_t peer,
                 aclrtStream stream) noexcept
    {
        return Z_OK;
    }

    int32_t DispatchNormalNotify(const zbal_tensor_info_t *sendTokensPerExpert, int64_t sendCount, int64_t topKNum,
                                 const zbal_tensor_info_t *recvBuff, const zbal_tensor_info_t *totalRecvTokens,
                                 const zbal_tensor_info_t *recvTokensPerExpert,
                                 const zbal_tensor_info_t *pushTargetOffset, const zbal_tensor_info_t *balanceMatrix,
                                 aclrtStream stream, int64_t flags) noexcept
    {
        return Z_OK;
    }

    int32_t DispatchNormalLayout(const zbal_tensor_info_t *topkIndex, int64_t tokens, int64_t expertNum,
                                 int64_t topkNum, const zbal_tensor_info_t *tokensPerRank,
                                 const zbal_tensor_info_t *tokensPerExpert, const zbal_tensor_info_t *isTokenInRank,
                                 const zbal_tensor_info_t *sendTokensIndex, const zbal_tensor_info_t *notifySendData,
                                 aclrtStream stream, int64_t flags) noexcept
    {
        return Z_OK;
    }

    int32_t DispatchNormal(const zbal_tensor_info_t *srcTokens, const zbal_tensor_info_t *topkIndex,
                           const zbal_tensor_info_t *sendTokensIndex, const zbal_tensor_info_t *pushTargetOffset,
                           const zbal_tensor_info_t *balanceMatrix, int64_t expertNum, zbal_quant_mode_t quantMode,
                           const zbal_tensor_info_t *destTokens, const zbal_tensor_info_t *destScale,
                           aclrtStream stream, int64_t flags) noexcept
    {
        return Z_OK;
    }

    int32_t CombineNormal(const zbal_tensor_info_t *srcTokens, const zbal_tensor_info_t *srcTokensPerEp,
                          const zbal_tensor_info_t *topKWeight, const zbal_tensor_info_t *topkIndex,
                          const zbal_tensor_info_t *sendTokensIndex, const zbal_tensor_info_t *balanceMatrix,
                          uint16_t expertNum, const zbal_tensor_info_t *destTokens, aclrtStream stream,
                          int64_t flags) noexcept
    {
        return Z_OK;
    }

    int32_t DispatchLowLatency(const zbal_tensor_info_t *x, const zbal_tensor_info_t *expertIds, int64_t moeExpertNum,
                               int64_t sharedExpertNum, int64_t sharedExpertRankNum, int64_t quantMode,
                               int64_t globalBs, int64_t magicVal, int64_t expertTokenNumsType,
                               const zbal_tensor_info_t *expandXOut, const zbal_tensor_info_t *dynamicScalesOut,
                               const zbal_tensor_info_t *expandIdxOut, const zbal_tensor_info_t *expertTokenNumsOut,
                               const zbal_tensor_info_t *epRecvCountsOut, const zbal_tensor_info_t *putOffset,
                               const zbal_tensor_info_t *putOffsetStatus, aclrtStream stream, int64_t flags) noexcept
    {
        return Z_OK;
    }

    int32_t CombineLowLatency(const zbal_tensor_info_t *expandX, const zbal_tensor_info_t *expertIds,
                              const zbal_tensor_info_t *expertIdx, const zbal_tensor_info_t *epSendCounts,
                              const zbal_tensor_info_t *expertScales, const zbal_tensor_info_t *xOut,
                              int64_t moeExpertNum, aclrtStream stream, int64_t flags) noexcept
    {
        return Z_OK;
    }
};
} // namespace operators
} // namespace zbal

#endif // ZBAL_COMMUNICATOR_DUMMY_H
