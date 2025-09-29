/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <cstring>
#include <random>
#include <stdexcept>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

#include "adapter_logger.h"
#include "smem.h"
#include "transfer_util.h"

namespace ock {
namespace adapter {

uint16_t findAvailableTcpPort(int &sockfd)
{
    static std::random_device rd;
    const int min_port = 15000;
    const int max_port = 25000;
    const int max_attempts = 1000;
    const int offset_bit = 32;
    uint64_t seed = 1;
    seed |= static_cast<uint64_t>(getpid()) << offset_bit;
    seed |= static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count() & 0xFFFFFFFF);
    static std::mt19937_64 gen(seed);
    std::uniform_int_distribution<> dis(min_port, max_port);

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        int port = dis(gen);
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            continue;
        }

        int on = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
            close(sockfd);
            sockfd = -1;
            continue;
        }

        sockaddr_in bind_address{};
        bind_address.sin_family = AF_INET;
        bind_address.sin_port = htons(port);
        bind_address.sin_addr.s_addr = INADDR_ANY;
        
        if (bind(sockfd, reinterpret_cast<sockaddr*>(&bind_address), sizeof(bind_address)) == 0) {
            return port;
        }

        close(sockfd);
        sockfd = -1;
    }
    ADAPTER_LOG_ERROR("Not find a available tcp port");
    return 0;
}

int32_t pytransfer_create_config_store(const char *storeUrl)
{
    // default: disable tls
    smem_set_conf_store_tls(false, nullptr, 0);
    int ret = smem_create_config_store(storeUrl);
    if (ret != 0) {
        ADAPTER_LOG_ERROR("SMEM API smem_create_config_store happen error, ret=" << ret);
    }
    return ret;
}

int32_t pytransfer_set_log_level(int level)
{
    int ret = smem_set_log_level(level);
    if (ret != 0) {
        ADAPTER_LOG_ERROR("SMEM API smem_set_log_level happen error, ret=" << ret);
        return ret;
    }
    return 0;
}

int32_t pytransfer_set_conf_store_tls(bool enable, std::string &tls_info)
{
    return smem_set_conf_store_tls(enable, tls_info.c_str(), tls_info.size());
}

}  // namespace adapter
}  // namespace ock