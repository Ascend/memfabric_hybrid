/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_IPV4_VALIDATOR_H
#define MEMFABRIC_HYBRID_IPV4_VALIDATOR_H

#include <arpa/inet.h>
#include <climits>
#include <string>
#include <vector>

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

        std::vector<std::string> ipPortVec;
        StrUtil::Split(value, ":", ipPortVec);
        if (ipPortVec.size() != DG_2) {
            errMsg_ = "Invalid value for <" + name_ + ">, it should be between ip mask like <ip>:<port>";
            return false;
        }

        /* check port value which should be 0~63535 */
        long tmp = 0;
        if (!StrUtil::StrToLong(ipPortVec[DG_1], tmp)) {
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
        std::vector<std::string> ip;
        StrUtil::Split(ipPortVec[0], ".", ip);
        if (ip.size() != DG_4 || Ip2UInt(ipPortVec[0]) == 0) { // 禁止全0 ip
            errMsg_ = "Invalid value for <" + name_ + ">, it should be between ip mask like <ip>:<port>";
            return false;
        }

        for (auto &item : ip) {
            /* check mask value which should be 0~32 */
            tmp = 0;
            if (!StrUtil::StrToLong(item, tmp)) {
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

}
}

#endif //MEMFABRIC_HYBRID_IPV4_VALIDATOR_H
