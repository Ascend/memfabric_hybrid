/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_META_PROXY_H
#define MEM_FABRIC_MMC_META_PROXY_H

#include "mmc_msg_client_meta.h"

namespace ock {
namespace mmc {

/* interface */
class MmcMetaMgrProxy : public MmcReferable {
public:
    virtual ~MmcMetaMgrProxy() = default;

    virtual Result Alloc(const AllocRequest &allocReq, AllocResponse &allocResp) = 0;

    virtual Result UpdateState(const UpdateRequest &updateReq, Response &upddateResp) = 0;

    virtual Result Get(const GetRequest &updateReq, AllocResponse &upddateResp) = 0;

    virtual Result Remove(const RemoveRequest &updateReq, Response &upddateResp) = 0;

    virtual Result Mount(const MmcLocation &loc, const MmcLocalMemlInitInfo &localMemInitInfo) = 0;

    virtual Result Unmount(const MmcLocation &loc) = 0;

    virtual Result ExistKey(const IsExistRequest &updateReq, Response &upddateResp) = 0;

    virtual Result BatchExistKey(const BatchIsExistRequest &updateReq, BatchIsExistResponse &upddateResp) = 0;
};
using MmcMetaMgrProxyPtr = MmcRef<MmcMetaMgrProxy>;
}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_META_PROXY_H
