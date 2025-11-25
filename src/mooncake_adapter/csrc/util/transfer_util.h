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
