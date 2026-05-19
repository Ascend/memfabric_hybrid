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
#ifndef ZBAL_OPERATIONS_H_
#define ZBAL_OPERATIONS_H_

#include "zbal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create zero buffer communicator
 *
 * @param options              [in] communicator options
 * @param comm                 [out] created communicator
 * @return 0 if successful
 */
int32_t zbal_comm_create(zbal_comm_options_t *options, zbal_comm_t *comm);

/**
 * @brief Get property of zero buffer communicator object
 *
 * @param comm                 [in] the communicator handle
 * @param property             [in/out]
 * @return 0 if successful
 */
int32_t zbal_comm_get_property(zbal_comm_t comm, zbal_comm_property_t *property);

/**
 * @brief Get global communicator object

 * @return global comm handle if successfully or else nullptr
 */
zbal_comm_t zbal_comm_get_global();

/**
 * @brief Get communicator object by name
 *
 * @param name                 [in] name of the communicator
 * @return comm object if successful, null if no such communicator
 */
zbal_comm_t zbal_comm_get_by_name(const char *name);

/**
 * @brief Destroy zero buffer communicator
 *
 * @param comm                 [in] the communicator to be destroyed
 * @param flags                [in] optional flags
 * @return
 */
int32_t zbal_comm_destroy(zbal_comm_t comm, uint32_t flags);

/**
 * @brief Destroy all communicators
 *
 * @param flags                [in] optional flags
 */
void zbal_comm_destroy_all(uint32_t flags);

/**
 * @brief Do all reduce operation
 *
 * @param sendBuff             [in] pointer of send buffer
 * @param recvBuff             [in] pointer of receive buffer
 * @param count                [in] size of buffer
 * @param dataType             [in] data type
 * @param op                   [in] operation type of reduce
 * @param comm                 [in] zbal communication handle
 * @param stream               [in] stream
 * @return 0 if successful
 */
int32_t zbal_all_reduce(const void *send_buff, void *recv_buff, void *buffer, size_t count, size_t buf_cnt,
                        zbal_datatype_t data_type, zbal_reduce_op_t op, zbal_comm_t comm, aclrtStream stream);

/**
 * @brief Do reduce scatter operation
 *
 * @param sendBuff             [in] pointer of send buffer
 * @param recvBuff             [in] pointer of receive buffer
 * @param recv_count           [in] size of buffer
 * @param dataType             [in] data type
 * @param op                   [in] operation type of reduce
 * @param comm                 [in] zbal communication handle
 * @param stream               [in] stream
 * @return 0 if successful
 */
int32_t zbal_reduce_scatter(const void *sendBuff, void *recvBuff, size_t recv_count, zbal_datatype_t dataType,
                            zbal_reduce_op_t op, zbal_comm_t comm, aclrtStream stream);

/**
 * @brief Do all gather operation
 *
 * @param sendBuff             [in] pointer of send buffer
 * @param recvBuff             [in] pointer of receive buffer
 * @param send_count           [in] size of buffer
 * @param dataType             [in] data type
 * @param comm                 [in] zbal communication handle
 * @param stream               [in] stream
 * @return 0 if successful
 */
int32_t zbal_all_gather(const void *sendBuff, void *recvBuff, size_t send_count, zbal_datatype_t dataType,
                        zbal_comm_t comm, aclrtStream stream);

/**
 * @brief All2allv operation
 *
 * @param sendBuff             [in] pointer of send buffer
 * @param recvBuff             [in] pointer of receive buffer
 * @param sendSplitCounts      [in] pointer of sender split elements array
 * @param recvSplitCounts      [in] pointer of recver split elements array
 * @param coreGroup            [in] pointer of output core split group
 * @param elements             [in] pointer of input and output total elements
 * @param dataType             [in] data type
 * @param comm                 [in] commnunicator handle
 * @param stream               [in] stream
 * @return
 */
int32_t zbal_all_to_all_v(const void *sendBuff, void *recvBuff, void *sendCumSum, void *recvSplitCounts, void *elements,
                          zbal_datatype_t dataType, zbal_comm_t comm, aclrtStream stream);

/**
 * @brief Broadcast operation
 *
 * @param buf                  [in] pointer of send buffer
 * @param data_count           [in] size of elems input buffer
 * @param dataType             [in] data type
 * @param root                 [in] the root rank in the operator.
 * @param comm                 [in] commnunicator handle
 * @param stream               [in] stream
 * @return
 */
int32_t zbal_broadcast(const void *buf, uint64_t data_count, zbal_datatype_t dataType, uint16_t root, zbal_comm_t comm,
                       aclrtStream stream);

/**
 * @brief Scatter operation
 *
 * @param sendBuff             [in] pointer of send buffer
 * @param recvBuff             [in] pointer of receive buffer
 * @param data_count           [in] size of elems input buffer
 * @param dataType             [in] data type
 * @param root                 [in] the root rank in the operator.
 * @param comm                 [in] commnunicator handle
 * @param stream               [in] stream
 * @return
 */
int32_t zbal_scatter(const void *sendBuff, void *recvBuff, uint64_t data_count, zbal_datatype_t dataType, uint16_t root,
                     zbal_comm_t comm, aclrtStream stream);

/**
 * @brief barrier operation
 *
 * @param comm                 [in] commnunicator handle
 * @param stream               [in] stream
 * @return 0 if successful or else error code
 */
int32_t zbal_barrier(zbal_comm_t comm, aclrtStream stream);

/**
 * @brief Point to point send operation
 *
 * @param sendBuff             [in] pointer of send buffer
 * @param sendCount            [in] size of buffer
 * @param dataType             [in] data type
 * @param peer                 [in] rank of peer
 * @param comm                 [in] zbal communication handle
 * @param stream               [in] stream
 * @return 0 if successful
 */
int32_t zbal_send(const void *sendBuff, size_t sendCount, zbal_datatype_t dataType, uint32_t peer, zbal_comm_t comm,
                  aclrtStream stream);

/**
 * @brief Point to point recv operation
 *
 * @param recvBuff             [in] pointer of recv buffer
 * @param recvCount            [in] size of buffer
 * @param dataType             [in] data type
 * @param peer                 [in] rank of peer
 * @param comm                 [in] zbal communication handle
 * @param stream               [in] stream
 * @return 0 if successful
 */
int32_t zbal_recv(const void *recvBuff, size_t recvCount, zbal_datatype_t dataType, uint32_t peer, zbal_comm_t comm,
                  aclrtStream stream);

/**
 * @brief Calculate the number of tokens sent to each rank and other info
 *
 * @param sendTokensPerExpert  [in] the number of tokens to be sent to each expert
 * @param sendCount            [in] send data count
 * @param topKNum              [in] num of topK
 * @param recvBuff             [in] receive data
 * @param totalRecvTokens      [in/out] total recv token num
 * @param recvTokensPerExpert  [in/out] the number of tokens received by each expert
 * @param pushTargetOffset     [in/out] the token offset sent by different ranks to each expert
 * @param balanceMatrix        [in/out] the token range processed by each rank after balanced
 * @param comm                 [in] zbal communication handle
 * @param stream               [in] stream
 * @param flags                [in] optional flags, reserved or extend
 * @return
 */
int32_t zbal_dispatch_normal_notify(const zbal_tensor_info_t *sendTokensPerExpert, int64_t sendCount, int64_t topKNum,
                                    const zbal_tensor_info_t *recvBuff, const zbal_tensor_info_t *totalRecvTokens,
                                    const zbal_tensor_info_t *recvTokensPerExpert,
                                    const zbal_tensor_info_t *pushTargetOffset, const zbal_tensor_info_t *balanceMatrix,
                                    zbal_comm_t comm, aclrtStream stream, int64_t flags);

/**
 * @brief Calculate the layout required for communication
 *
 * @param topkIndex            [in] topK index info of per token
 * @param tokens               [in] num of tokens
 * @param expertNum            [in] num of experts
 * @param topkNum              [in] num of topK
 * @param tokensPerRank        [in/out] the number of tokens to be sent to each rank
 * @param tokensPerExpert      [in/out] the number of tokens to be sent to each expert
 * @param isTokenInRank        [in/out] whether a token be sent to a rank
 * @param sendTokensIndex      [in/out] send index of per token
 * @param notifySendData       [in/out] exchange data for notify
 * @param comm                 [in] zbal communication handle
 * @param stream               [in] compute stream
 * @param flags                [in] optional flags, reserved or extend
 * @return
 */
int32_t zbal_dispatch_normal_layout(const zbal_tensor_info_t *topkIndex, int64_t tokens, int64_t expertNum,
                                    int64_t topkNum, const zbal_tensor_info_t *tokensPerRank,
                                    const zbal_tensor_info_t *tokensPerExpert, const zbal_tensor_info_t *isTokenInRank,
                                    const zbal_tensor_info_t *sendTokensIndex, const zbal_tensor_info_t *notifySendData,
                                    zbal_comm_t comm, aclrtStream stream, int64_t flags);

/**
 * @brief Dispatch operation in push mode
 *
 * @param srcTokens            [in] tensor info of source tokens to be dispatched
 * @param topkIndex            [in] topK index info of per token
 * @param sendTokensIndex      [in] send index of per token
 * @param pushTargetOffset     [in] the token offset sent by different ranks to each expert
 * @param balanceMatrix        [in] the token range processed by each rank after balanced
 * @param expertNum            [in] number of export to be dispatch
 * @param quantMode            [in] quant mode
 * @param destTokens           [in/out] tensor info of destination tokens
 * @param destScale            [in/out] scale output after quant
 * @param comm                 [in] zbal communication handle
 * @param stream               [in] stream
 * @param flags                [in] optional flags, reserved or extend
 * @return 0 if successful
 */
int32_t zbal_dispatch_normal(const zbal_tensor_info_t *srcTokens, const zbal_tensor_info_t *topkIndex,
                             const zbal_tensor_info_t *sendTokensIndex, const zbal_tensor_info_t *pushTargetOffset,
                             const zbal_tensor_info_t *balanceMatrix, int64_t expertNum, zbal_quant_mode_t quantMode,
                             const zbal_tensor_info_t *destTokens, const zbal_tensor_info_t *destScale,
                             zbal_comm_t comm, aclrtStream stream, int64_t flags);

/**
 * @brief Combine operation in pull mode
 *
 * @param srcTokens            [in] tensor info of tokens be dispatched
 * @param srcTokensPerEp       [in] the number of tokens received by each expert from different ranks (prefix sum form)
 * @param topKWeight           [in] the weights of the topK experts for each token
 * @param topkIndex            [in] topK index info of per token
 * @param sendTokensIndex      [in] send index of per token
 * @param balanceMatrix        [in] the token range processed by each rank after balanced
 * @param expertNum            [in] moe expert number
 * @param destTokens           [in/out] tensor info of destination tokens
 * @param comm                 [in] zbal communication handle
 * @param stream               [in] stream
 * @param flags                [in] optional flags, reserved or extend
 * @return
 */
int32_t zbal_combine_normal(const zbal_tensor_info_t *srcTokens, const zbal_tensor_info_t *srcTokensPerEp,
                            const zbal_tensor_info_t *topKWeight, const zbal_tensor_info_t *topkIndex,
                            const zbal_tensor_info_t *sendTokensIndex, const zbal_tensor_info_t *balanceMatrix,
                            uint16_t expertNum, const zbal_tensor_info_t *destTokens, zbal_comm_t comm,
                            aclrtStream stream, int64_t flags);

int32_t zbal_dispatch_low_latency(const zbal_tensor_info_t *x, const zbal_tensor_info_t *expertIds,
                                  int64_t moeExpertNum, int64_t sharedExpertNum, int64_t sharedExpertRankNum,
                                  int64_t quantMode, int64_t globalBs, int64_t magicVal, int64_t expertTokenNumsType,
                                  const zbal_tensor_info_t *expandXOut, const zbal_tensor_info_t *dynamicScalesOut,
                                  const zbal_tensor_info_t *expandIdxOut, const zbal_tensor_info_t *expertTokenNumsOut,
                                  const zbal_tensor_info_t *epRecvCountsOut, const zbal_tensor_info_t *putOffset,
                                  const zbal_tensor_info_t *putOffsetStatus, zbal_comm_t comm, aclrtStream stream,
                                  int64_t flags);

int32_t zbal_combine_low_latency(const zbal_tensor_info_t *expandX, const zbal_tensor_info_t *expertIds,
                                 const zbal_tensor_info_t *expertIdx, const zbal_tensor_info_t *epSendCounts,
                                 const zbal_tensor_info_t *expertScales, const zbal_tensor_info_t *xOut,
                                 int64_t moeExpertNum, zbal_comm_t comm, aclrtStream stream, int64_t flags);

#ifdef __cplusplus
}
#endif

#endif // ZBAL_OPERATIONS_H_
