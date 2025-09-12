/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_NET_LINK_ACC_H
#define MEM_FABRIC_MMC_NET_LINK_ACC_H

#include "mmc_net_engine.h"
#include "mmc_net_common_acc.h"

namespace ock {
namespace mmc {
class NetLinkAcc final : public NetLink {
public:
    NetLinkAcc(int32_t id, const TcpLinkPtr &tcpLink) : id_(id), tcpLink_(tcpLink) {}
    ~NetLinkAcc() override = default;

    int32_t Id() const override;
    const TcpLinkPtr &RealLink() const;

private:
    const int32_t id_;
    const TcpLinkPtr tcpLink_;
};

inline int32_t NetLinkAcc::Id() const
{
    return id_;
}
inline const TcpLinkPtr &NetLinkAcc::RealLink() const
{
    return tcpLink_;
}
}
}

#endif  //MEM_FABRIC_MMC_NET_LINK_ACC_H
