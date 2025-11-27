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

static int BindTcpPortV4(int &sockfd, int port)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd != -1) {
        int on_v4 = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on_v4, sizeof(on_v4)) == 0) {
            sockaddr_in bind_address_v4{};
            bind_address_v4.sin_family = AF_INET;
            bind_address_v4.sin_port = htons(port);
            bind_address_v4.sin_addr.s_addr = INADDR_ANY;

            if (bind(sockfd, reinterpret_cast<sockaddr*>(&bind_address_v4), sizeof(bind_address_v4)) == 0) {
                return 0;
            }
        }
        close(sockfd);
        sockfd = -1;
    }
    return -1;
}

static int BindTcpPortV6(int &sockfd, int port)
{
    sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd != -1) {
        int on_v6 = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on_v6, sizeof(on_v6)) == 0) {
            sockaddr_in6 bind_address_v6{};
            bind_address_v6.sin6_family = AF_INET6;
            bind_address_v6.sin6_port = htons(port);
            bind_address_v6.sin6_addr = in6addr_any;

            if (bind(sockfd, reinterpret_cast<sockaddr*>(&bind_address_v6), sizeof(bind_address_v6)) == 0) {
                return 0;
            }
        }
        close(sockfd);
        sockfd = -1;
    }
    return -1;
}

uint16_t findAvailableTcpPort(int &sockfd)
{
    static std::random_device rd;
    const int min_port = 15000;
    const int max_port = 25000;
    const int max_attempts = 1000;
    const int offset_bit = 32;
    uint64_t seed = 1;
    seed |= static_cast<uint64_t>(getpid()) << offset_bit;
    seed |= static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()) & 0xFFFFFFFFULL;
    static std::mt19937_64 gen(seed);
    std::uniform_int_distribution<> dis(min_port, max_port);

    bool supports_ipv6 = false;
    int sockfd_check = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd_check != -1) {
        supports_ipv6 = true;
        close(sockfd_check);
    }

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        int port = dis(gen);
        auto ret = BindTcpPortV4(sockfd, port);
        if (ret == 0) {
            return port;
        }
        if (supports_ipv6) {
            ret = BindTcpPortV6(sockfd, port);
            if (ret == 0) {
                return port;
            }
        }
    }
    ADAPTER_LOG_ERROR("Not find a available tcp port");
    return 0;
}

int32_t pytransfer_create_config_store(const char *storeUrl)
{
    const char *shmem_level = std::getenv("SHMEM_LOG_LEVEL");
    const char* mf_level = std::getenv("ASCEND_MF_LOG_LEVEL");
    if (shmem_level == nullptr && mf_level != nullptr && strlen(mf_level) == 1) {
        unsigned char c = static_cast<unsigned char>(mf_level[0]);
        if (std::isdigit(c)) {
            int level = c - '0';
            smem_set_log_level(level);
        }
    }
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