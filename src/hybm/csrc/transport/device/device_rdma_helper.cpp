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
#include <arpa/inet.h>
#include <cstdlib>
#include <sstream>
#include <regex>
#include <limits>
#include "mf_str_util.h"
#include "mf_num_util.h"
#include "hybm_logger.h"
#include "device_rdma_helper.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
Result ParseDeviceNic(const std::string &nic, uint16_t &port)
{
    static const std::regex pattern(R"(^[a-zA-Z0-9_]{1,16}://[0-9./]{0,24}:(\d{1,5})$)");

    std::smatch match;
    if (!std::regex_search(nic, match, pattern)) {
        BM_LOG_ERROR("input nic(" << nic << ") not matches.");
        return BM_INVALID_PARAM;
    }

    auto caught = match[1].str();
    auto parsePort = std::strtol(caught.c_str(), nullptr, 10);
    if (parsePort <= 0 || parsePort > std::numeric_limits<uint16_t>::max()) {
        BM_LOG_ERROR("input nic(" << nic << ") not matches, port(" << parsePort << ") too large.");
        return BM_INVALID_PARAM;
    }

    port = static_cast<uint16_t>(parsePort);
    return BM_OK;
}

Result ParseDeviceNic(const std::string &nic, sockaddr_in &address)
{
    static const std::regex pattern(R"(^[a-zA-Z0-9_]{1,16}://([0-9.]{1,24}):(\d{1,5})$)");

    std::smatch match;
    if (!std::regex_search(nic, match, pattern)) {
        BM_LOG_ERROR("input nic(" << nic << ") not matches.");
        return BM_INVALID_PARAM;
    }

    if (inet_aton(match[1].str().c_str(), &address.sin_addr) == 0) {
        BM_LOG_ERROR("parse ip for nic: " << nic << " failed.");
        return BM_INVALID_PARAM;
    }

    auto caught = match[2].str();
    auto parsePort = std::strtol(caught.c_str(), nullptr, 10);
    if (parsePort <= 0 || parsePort > std::numeric_limits<uint16_t>::max()) {
        BM_LOG_ERROR("input nic(" << nic << ") not matches, port(" << parsePort << ") too large.");
        return BM_INVALID_PARAM;
    }

    address.sin_port = static_cast<uint16_t>(parsePort);
    address.sin_family = AF_INET;
    return BM_OK;
}

std::string GenerateDeviceNic(in_addr ip, uint16_t port)
{
    std::stringstream ss;
    char host[INET_ADDRSTRLEN];
    auto ret = inet_ntop(AF_INET, &ip, host, INET_ADDRSTRLEN);
    if (ret == nullptr) {
        BM_LOG_ERROR("inet_ntop failed, " << strerror(errno));
        return "";
    }
    ss << "tcp://" << host << ":" << port;
    return ss.str();
}
} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock