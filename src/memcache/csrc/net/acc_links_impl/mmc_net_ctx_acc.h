/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MMC_NET_CTX_ACC_H
#define MMC_NET_CTX_ACC_H

#include "mmc_net_engine.h"
#include "mmc_net_common_acc.h"

namespace ock {
namespace mmc {
class NetContextAcc final : public NetContext {
public:
    explicit NetContextAcc(const TcpReqContext &ctx) : realContext(ctx) {}

    int32_t Reply(int16_t responseCode, char *respData, uint32_t &respDataLen) override;

    uint32_t SeqNo() const override;

    int16_t OpCode() const override;

    int16_t SrcRankId() const override;

    uint32_t DataLen() const override;

    void *Data() const override;

private:
    const TcpReqContext realContext;
};
using NetContextAccPtr = MmcRef<NetContextAcc>;

inline int32_t NetContextAcc::Reply(int16_t responseCode, char *respData, uint32_t &respDataLen)
{
    /* step2: copy data */
    TcpDataBufPtr dataBuf = new (std::nothrow)ock::acc::AccDataBuffer(respDataLen);
    MMC_ASSERT_RETURN(dataBuf.Get() != nullptr, MMC_NEW_OBJECT_FAILED);
    MMC_ASSERT_RETURN(dataBuf->AllocIfNeed(), MMC_NEW_OBJECT_FAILED);
    memcpy(dataBuf->DataPtrVoid(), static_cast<void *>(const_cast<char *>(respData)), respDataLen);
    dataBuf->SetDataSize(respDataLen);
    return realContext.Reply(responseCode, dataBuf);
}

inline uint32_t NetContextAcc::SeqNo() const
{
    return realContext.Header().seqNo;
}

inline int16_t NetContextAcc::OpCode() const
{
    return realContext.Header().result;
}

inline int16_t NetContextAcc::SrcRankId() const
{
    return 0;
}

inline uint32_t NetContextAcc::DataLen() const
{
    return realContext.DataLen();
}

inline void *NetContextAcc::Data() const
{
    return realContext.DataPtr();
}
}
}

#endif  // MMC_NET_CTX_ACC_H
