/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <arpa/inet.h>
#include <cstdlib>
#include <cerrno>
#include <sstream>
#include <regex>
#include <limits>
#include "hybm_logger.h"
#include "mf_string_util.h"
#include "mf_num_util.h"
#include "device_rdma_helper.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
Result ParseDeviceNic(const std::string &nic, uint16_t &port)
{
    if (!StringUtil::String2Uint(nic, port) || port == 0) {
        BM_LOG_ERROR("failed to convert nic : " << nic << " to uint16_t, or port is 0.");
        return BM_INVALID_PARAM;
    }
    return BM_OK;
}

Result ParseDeviceNic(const std::string &nic, sockaddr_in &address)
{
    static std::regex pattern(R"(^[a-zA-Z0-9_]{1,16}://([0-9.]{1,24}):(\d{1,5})$)");

    std::smatch match;
    if (!std::regex_search(nic, match, pattern)) {
        BM_LOG_ERROR("input nic(" << nic << ") not matches.");
        return BM_INVALID_PARAM;
    }

    if (inet_aton(match[INDEX_1].str().c_str(), &address.sin_addr) == 0) {
        BM_LOG_ERROR("parse ip for nic: " << nic << " failed.");
        return BM_INVALID_PARAM;
    }

    auto caught = match[INDEX_2].str();
    if (!StringUtil::String2Uint(caught, address.sin_port)) {
        BM_LOG_ERROR("failed to convert str : " << caught << " to uint16_t, or sin_port is 0.");
        return BM_INVALID_PARAM;
    }

    address.sin_family = AF_INET;
    return BM_OK;
}

std::string GenerateDeviceNic(in_addr ip, uint16_t port)
{
    std::stringstream ss;
    ss << "tcp://" << inet_ntoa(ip) << ":" << port;
    return ss.str();
}
}
}
}
}