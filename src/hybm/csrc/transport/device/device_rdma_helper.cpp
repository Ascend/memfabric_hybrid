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
#include <limits>
#include "dl_acl_api.h"
#include "mf_str_util.h"
#include "mf_ipv4_validator.h"
#include "mf_num_util.h"
#include "hybm_logger.h"
#include "device_rdma_helper.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

constexpr uint32_t ACL_STREAM_FAST_LAUNCH = 1U;
constexpr uint32_t ACL_STREAM_FAST_SYNC = 2U;

static bool IsValidNicUrlFormat(const std::string &ip, const std::string &protocol)
{
    constexpr std::size_t protocolMinLen = 4;  // "x://"
    constexpr std::size_t protocolMaxLen = 19; // 16 prefix + "://"
    constexpr std::size_t protocolSepLen = 3;  // "://"
    // Check protocol prefix: 1-16 chars of [a-zA-Z0-9_]
    if (protocol.size() < protocolMinLen || protocol.size() > protocolMaxLen) {
        return false;
    }
    // protocol includes "://", check only the prefix part
    std::size_t prefixLen = protocol.size() - protocolSepLen; // subtract "://"
    for (std::size_t i = 0; i < prefixLen; ++i) {
        char c = protocol[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            return false;
        }
    }
    // Check IP part: only digits and dots allowed
    for (char c : ip) {
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.') {
            return false;
        }
    }
    return true;
}

Result ParseDeviceNic(const std::string &nic, uint16_t &port)
{
    std::string protocol;
    std::string ip;
    std::string mask;
    std::string portStr;
    if (!ock::mf::NetValidator::ParseNicUrl(nic, protocol, ip, mask, portStr)) {
        BM_LOG_ERROR("input nic(" << nic << ") not matches.");
        return BM_INVALID_PARAM;
    }
    if (!IsValidNicUrlFormat(ip, protocol)) {
        BM_LOG_ERROR("input nic(" << nic << ") not matches.");
        return BM_INVALID_PARAM;
    }
    auto parsePort = std::strtol(portStr.c_str(), nullptr, 10);
    if (parsePort <= 0 || parsePort > std::numeric_limits<uint16_t>::max()) {
        BM_LOG_ERROR("input nic(" << nic << ") not matches, port(" << parsePort << ") too large.");
        return BM_INVALID_PARAM;
    }
    port = static_cast<uint16_t>(parsePort);
    return BM_OK;
}

Result ParseDeviceNic(const std::string &nic, sockaddr_in &address)
{
    std::string protocol;
    std::string ip;
    std::string mask;
    std::string portStr;
    if (!ock::mf::NetValidator::ParseNicUrl(nic, protocol, ip, mask, portStr)) {
        BM_LOG_ERROR("input nic(" << nic << ") not matches.");
        return BM_INVALID_PARAM;
    }
    if (!IsValidNicUrlFormat(ip, protocol)) {
        BM_LOG_ERROR("input nic(" << nic << ") not matches.");
        return BM_INVALID_PARAM;
    }
    if (inet_aton(ip.c_str(), &address.sin_addr) == 0) {
        BM_LOG_ERROR("parse ip for nic: " << nic << " failed.");
        return BM_INVALID_PARAM;
    }
    auto parsePort = std::strtol(portStr.c_str(), nullptr, 10);
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

int ThreadResourceContext::ThreadStartup() noexcept
{
    auto ret = DlAclApi::AclrtSetDevice(deviceId_);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSetDevice failed: " << ret);
        return BM_ERROR;
    }

    void *stream = nullptr;
    ret = DlAclApi::AclrtCreateStreamWithConfig(&stream, 0, ACL_STREAM_FAST_LAUNCH | ACL_STREAM_FAST_SYNC);
    if (ret != 0) {
        BM_LOG_ERROR("create stream failed: " << ret);
        return BM_ERROR;
    }

    std::unique_lock<std::shared_mutex> locker{mutex_};
    streams_.emplace(std::this_thread::get_id(), stream);
    return BM_OK;
}

void ThreadResourceContext::ThreadShutdown() noexcept
{
    std::unique_lock<std::shared_mutex> locker{mutex_};
    auto pos = streams_.find(std::this_thread::get_id());
    if (pos == streams_.end()) {
        return;
    }
    auto stream = pos->second;
    streams_.erase(pos);
    locker.unlock();

    if (stream != nullptr) {
        DlAclApi::AclrtDestroyStream(stream);
    }
}

void *ThreadResourceContext::GetStream() const noexcept
{
    std::shared_lock<std::shared_mutex> locker{mutex_};
    auto pos = streams_.find(std::this_thread::get_id());
    if (pos != streams_.end()) {
        return pos->second;
    }
    return nullptr;
}
} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock