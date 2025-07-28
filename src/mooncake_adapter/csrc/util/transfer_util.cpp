/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Create Date : 2025
 */
#include <cstring>
#include <random>
#include <stdexcept>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

#include "adapter_logger.h"
#include "transfer_util.h"
#include "smem.h"

namespace ock {
namespace adapter {

uint16_t findAvailableTcpPort()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    const int min_port = 15000;
    const int max_port = 17000;
    const int max_attempts = 500;
    std::uniform_int_distribution<> dis(min_port, max_port);

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        int port = dis(gen);
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            continue;
        }

        int on = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
            close(sockfd);
            continue;
        }

        sockaddr_in bind_address{};
        bind_address.sin_family = AF_INET;
        bind_address.sin_port = htons(port);
        bind_address.sin_addr.s_addr = INADDR_ANY;
        
        if (bind(sockfd, reinterpret_cast<sockaddr*>(&bind_address), sizeof(bind_address)) == 0) {
            close(sockfd);
            return port;
        }

        close(sockfd);
    }
    ADAPTER_LOG_ERROR("Not find a available tcp port");
    return 0;
}

int32_t PyTransferConfigStore(const char *storeUrl)
{
    int ret = smem_create_config_store(storeUrl);
    if (ret != 0) {
        ADAPTER_LOG_ERROR("SMEM API smem_create_config_store happen error, ret=" << ret);
    }
    return ret;
}

int32_t PyTransferSetLogLevel(int level)
{
    int ret = smem_set_log_level(level);
    if (ret != 0) {
        ADAPTER_LOG_ERROR("SMEM API smem_set_log_level happen error, ret=" << ret);
        return ret;
    }
    ock::adapter::AdapterOutLogger::Instance().SetLogLevel(static_cast<LogLevel>(level));
    return 0;
}


}  // namespace adapter
}  // namespace ock