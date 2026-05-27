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
#ifndef ZBAL_COMM_TYPES_H
#define ZBAL_COMM_TYPES_H

#include "zbal_common_includes.h"
#include "zbal_comm_host_device_struct.h"

namespace zbal {
namespace operators {

class Communicator;
class GroupMetaArranger;
class GroupInfoExchanger;

using CommunicatorPtr = ZRef<Communicator>;

struct CommGroupOptions {
    std::string name;                    /* name of group */
    uint16_t worldSize = 0;              /* the ranks in the world */
    uint16_t groupSize = 0;              /* the ranks in the group */
    uint16_t myWorldRank = 0;            /* rank id in the world */
    uint16_t myGroupRank = 0;            /* rank id in the group */
    uint32_t dataOpType = 0;             /* data operation type, see zbal_data_op_type_t */
    void *gva = nullptr;                 /* gva of the world */
    uint64_t metaSize = 0;               /* size of meta */
    uintptr_t myMetaGva = 0;             /* gva of mine */
    uintptr_t myParamDataGva = 0;        /* gva of for param exchange of operation */
    uintptr_t myAddressExchangeGva = 0;  /* gva of for param exchange of operation */
    uint64_t sizeForCommGroupInfo = 0;   /* max memory size of passing param from host to device */
    uint64_t sizeForParam = 0;           /* max memory size of passing param from host to device */
    uint64_t sizeForExchangeAddress = 0; /* max memory size for exchange operation data addresses */
    uint16_t deviceId = 0;               /* device Id */
    uint32_t groupIndex = 0;             /* group index */
    uint64_t fftsConfig = 0;             /* ffts config for operator in inner option*/
    uint64_t localDeviceMemSize = 0;     /* local device memory size */

    friend std::ostream &operator<<(std::ostream &os, const CommGroupOptions &options)
    {
        os << "CommGroupOptions [name: " << options.name << ", worldSize: " << options.worldSize
           << ", groupSize: " << options.groupSize << ", myWorldRank: " << options.myWorldRank
           << ", myGroupRank: " << options.myGroupRank << ", dataOpType: " << options.dataOpType
           << ", gva: " << options.gva << ", metaSize: " << options.metaSize
           << ", myMetaGva: " << std::hex << options.myMetaGva << ", myParamDataGva: " << options.myParamDataGva
           << ", myAddressExchangeGva: " << options.myAddressExchangeGva << std::dec
           << ", sizeForCommGroupInfo: " << options.sizeForCommGroupInfo << ", sizeForParam: " << options.sizeForParam
           << ", sizeForExchangeAddress: " << options.sizeForExchangeAddress << ", deviceId: " << options.deviceId
           << ", groupIndex: " << options.groupIndex << ", fftsConfig: " << options.fftsConfig
           << ", localDeviceMemSize: " << options.localDeviceMemSize << "]";

        return os;
    }
};

struct CommGroupExchangeInfo {
    uint16_t groupId = UINT16_MAX; /* group id */
    uint16_t myWorldRankId = 0;    /* rank id in the world */
    uint16_t myGroupRankId = 0;    /* rank id in the group */

    friend std::ostream &operator<<(std::ostream &os, const CommGroupExchangeInfo &info)
    {
        os << "CommGroupExchangeInfo [groupId: " << info.groupId << ", myWorldRankId: " << info.myWorldRankId
           << ", myGroupRankId: " << info.myGroupRankId << "]";

        return os;
    }
};

static inline std::ostream &operator<<(std::ostream &os, const CommGroupInfo &info)
{
    os << "CommGroupInfo [groupSize: " << info.groupSize << ", myGroupRank: " << info.myGroupRank
       << ", myMetaGva: " << info.myMetaGva << ", myParamDataGva: " << info.myParamDataGva
       << ", myAddressExchangeGva: " << info.myAddressExchangeGva
       << ", sizeForCommGroupInfo: " << info.sizeForCommGroupInfo << ", sizeForParam: " << info.sizeForParam
       << ", sizeForExchangeAddress: " << info.sizeForExchangeAddress << ", fftsConfig: " << info.fftsConfig
       << ", localDeviceMemSize: " << info.localDeviceMemSize << ", peerGroupRank2WorldRank: [";

    auto size = std::min(info.groupSize + 1, ZBAL_MAX_RANKS);
    for (auto i = 0; i < size; ++i) {
        os << info.peerGroupRank2WorldRank[i] << ",";
    }

    os << "]]";

    return os;
}

} // namespace operators
} // namespace zbal

#endif // ZBAL_COMM_TYPES_H
