/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MF_NET_H
#define MF_NET_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <functional>
#include <cstdint>

namespace ock {
namespace mf {
enum IpType {
    IpV4,
    IpV6,
    IPNONE,
};

struct net_addr_t {
    union {
        struct in_addr ipv4;
        struct in6_addr ipv6;
    } ip {};
    IpType type {IPNONE};

    net_addr_t() : type(IPNONE) {}

    static net_addr_t from_ipv4(const struct in_addr& addr)
    {
        net_addr_t result;
        result.type = IpV4;
        result.ip.ipv4 = addr;
        return result;
    }

    static net_addr_t from_ipv6(const struct in6_addr& addr)
    {
        net_addr_t result;
        result.type = IpV6;
        result.ip.ipv6 = addr;
        return result;
    }

    bool operator==(const net_addr_t& other) const
    {
        if (type != other.type) return false;
        
        if (type == IpV4) {
            return ip.ipv4.s_addr == other.ip.ipv4.s_addr;
        } else if (type == IpV6) {
            return std::memcmp(&ip.ipv6, &other.ip.ipv6, sizeof(struct in6_addr)) == 0;
        }
        
        return true;
    }
};
}
}
namespace std {
    template<>
    struct hash<ock::mf::net_addr_t> {
        size_t operator()(const ock::mf::net_addr_t& addr) const
        {
            size_t result = 0;

            hash_combine(result, static_cast<int>(addr.type));

            if (addr.type == ock::mf::IpV4) {
                hash_combine(result, addr.ip.ipv4.s_addr);
            } else if (addr.type == ock::mf::IpV6) {
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&addr.ip.ipv6);
                for (size_t i = 0; i < sizeof(struct in6_addr); ++i) {
                    hash_combine(result, bytes[i]);
                }
            }

            return result;
        }

    private:
        static void hash_combine(size_t& seed, size_t value)
        {
            constexpr size_t SHIFT_LEFT = 6;
            constexpr size_t SHIFT_RIGHT = 2;
            seed ^= value + 0x9e3779b9 + (seed << SHIFT_LEFT) + (seed >> SHIFT_RIGHT);
        }
    };
}

#endif // MF_NET_H