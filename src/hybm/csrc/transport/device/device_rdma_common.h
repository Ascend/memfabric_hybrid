/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MF_HYBRID_DEVICE_RDMA_COMMON_H
#define MF_HYBRID_DEVICE_RDMA_COMMON_H

#include <arpa/inet.h>
#include <ostream>
#include <sstream>
#include <map>
#include "hybm_define.h"
#include "hybm_transport_common.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

#define container_of(ptr, type, member)                                              \
    ({                                                                               \
        const typeof(((const type *)0)->member) *__mptr = (ptr);                     \
        (const type *)(const void *)((const char *)__mptr - offsetof(type, member)); \
    })

// 注册内存结果结构体
struct RegMemResult {
    uint32_t type{TT_HCCP};
    uint32_t reserved{0};
    uint64_t address{0};
    uint64_t size{0};
    void *mrHandle{nullptr};
    uint32_t lkey{0};
    uint32_t rkey{0};

    RegMemResult() = default;

    RegMemResult(uint64_t addr, uint64_t sz, void *hd, uint32_t lk, uint32_t rk)
        : address(addr),
          size(sz),
          mrHandle(hd),
          lkey(lk),
          rkey(rk)
    {
    }
};

union RegMemKeyUnion {
    TransportMemoryKey commonKey;
    RegMemResult deviceKey;
};

using MemoryRegionMap = std::map<uint64_t, RegMemResult, std::greater<uint64_t>>;

struct ConnectRankInfo {
    hybm_role_type role;
    sockaddr_in network;
    MemoryRegionMap memoryMap;

    ConnectRankInfo(hybm_role_type r, sockaddr_in nw, const TransportMemoryKey &mk) : role{r}, network{std::move(nw)}
    {
        auto &deviceKey = container_of(&mk, RegMemKeyUnion, commonKey)->deviceKey;
        memoryMap.emplace(deviceKey.address, deviceKey);
    }

    ConnectRankInfo(hybm_role_type r, sockaddr_in nw, const std::vector<TransportMemoryKey> &mks)
        : role{r},
          network{std::move(nw)}
    {
        for (auto &mk : mks) {
            auto &deviceKey = container_of(&mk, RegMemKeyUnion, commonKey)->deviceKey;
            memoryMap.emplace(deviceKey.address, deviceKey);
        }
    }
};

inline std::ostream &operator<<(std::ostream &output, const RegMemResult &mr)
{
    output << "RegMemResult(size=" << mr.size << ")";
    return output;
}

inline std::ostream &operator<<(std::ostream &output, const MemoryRegionMap &map)
{
    for (auto it = map.rbegin(); it != map.rend(); ++it) {
        output << it->second << ", ";
    }
    return output;
}

inline std::ostream &operator<<(std::ostream &output, const HccpRaInitConfig &config)
{
    output << "HccpRaInitConfig(phyId=" << config.phyId << ", nicPosition=" << config.nicPosition
           << ", hdcType=" << config.hdcType << ")";
    return output;
}

inline std::ostream &operator<<(std::ostream &output, const HccpRdevInitInfo &info)
{
    output << "HccpRdevInitInfo(mode=" << info.mode << ", notify=" << info.notifyType
           << ", enabled910aLite=" << info.enabled910aLite << ", disabledLiteThread=" << info.disabledLiteThread
           << ", enabled2mbLite=" << info.enabled2mbLite << ")";
    return output;
}

inline std::ostream &operator<<(std::ostream &output, const HccpRdev &rdev)
{
    output << "HccpRdev(phyId=" << rdev.phyId << ", family=" << rdev.family
           << ", rdev.ip=" << inet_ntoa(rdev.localIp.addr) << ")";
    return output;
}

inline std::ostream &operator<<(std::ostream &output, const ai_data_plane_wq &info)
{
    output << "ai_data_plane_wq(wqn=" << info.wqn
           << ", buff_addr=" << static_cast<void *>(reinterpret_cast<void *>(info.buf_addr))
           << ", wqebb_size=" << info.wqebb_size << ", depth=" << info.depth
           << ", head=" << static_cast<void *>(reinterpret_cast<void *>(info.head_addr))
           << ", tail=" << static_cast<void *>(reinterpret_cast<void *>(info.tail_addr))
           << ", swdb_addr=" << static_cast<void *>(reinterpret_cast<void *>(info.swdb_addr))
           << ", db_reg=" << info.db_reg << ")";
    return output;
}

inline std::ostream &operator<<(std::ostream &output, const ai_data_plane_cq &info)
{
    output << "ai_data_plane_cq(cqn=" << info.cqn
           << ", buff_addr=" << static_cast<void *>(reinterpret_cast<void *>(info.buf_addr))
           << ", cqe_size=" << info.cqe_size << ", depth=" << info.depth
           << ", head=" << static_cast<void *>(reinterpret_cast<void *>(info.head_addr))
           << ", tail=" << static_cast<void *>(reinterpret_cast<void *>(info.tail_addr))
           << ", swdb_addr=" << static_cast<void *>(reinterpret_cast<void *>(info.swdb_addr))
           << ", db_reg=" << info.db_reg << ")";
    return output;
}

inline std::ostream &operator<<(std::ostream &output, const HccpAiQpInfo &info)
{
    output << "HccpAiQpInfo(addr=" << static_cast<void *>(reinterpret_cast<void *>(info.aiQpAddr))
           << ", sq_index=" << info.sqIndex << ", db_index=" << info.dbIndex
           << ", ai_scq_addr=" << static_cast<void *>(reinterpret_cast<void *>(info.ai_scq_addr))
           << ", ai_rcq_addr=" << static_cast<void *>(reinterpret_cast<void *>(info.ai_rcq_addr))
           << ", data_plane_info:<sq=" << info.data_plane_info.sq << ", rq=" << info.data_plane_info.rq
           << ", scq=" << info.data_plane_info.scq << ", rcq=" << info.data_plane_info.rcq << ">)";
    return output;
}

inline std::ostream &operator<<(std::ostream &output, const AiQpRMAWQ &info)
{
    output << "AiQpRMAWQ(wqn=" << info.wqn
           << ", buff_addr=" << static_cast<void *>(reinterpret_cast<void *>(info.bufAddr))
           << ", wqe_size=" << info.wqeSize << ", depth=" << info.depth
           << ", head=" << static_cast<void *>(reinterpret_cast<void *>(info.headAddr))
           << ", tail=" << static_cast<void *>(reinterpret_cast<void *>(info.tailAddr))
           << ", db_mode=" << static_cast<int>(info.dbMode)
           << ", db_addr=" << static_cast<void *>(reinterpret_cast<void *>(info.dbAddr)) << ", sl=" << info.sl << ")";
    return output;
}

inline std::ostream &operator<<(std::ostream &output, const AiQpRMACQ &info)
{
    output << "AiQpRMACQ(cqn=" << info.cqn
           << ", buff_addr=" << static_cast<void *>(reinterpret_cast<void *>(info.bufAddr))
           << ", cqe_size=" << info.cqeSize << ", depth=" << info.depth
           << ", head=" << static_cast<void *>(reinterpret_cast<void *>(info.headAddr))
           << ", tail=" << static_cast<void *>(reinterpret_cast<void *>(info.tailAddr))
           << ", db_mode=" << static_cast<int>(info.dbMode)
           << ", db_addr=" << static_cast<void *>(reinterpret_cast<void *>(info.dbAddr)) << ")";
    return output;
}

inline std::ostream &operator<<(std::ostream &output, const RdmaMemRegionInfo &info)
{
    output << "RdmaMemRegionInfo(size=" << info.size << ")";
    return output;
}

inline std::string AiQpInfoToString(const AiQpRMAQueueInfo &info, uint32_t rankCount)
{
    std::stringstream ss;
    ss << "QiQpInfo(rankCount=" << rankCount << ", mq_count=" << info.count << ")={\n";

    for (uint32_t i = 0; i < rankCount; ++i) {
        ss << "  rank" << i << "={\n";

        for (uint32_t j = 0; j < info.count; ++j) {
            const uint32_t idx = i * info.count + j;
            ss << "    qp" << j << "_info={\n";
            ss << "      sq=<" << info.sq[idx] << ">\n";
            ss << "      rq=<" << info.rq[idx] << ">\n";
            ss << "      scq=<" << info.scq[idx] << ">\n";
            ss << "      rcq=<" << info.rcq[idx] << ">\n";
            ss << "    }\n";
        }

        ss << "    MR-rank-" << i << "=<" << info.mr[i] << ">\n";
        ss << "  }\n";
    }

    ss << "}";
    return ss.str();
}

inline std::ostream &operator<<(std::ostream &output, const HccpSocketConnectInfo &info)
{
    output << "HccpSocketConnectInfo(socketHandle=" << info.handle << ", remoteIp=" << inet_ntoa(info.remoteIp.addr)
           << ", port=" << info.port << ")";
    return output;
}

}
}
}
}

#endif  // MF_HYBRID_DEVICE_RDMA_COMMON_H
