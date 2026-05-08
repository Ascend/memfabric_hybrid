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
#ifndef ZBAL_COMMUNICATOR_H
#define ZBAL_COMMUNICATOR_H

#include "zbal_common_includes.h"
#include "zbal_comm_group_id.h"
#include "zbal_comm_types.h"

namespace zbal {
namespace operators {
class Communicator : public ZReferable {
public:
    /**
     * @brief Factory function to create communicator
     *
     * @param options      [in] options of communicator
     * @param comm         [in/out] communicator ptr created
     * @param extraState   [in] extra state after bootstrap
     *
     * @return 0 if successful
     */
    static ZResult Create(const zbal_comm_options_t &options, zbal_comm_t *comm, const ZBALInitStateExt &extraState);

    /**
     * @brief Destroy on communicator
     *
     * @param comm         [in] communicator to be destroyed
     * @param flags        [in] optional flags
     *
     * @return 0 if successful
     */
    static ZResult Destroy(zbal_comm_t comm, uint32_t flags);

    /**
     * @brief Destroy all communicators
     */
    static void DestroyAll();

    /**
     * @brief Lookup communicator by name
     *
     * @param name         [in] name of the communicator
     * @param comm         [in/out] the communicator ptr found
     * @return 0 if successful
     */
    static ZResult Lookup(const std::string &name, zbal_comm_t *comm);

    /**
     * @brief Get the global communicator
     *
     * @param comm         [in/out] the commnicator ptr
     * @return 0 if successful or else error code
     */
    static ZResult GetGlobalComm(zbal_comm_t *comm);

    /**
     * @brief Lookup communicator property
     *
     * @param comm         [in] communicator
     * @param property     [out] property of communicator
     * @return 0 if successful or else error code
     */
    static ZResult GetCommProperty(const zbal_comm_t comm, zbal_comm_property_t *property);

    /**
     * @brief Get the count of communicators
     *
     * @return Count of existing communicators
     */
    static uint32_t Count();

    /**
     * @brief dump all group trace and meta
     */
    static void DumpAllComm();

public:
    Communicator(const CommGroupOptions &options, bool isWorldGroup, const CommunicatorPtr &worldGroup)
        : isWorldGroup_(isWorldGroup), worldGroup_(worldGroup), options_(options)
    {}

    ~Communicator() override = default;

    /**
     * @brief Initialize communicator
     *
     * @return 0 if successful
     */
    virtual ZResult Initialize() noexcept = 0;

    /**
     * @brief Un-initialize communicator
     */
    virtual void UnInitialize() noexcept = 0;

    /**
     * @brief construct comm from option
     */
    virtual void ConstructCommGroupInfo(const CommGroupOptions &opt) noexcept = 0;

    /**
     * @brief Dump profiling trace
     */
    virtual void DumpProfilingTrace() noexcept = 0;

    /**
     * @brief Dump comm trace info
     */
    virtual void SignalDumpTrace() noexcept = 0;

    /**
     * @brief Do allReduce operation
     *
     * @return 0 if successful
     */
    virtual int32_t AllReduce(const void *send_buff, void *recv_buff, void *buffer, size_t count, size_t buf_cnt,
                              zbal_datatype_t data_type, zbal_reduce_op_t op, aclrtStream stream) noexcept = 0;

    /**
     * @brief Do ReduceScatter operation
     *
     * @return 0 if successful
     */
    virtual int32_t ReduceScatter(const void *send_buff, void *recv_buff, size_t recv_count, zbal_datatype_t data_type,
                                  zbal_reduce_op_t op, aclrtStream stream) noexcept = 0;

    /**
     * @brief Do allGather operation
     *
     * @return 0 if successful
     */
    virtual int32_t AllGather(const void *send_buff, void *recv_buff, size_t send_count, zbal_datatype_t data_type,
                              aclrtStream stream) noexcept = 0;

    /**
     * @brief Do All2all operation
     *
     * @return 0 if successful
     */
    virtual int32_t AlltoAllBase(const void *sendBuff, void *recvBuff, uint64_t data_count, zbal_datatype_t dataType,
                                 aclrtStream stream) noexcept = 0;

    virtual int32_t AlltoAllV(const void *sendBuff, void *recvBuff, void *sendCumSum, void *recvSplitCounts,
                              void *elements, zbal_datatype_t dataType, aclrtStream s) noexcept = 0;

    /**
     * @brief Do Broadcast operation
     *
     * @return 0 if successful
     */
    virtual int32_t Broadcast(void *buf, uint64_t data_count, zbal_datatype_t dataType, uint16_t root,
                              aclrtStream stream) noexcept = 0;

    /**
     * @brief Do Scatter operation
     *
     * @return 0 if successful
     */
    virtual int32_t Scatter(const void *sendBuff, void *recvBuff, uint64_t data_count, zbal_datatype_t dataType,
                            uint16_t root, aclrtStream stream) noexcept = 0;

    /**
     * @brief Do barrier operation
     *
     * @return 0 if successful
     */
    virtual int32_t Barrier(aclrtStream stream) noexcept = 0;

    /**
     * @brief Point to point send operation
     *
     * @return 0 if successful
     */
    virtual int32_t Send(const void *sendBuff, size_t sendCount, zbal_datatype_t dataType, uint32_t peer,
                         aclrtStream stream) noexcept = 0;

    /**
     * @brief Point to point recv operation
     *
     * @return 0 if successful
     */
    virtual int32_t Recv(const void *recvBuff, size_t recvCount, zbal_datatype_t dataType, uint32_t peer,
                         aclrtStream stream) noexcept = 0;

    /**
     * @brief Do dispatch normal notify operation
     *
     * @return 0 if successful
     */
    virtual int32_t
    DispatchNormalNotify(const zbal_tensor_info_t *sendTokensPerExpert, int64_t sendCount, int64_t topKNum,
                         const zbal_tensor_info_t *recvBuff, const zbal_tensor_info_t *totalRecvTokens,
                         const zbal_tensor_info_t *recvTokensPerExpert, const zbal_tensor_info_t *pushTargetOffset,
                         const zbal_tensor_info_t *balanceMatrix, aclrtStream stream, int64_t flags) noexcept = 0;

    /**
     * @brief Dispatch normal layout
     *
     * @return 0 if successful
     */
    virtual int32_t
    DispatchNormalLayout(const zbal_tensor_info_t *topkIndex, int64_t tokens, int64_t expertNum, int64_t topkNum,
                         const zbal_tensor_info_t *tokensPerRank, const zbal_tensor_info_t *tokensPerExpert,
                         const zbal_tensor_info_t *isTokenInRank, const zbal_tensor_info_t *sendTokensIndex,
                         const zbal_tensor_info_t *notifySendData, aclrtStream stream, int64_t flags) noexcept = 0;

    /**
     * @brief Dispatch operation
     *
     * @return 0 if successful
     */
    virtual int32_t DispatchNormal(const zbal_tensor_info_t *srcTokens, const zbal_tensor_info_t *topkIndex,
                                   const zbal_tensor_info_t *sendTokensIndex,
                                   const zbal_tensor_info_t *pushTargetOffset, const zbal_tensor_info_t *balanceMatrix,
                                   int64_t expertNum, zbal_quant_mode_t quantMode, const zbal_tensor_info_t *destTokens,
                                   const zbal_tensor_info_t *destScale, aclrtStream stream, int64_t flags) noexcept = 0;

    /**
     * @brief Combine operation
     *
     * @return 0 if successful
     */
    virtual int32_t CombineNormal(const zbal_tensor_info_t *srcTokens, const zbal_tensor_info_t *srcTokensPerEp,
                                  const zbal_tensor_info_t *topKWeight, const zbal_tensor_info_t *topkIndex,
                                  const zbal_tensor_info_t *sendTokensIndex, const zbal_tensor_info_t *balanceMatrix,
                                  uint16_t expertNum, const zbal_tensor_info_t *destTokens, aclrtStream stream,
                                  int64_t flags) noexcept = 0;

    /**
     * @brief Dispatch low latency operation
     *
     * @return 0 if successful
     */
    virtual int32_t
    DispatchLowLatency(const zbal_tensor_info_t *x, const zbal_tensor_info_t *expertIds, int64_t moeExpertNum,
                       int64_t sharedExpertNum, int64_t sharedExpertRankNum, int64_t quantMode, int64_t globalBs,
                       int64_t magicVal, int64_t expertTokenNumsType, const zbal_tensor_info_t *expandXOut,
                       const zbal_tensor_info_t *dynamicScalesOut, const zbal_tensor_info_t *expandIdxOut,
                       const zbal_tensor_info_t *expertTokenNumsOut, const zbal_tensor_info_t *epRecvCountsOut,
                       const zbal_tensor_info_t *putOffset, const zbal_tensor_info_t *putOffsetStatus,
                       aclrtStream stream, int64_t flags) noexcept = 0;

    /**
     * @brief Combine low latency operation
     *
     * @return 0 if successful
     */
    virtual int32_t CombineLowLatency(const zbal_tensor_info_t *expandX, const zbal_tensor_info_t *expertIds,
                                      const zbal_tensor_info_t *expertIdx, const zbal_tensor_info_t *epSendCounts,
                                      const zbal_tensor_info_t *expertScales, const zbal_tensor_info_t *xOut,
                                      int64_t moeExpertNum, aclrtStream stream, int64_t flags) noexcept = 0;

    /**
     * @brief Assign group id and gathered group info
     *
     * @param id           [in] group id which contained gathered group info
     *
     * @return 0 if successful
     */
    virtual ZResult AssignGatherGroupId(AutoReleaseGroupId &id) noexcept = 0;

    /**
     * @brief Check if it is world group
     *
     * @return true if world group
     */
    bool IsWorldGroup() const noexcept;

    /**
     * @brief Group info of communicator, this will be passed to device in the param area of group meta
     *
     * @return meta info
     */
    const CommGroupInfo &GetMetaInfo() const noexcept;

    /**
     * @brief Get the name of communicator
     *
     * @return name string of communicator
     */
    const std::string &Name() const noexcept;

    /**
     * @brief Get group id of communicator
     *
     * @return group id
     */
    uint16_t GroupId() const noexcept;

protected:
    bool isWorldGroup_ = false;           /* if it is world group */
    AutoReleaseGroupId uniqueGroupId_;    /* unique group id */
    CommGroupOptions options_{};          /* options */
    CommGroupInfo groupInfo_{};           /* meta info, which will be H2D to device, keep it simple */
    CommunicatorPtr worldGroup_{nullptr}; /* world group */
    std::mutex mutex_;                    /* object mutex */
    bool initialized_{false};             /* flag for initialization */

private:
    static CommunicatorPtr CreateInner(zbal_backend_t backendType, const CommGroupOptions &options, bool isWorldGroup);
    static ZResult DestroyInner(CommunicatorPtr &comm);
    static void DestroyAllInner();
    static ZResult LookupInner(const std::string &name, CommunicatorPtr &comm);

    static CommunicatorPtr gWorldCommunicator;                          /* the world comm, i.e. the first one */
    static std::mutex gMutex;                                           /* mutex for world comm */
    static std::map<uintptr_t, CommunicatorPtr> gCommLookupMap;         /* all comm object except the world comm */
    static std::map<std::string, CommunicatorPtr> gCommLookupMapByName; /* all comm object */
};

inline bool Communicator::IsWorldGroup() const noexcept
{
    return isWorldGroup_;
}

inline const CommGroupInfo &Communicator::GetMetaInfo() const noexcept
{
    return groupInfo_;
}

inline const std::string &Communicator::Name() const noexcept
{
    return options_.name;
}

inline uint16_t Communicator::GroupId() const noexcept
{
    return uniqueGroupId_.Id();
}

} // namespace operators
} // namespace zbal

#endif // ZBAL_COMMUNICATOR_H
