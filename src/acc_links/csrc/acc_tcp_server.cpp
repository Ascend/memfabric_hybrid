/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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