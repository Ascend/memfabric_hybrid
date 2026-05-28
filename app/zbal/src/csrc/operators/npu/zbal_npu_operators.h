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
#ifndef ZBAL_NPU_OPERATORS_H
#define ZBAL_NPU_OPERATORS_H

#include "zbal.h"
#include "zbal_communicator.h"

int32_t ZBALOpAllGather(const void *sendBuff, void *recvBuff, size_t sendCount, zbal_datatype_t dataType,
                        aclrtStream stream, CommGroupInfo &groupInfo);

int32_t ZBALOpReduceScatter(const void *inp, void *out, size_t recvNumel, zbal_datatype_t dataType, aclrtStream stream,
                            zbal_reduce_op_t reduceOp, CommGroupInfo &groupInfo);

int32_t ZBALOpAllReduce(const void *inp, void *out, void *buf, size_t numel, size_t bufCnt, zbal_datatype_t dataType,
                        aclrtStream stream, zbal_reduce_op_t reduceOp, CommGroupInfo &groupInfo);

int32_t ZBALOpAlltoAllV(const void *sendBuff, void *recvBuff, void *sendCumSum, void *recvSplitCounts, void *elements,
                        zbal_datatype_t dataType, aclrtStream stream, CommGroupInfo &groupInfo);

int32_t ZBALOpBroadcast(const void *buff, size_t sendCount, zbal_datatype_t dataType, uint16_t root, aclrtStream stream,
                        CommGroupInfo &groupInfo);

int32_t ZBALOpScatter(const void *sendBuff, void *recvBuff, size_t sendCount, zbal_datatype_t dataType, uint16_t root,
                      aclrtStream stream, CommGroupInfo &groupInfo);

int32_t ZBALOpBarrier(aclrtStream stream, CommGroupInfo &groupInfo);

int32_t ZBALOpSend(const void *sendBuff, size_t sendCount, zbal_datatype_t dataType, uint32_t peer, aclrtStream stream,
                   CommGroupInfo &groupInfo);

int32_t ZBALOpRecv(const void *recvBuff, size_t recvCount, zbal_datatype_t dataType, uint32_t peer, aclrtStream stream,
                   CommGroupInfo &groupInfo);

int32_t ZBALOpDispatchLayout(const zbal_tensor_info_t *topkIndex, int64_t tokens, int64_t expertNum, int64_t topkNum,
                             const zbal_tensor_info_t *tokensPerRank, const zbal_tensor_info_t *tokensPerExpert,
                             const zbal_tensor_info_t *sendTokensIndex, const zbal_tensor_info_t *notifySendData,
                             aclrtStream stream, const CommGroupInfo &groupInfo, int64_t flags);

int32_t ZBALOpNotifyDispatch(const zbal_tensor_info_t *sendTokensPerExpert, int64_t sendCount, int64_t topKNum,
                             const zbal_tensor_info_t *recvBuff, const zbal_tensor_info_t *totalRecvTokens,
                             const zbal_tensor_info_t *recvTokensPerExpert, const zbal_tensor_info_t *pushTargetOffset,
                             const zbal_tensor_info_t *balanceMatrix, float factorHigh, float factorLow,
                             aclrtStream stream, const CommGroupInfo &groupInfo, int64_t flags);

int32_t ZBALOpDispatchNormal(const zbal_tensor_info_t *srcTokens, const zbal_tensor_info_t *topkIndex,
                             const zbal_tensor_info_t *sendTokensIndex, const zbal_tensor_info_t *pushTargetOffset,
                             const zbal_tensor_info_t *balanceMatrix, int64_t expertNum, zbal_quant_mode_t quantMode,
                             const zbal_tensor_info_t *destTokens, const zbal_tensor_info_t *destScale,
                             bool enableBalance, aclrtStream stream, const CommGroupInfo &groupInfo, int64_t flags);

int32_t ZBALOpCombineNormal(const zbal_tensor_info_t *srcTokens, const zbal_tensor_info_t *srcTokensPerEp,
                            const zbal_tensor_info_t *topKWeight, const zbal_tensor_info_t *topkIndex,
                            const zbal_tensor_info_t *sendTokensIndex, const zbal_tensor_info_t *balanceMatrix,
                            uint16_t expertNum, const zbal_tensor_info_t *destTokens, bool enableBalance,
                            aclrtStream stream, const CommGroupInfo &groupInfo, int64_t flags);

int32_t ZBALOpDispatchLowLatency(const zbal_tensor_info_t *x, const zbal_tensor_info_t *expertIds, int64_t moeExpertNum,
                                 int64_t sharedExpertNum, int64_t sharedExpertRankNum, int64_t quantMode,
                                 int64_t globalBs, int64_t magicVal, int64_t expertTokenNumsType,
                                 const zbal_tensor_info_t *expandXOut, const zbal_tensor_info_t *dynamicScalesOut,
                                 const zbal_tensor_info_t *expandIdxOut, const zbal_tensor_info_t *expertTokenNumsOut,
                                 const zbal_tensor_info_t *epRecvCountsOut, const zbal_tensor_info_t *putOffset,
                                 const zbal_tensor_info_t *putOffsetStatus, aclrtStream stream,
                                 const CommGroupInfo &groupInfo, int64_t flags);

int32_t ZBALOpCombineLowLatency(const zbal_tensor_info_t *expandX, const zbal_tensor_info_t *expertIds,
                                const zbal_tensor_info_t *expertIdx, const zbal_tensor_info_t *epSendCounts,
                                const zbal_tensor_info_t *expertScales, const zbal_tensor_info_t *xOut,
                                int64_t moeExpertNum, aclrtStream stream, const CommGroupInfo &groupInfo,
                                int64_t flags);

#endif // ZBAL_NPU_OPERATORS_H
