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
#ifndef ZBAL_COMM_GROUP_META_H
#define ZBAL_COMM_GROUP_META_H

#include "zbal_common_includes.h"
#include "zbal_comm_host_device_struct.h"

namespace zbal {
namespace operators {
class GroupMetaArranger {
public:
    static GroupMetaArranger &Instance()
    {
        static GroupMetaArranger gInstance;
        return gInstance;
    }

public:
    GroupMetaArranger() = default;
    ~GroupMetaArranger() = default;

    /**
     * @brief Initialize arranger
     *
     * @param extraState
     *
     * @return 0 if successful
     */
    ZResult Initialize(const ZBALInitStateExt &extraState) noexcept;

    /**
     * @brief Un-initialize arranger, reset anything to 0, include position index (i.e. gGroupIndex)
     */
    void UnInitialize() noexcept;

    /**
     * @brief Get total meta space size in bytes of single communicator (i.e. single process group for pytorch)
     */
    uint64_t GetSingleMetaSpaceSize() const noexcept;

    /**
     * @brief Get size of comm group info of single communicator
     */
    uint64_t GetCommGroupInfoSpaceSize() const noexcept;

    /**
     * @brief Get space size for param exchange of single communicator
     * @return
     */
    uint64_t GetParamSpaceSize() const noexcept;

    /**
     * @brief Get space size for exchanging addresses of tensors, of single communicator
     */
    uint64_t GetExchangeSpaceSize() const noexcept;

    /**
     * @brief Get index and space address at current position, here we don't move to next position
     *
     * @param index                [in/out]
     * @param groupMetaGVA         [in/out]
     * @param paramGVA             [in/out]
     * @param addressExchangeGVA   [in/out]
     *
     * @return 0 if successful, error if no more position
     */
    ZResult CurrentGroup(uint32_t &index, uintptr_t &groupMetaGVA, uintptr_t &paramGVA,
                         uintptr_t &addressExchangeGVA) noexcept;

    /**
     * @brief Move to next position, called created communicator successfully
     */
    void Move2NextGroup() noexcept;

    /**
     * @brief Get addresses by index
     *
     * @param index                [in/out]
     * @param groupMetaGVA         [in/out]
     * @param paramGVA             [in/out]
     * @param addressExchangeGVA   [in/out]
     *
     * @return 0 if successful, error if no more position
     */
    ZResult GetGroupByIndex(uint32_t index, uintptr_t &groupMetaGVA, uintptr_t &paramGVA,
                            uintptr_t &addressExchangeGVA) noexcept;

    /**
     * @brief Check if initialized
     *
     * @return true if initialized successfully
     */
    bool Initialized() const noexcept;

private:
    ZResult Verify() noexcept;

private:
    uintptr_t myMetaGVA_ = 0;          /* meta GVA of communicator */
    uint64_t totalMetaSpaceSize_ = 0;  /* total meta space size of all communicators */
    uint64_t singleMetaSpaceSize_ = 0; /* meta space size of one communicator */
    uint16_t commGroupCap_ = 0;        /* max number of communicators */

private:
    static std::atomic<uint32_t> gGroupIndex;
};

inline uint64_t GroupMetaArranger::GetSingleMetaSpaceSize() const noexcept
{
    return singleMetaSpaceSize_;
}

inline uint64_t GroupMetaArranger::GetCommGroupInfoSpaceSize() const noexcept
{
    return sizeof(CommGroupInfo);
}

inline uint64_t GroupMetaArranger::GetParamSpaceSize() const noexcept
{
    return OPERATE_PARAM_SIZE - sizeof(CommGroupInfo);
}

inline uint64_t GroupMetaArranger::GetExchangeSpaceSize() const noexcept
{
    return singleMetaSpaceSize_ - OPERATE_PARAM_SIZE;
}

inline bool GroupMetaArranger::Initialized() const noexcept
{
    return myMetaGVA_ != 0;
}
} // namespace operators
} // namespace zbal

#endif // ZBAL_COMM_GROUP_META_H
