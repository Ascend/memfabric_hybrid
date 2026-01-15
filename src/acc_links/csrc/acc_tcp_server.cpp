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
} // namespace acc
} // namespace ock