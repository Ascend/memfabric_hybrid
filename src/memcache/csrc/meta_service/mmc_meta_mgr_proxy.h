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
    ~MmcMetaMgrProxy() override = default;

    virtual Result Alloc(const AllocRequest &req, AllocResponse &resp) = 0;

    virtual Result UpdateState(const UpdateRequest &req, Response &resp) = 0;

    virtual Result Get(const GetRequest &req, AllocResponse &resp) = 0;

    virtual Result Remove(const RemoveRequest &req, Response &resp) = 0;

    virtual Result Mount(const MmcLocation &loc, const MmcLocalMemlInitInfo &localMemInitInfo) = 0;

    virtual Result Unmount(const MmcLocation &loc) = 0;

    virtual Result ExistKey(const IsExistRequest &req, IsExistResponse &resp) = 0;

    virtual Result BatchRemove(const BatchRemoveRequest &req, BatchRemoveResponse &resp) = 0;

    virtual Result BatchExistKey(const BatchIsExistRequest &req, BatchIsExistResponse &resp) = 0;

    virtual Result BatchGet(const BatchGetRequest &req, BatchAllocResponse &resp) = 0;

    virtual Result Query(const QueryRequest &req, QueryResponse &resp) = 0;

    virtual Result BatchQuery(const BatchQueryRequest &req, BatchQueryResponse &resp) = 0;
};
using MmcMetaMgrProxyPtr = MmcRef<MmcMetaMgrProxy>;
}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_META_PROXY_H
