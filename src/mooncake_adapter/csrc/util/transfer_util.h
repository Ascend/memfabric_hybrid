/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef PYTRANSFER_UITL_H
#define PYTRANSFER_UITL_H

#include <cstdint>

namespace ock {
namespace adapter {

#define STR(x) #x
#define STR2(x) STR(x)

uint16_t findAvailableTcpPort(int &sockfd);

int32_t pytransfer_create_config_store(const char *storeUrl);

int32_t pytransfer_set_log_level(int level);

int32_t pytransfer_set_conf_store_tls(bool enable, std::string &tls_info);

}  // namespace adapter
}  // namespace ock
#endif // PYTRANSFER_UITL_H
