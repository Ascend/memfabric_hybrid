/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_IPV4_VALIDATOR_H
#define MEMFABRIC_HYBRID_IPV4_VALIDATOR_H

#include <arpa/inet.h>
#include <climits>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "mf_str_util.h"

namespace ock {
namespace mf {

constexpr uint32_t DG_256 = 256;
constexpr uint32_t DG_4 = 4;
constexpr uint32_t DG_3 = 3;
constexpr uint32_t DG_2 = 2;
constexpr uint32_t DG_1 = 1;

class Ipv4PortValidator {
public:
    explicit Ipv4PortValidator(const std::string &name, bool emptyAllowed = false)
        : name_(name), emptyAllowed_(emptyAllowed)
    {}

    ~Ipv4PortValidator() = default;

    bool Initialize()
    {
        return true;
    }

    const std::string &ErrorMessage()
    {
        return errMsg_;
    }

    bool Validate(const std::string &value)
    {
        covertedIp_ = "";
        covertedPort_ = 0;

        if (value.empty() && emptyAllowed_) {
            return true;
        }

        std::vector<std::string> ipPortVec = StrUtil::Split(value, ':');
        if (ipPortVec.size() != DG_2) {
            errMsg_ = "Invalid value for <" + name_ + ">, it should be between ip mask like <ip>:<port>";
            return false;
        }

        /* check port value which should be 0~63535 */
        long tmp = 0;
        if (!StrUtil::String2Uint<long>(ipPortVec[DG_1], tmp)) {
            errMsg_ = "Invalid value for <" + name_ + ">, it should be between ip mask like <ip>:<port>";
            return false;
        }

        static const uint16_t PORT_MAX = 0xFFFF;
        if (tmp < 0 || tmp > PORT_MAX) {
            errMsg_ = "Invalid value for <" + name_ + ">, it should be between ip mask like <ip>:<port>";
            return false;
        }
        covertedPort_ = static_cast<uint16_t>(tmp);

        /* split ip and check each seg */
        std::vector<std::string> ip = StrUtil::Split(ipPortVec[0], '.');
        if (ip.size() != DG_4 || Ip2UInt(ipPortVec[0]) == 0) { // 禁止全0 ip
            errMsg_ = "Invalid value for <" + name_ + ">, it should be between ip mask like <ip>:<port>";
            return false;
        }

        for (auto &item : ip) {
            /* check mask value which should be 0~32 */
            tmp = 0;
            if (!StrUtil::String2Uint<long>(item, tmp)) {
                errMsg_ = "Invalid value for <" + name_ + ">, it should be between ip mask like <ip>:<port>";
                return false;
            }

            if (tmp < 0 || tmp >= DG_256) {
                errMsg_ = "Invalid value for <" + name_ + ">, it should be between ip mask like <ip>:<port>";
                return false;
            }
        }

        covertedIp_ = ipPortVec[0];
        return true;
    }

    void GetIpPort(std::string &ip, uint16_t &port)
    {
        ip = covertedIp_;
        port = covertedPort_;
    }

private:
    inline uint32_t Ip2UInt(const std::string &ip)
    {
        return htonl(inet_addr(ip.c_str()));
    }

private:
    bool emptyAllowed_ = false;
    std::string name_;
    std::string errMsg_;
    std::string covertedIp_;
    uint16_t covertedPort_{0};
};

class SocketAddressParser {
public:
    SocketAddressParser() = default;
    bool Initialize(const std::string &url)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (initialized_) {
            return true;
        }

        if (url.empty()) {
            return false;
        }

        if (!ParseUrl(url) || !ResolveAddress()) {
            return false;
        }
        initialized_ = true;
        return true;
    }

    [[nodiscard]] std::string GetIp()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!initialized_) {
            return {};
        }
        return ip_;
    }

    [[nodiscard]] uint16_t GetPort()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!initialized_) {
            return {};
        }
        return port_;
    }

    [[nodiscard]] bool IsIpv6()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!initialized_) {
            return {};
        }
        return is_ipv6_;
    }

    [[nodiscard]] int GetAddressFamily()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!initialized_) {
            return {};
        }
        return address_family_;
    }

    [[nodiscard]] const struct sockaddr *GetSockAddr()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!initialized_) {
            return {};
        }
        return reinterpret_cast<const struct sockaddr *>(&storage_);
    }

    [[nodiscard]] socklen_t GetAddrLen()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!initialized_) {
            return {};
        }
        return addr_len_;
    }

    [[nodiscard]] bool IsInitialized()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return initialized_;
    }

    [[nodiscard]] std::pair<const sockaddr *, size_t> GetPeerAddress(const std::string &peerIp, const uint16_t port)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!initialized_) {
            return {};
        }
        std::fill_n(reinterpret_cast<uint8_t *>(&peerStorage_), sizeof(peerStorage_), 0);
        if (is_ipv6_) {
            auto *addr6 = reinterpret_cast<struct sockaddr_in6 *>(&peerStorage_);
            if (inet_pton(AF_INET6, peerIp.c_str(), &addr6->sin6_addr) != 1) {
                return {};
            }
            addr6->sin6_family = AF_INET6;
            addr6->sin6_port = htons(port);
            return {reinterpret_cast<const sockaddr *>(&peerStorage_), sizeof(sockaddr_in6)};
        }
        auto *addr4 = reinterpret_cast<struct sockaddr_in *>(&peerStorage_);
        if (inet_pton(AF_INET, peerIp.c_str(), &addr4->sin_addr) != 1) {
            return {};
        }
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(port);
        return {reinterpret_cast<const sockaddr *>(&peerStorage_), sizeof(sockaddr_in)};
    }

private:

    bool ParseUrl(const std::string &url)
    {
        const std::string tcp_prefix = "tcp://";
        if (url.find(tcp_prefix) != 0) {
            return false;
        }

        std::string host_port = url.substr(tcp_prefix.length());

        size_t colon_pos = host_port.find_last_of(':');
        if (colon_pos == std::string::npos) {
            return false;
        }
        std::string host = host_port.substr(0, colon_pos);
        std::string port_str = host_port.substr(colon_pos + 1);
        // 解析端口
        try {
            int port = std::stoi(port_str);
            if (port < 1 || port > std::numeric_limits<uint16_t>::max()) {
                return false;
            }
            port_ = static_cast<uint16_t>(port);
        } catch (const std::exception &) {
            return false;
        }

        // 处理IPv6地址（包含在方括号中）
        if (host.front() == '[' && host.back() == ']') {
            ip_ = host.substr(1, host.length() - 2);
            is_ipv6_ = true;
        } else {
            ip_ = host;
            // 检查是否是IPv6地址（包含冒号）
            is_ipv6_ = (host.find(':') != std::string::npos);
        }

        if (ip_.empty()) {
            return false;
        }
        return true;
    }

    // 解析地址信息
    [[nodiscard]] bool ResolveAddress()
    {
        std::fill_n(reinterpret_cast<uint8_t *>(&storage_), sizeof(storage_), 0);
        if (is_ipv6_) {
            auto *addr6 = reinterpret_cast<struct sockaddr_in6 *>(&storage_);
            addr_len_ = sizeof(struct sockaddr_in6);
            address_family_ = AF_INET6;
            if (inet_pton(AF_INET6, ip_.c_str(), &addr6->sin6_addr) != 1) {
                return false;
            }
            addr6->sin6_family = AF_INET6;
            addr6->sin6_port = htons(port_);
            return true;
        }
        auto *addr4 = reinterpret_cast<struct sockaddr_in *>(&storage_);
        addr_len_ = sizeof(struct sockaddr_in);
        address_family_ = AF_INET;
        if (inet_pton(AF_INET, ip_.c_str(), &addr4->sin_addr) != 1) {
            return false;
        }
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(port_);
        return true;
    }

    std::mutex mutex_{};
    std::string ip_;
    uint16_t port_ = 0;
    int address_family_ = 0;
    socklen_t addr_len_ = 0;
    sockaddr_storage storage_{};
    sockaddr_storage peerStorage_{};
    bool initialized_ = false;
    bool is_ipv6_ = false;
};

class SocketAddressParserMgr {
public:
    SocketAddressParserMgr(const SocketAddressParserMgr &) = delete;
    SocketAddressParserMgr &operator=(const SocketAddressParserMgr &) = delete;

    static SocketAddressParserMgr &getInstance()
    {
        static SocketAddressParserMgr instance;
        return instance;
    }

    std::shared_ptr<SocketAddressParser> CreateParser(const std::string &url)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto find = url2Parsers_.find(url);
        if (find != url2Parsers_.end()) {
            return find->second;
        }
        auto *o = new (std::nothrow) SocketAddressParser;
        if (o == nullptr) {
            return nullptr;
        }
        const std::shared_ptr<SocketAddressParser> obj(o);
        if (!obj->Initialize(url)) {
            return nullptr;
        }
        url2Parsers_[url] = obj;
        port2Parsers_[obj->GetPort()] = obj;
        return obj;
    }

    std::shared_ptr<SocketAddressParser> GetParser(const uint32_t port)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // 9966L 未赋值，来源acl
        if (!port2Parsers_.empty() && port == 9966L) {
            return port2Parsers_.begin()->second;
        }
        auto find = port2Parsers_.find(port);
        if (find == port2Parsers_.end()) {
            return nullptr;
        }
        return find->second;
    }

private:
    std::mutex mutex_{};
    std::unordered_map<std::string, std::shared_ptr<SocketAddressParser>> url2Parsers_{};
    std::unordered_map<uint32_t, std::shared_ptr<SocketAddressParser>> port2Parsers_{};
    SocketAddressParserMgr() = default;
};

} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_IPV4_VALIDATOR_H