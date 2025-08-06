/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "acc_common_util.h"
#include "acc_includes.h"
#include "acc_tcp_client_default.h"

namespace ock {
namespace acc {
AccTcpClientPtr AccTcpClient::Create(const std::string &serverIp, uint16_t serverPort)
{
    if (!AccCommonUtil::IsValidIPv4(serverIp) || serverPort == 0) {
        LOG_ERROR("Failed to create AccTcpClient, invalid ip address or port");
        return nullptr;
    }

    auto client = AccMakeRef<AccTcpClientDefault>(serverIp, serverPort);
    if (client.Get() == nullptr) {
        LOG_ERROR("Failed to create AccTcpClientDefault, probably out of memory");
        return nullptr;
    }

    return client.Get();
}
}  // namespace ock
}  // namespace cled