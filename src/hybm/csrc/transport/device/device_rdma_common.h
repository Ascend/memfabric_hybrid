/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#ifndef MF_HYBRID_DEVICE_RDMA_COMMON_H
#define MF_HYBRID_DEVICE_RDMA_COMMON_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
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
    uint64_t address;
    uint64_t size;
    uint64_t regAddress;
    void *mrHandle;
    uint32_t lkey;
    uint32_t rkey;

    uint32_t type;
    uint32_t notifyRkey = 0;
    uint64_t notifyAddr = 0;

    RegMemResult() : RegMemResult{0, 0, nullptr, 0, 0} {}

    RegMemResult(uint64_t addr, uint64_t sz, void *hd, uint32_t lk, uint32_t rk)
        : type{TT_HCCP}, address(addr), size(sz), regAddress{addr}, mrHandle(hd), lkey(lk), rkey(rk)
    {}

    RegMemResult(uint64_t addr, uint64_t regAddr, uint64_t sz, void *hd, uint32_t lk, uint32_t rk)
        : type{TT_HCCP}, address(addr), size(sz), regAddress{regAddr}, mrHandle(hd), lkey(lk), rkey(rk)
    {}
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
        : role{r}, network{std::move(nw)}
    {
        for (auto &mk : mks) {
            auto &deviceKey = container_of(&mk, RegMemKeyUnion, commonKey)->deviceKey;
            memoryMap.emplace(deviceKey.address, deviceKey);
        }
    }
};

inline std::ostream &operator<<(std::ostream &output, const RegMemResult &mr)
{
    output << "RegMemResult(size=" << mr.size << ", mrHandle=" << mr.mrHandle << ", lkey=" << mr.lkey
           << ", rkey=" << mr.rkey << ")";
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

inline std::string DescribeIPv4(const struct in_addr &addr)
{
    char str[INET_ADDRSTRLEN];
    auto ret = inet_ntop(AF_INET, &addr, str, INET_ADDRSTRLEN);
    if (ret == nullptr) {
        return "<Invalid IP>";
    }
    return str;
}

inline std::ostream &operator<<(std::ostream &output, const HccpRdev &rdev)
{
    output << "HccpRdev(phyId=" << rdev.phyId << ", family=" << rdev.family
           << ", rdev.ip=" << DescribeIPv4(rdev.localIp.addr) << ")";
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
    output << "RdmaMemRegionInfo(size=" << info.size
           << ", addr=" << static_cast<void *>(reinterpret_cast<void *>(info.addr)) << ", lkey=" << info.lkey
           << ", rkey=" << info.rkey << ")";
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
    output << "HccpSocketConnectInfo(socketHandle=" << info.handle << ", remoteIp=" << DescribeIPv4(info.remoteIp.addr)
           << ", port=" << info.port << ")";
    return output;
}

struct RdmaNotifyInfo {
    uint64_t srcAddr;
    uint64_t notifyAddr;
    uint32_t len;
    uint32_t srcRkey;
    uint32_t notifyLkey;
};

} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock

#endif // MF_HYBRID_DEVICE_RDMA_COMMON_H