/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "acc_common_util.h"
#include "acc_includes.h"
#include "acc_tcp_server_default.h"

namespace ock {
namespace acc {
AccTcpServerPtr AccTcpServer::Create()
{
    auto server = AccMakeRef<AccTcpServerDefault>();
    if (server.Get() == nullptr) {
        LOG_ERROR("Failed to create AccTcpserverDefault, probably out of memory");
        return nullptr;
    }
    return server.Get();
}
}  // namespace acc
}  // namespace ock