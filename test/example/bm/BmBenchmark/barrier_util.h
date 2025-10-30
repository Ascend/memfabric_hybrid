/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef SMEM_BARRIER_UTIL_H
#define SMEM_BARRIER_UTIL_H

#include <string>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

class BarrierUtil {
public:
    BarrierUtil() {}
    ~BarrierUtil()
    {
    }

    int32_t Init(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, std::string ipPort)
    {
        // ipPort = tcp://ip:port
        auto ip = Split(ipPort, ':')[1]; // ip = //ip
        ip = ip.substr(2);

        if (rankId == 0) {
            return ServerUp(rkSize, ip);
        } else {
            return ClientUp(rkSize, ip);
        }
    }

    int32_t Barrier()
    {
        int ret = 0;
        char msg[10] = {0};
        char buffer[10] = {0};

        if (isServer_) {
            for (int fd : clientFd_) {
                do {
                    ret = read(fd, buffer, 10);
                } while (ret == 0);
                if (ret <= 0 || buffer[0] != 'S') {
                    return -71;
                }
            }

            msg[0] = 'R';
            for (int fd : clientFd_) {
                ret = send(fd, msg, 1, 0);
                if (ret <= 0) {
                    return -72;
                }
            }
        } else {
            msg[0] = 'S';
            ret = send(localFd_, msg, 1, 0);
            if (ret <= 0) {
                return -73;
            }

            do {
                ret = read(localFd_, buffer, 10);
            } while (ret == 0);
            if (ret <= 0 || buffer[0] != 'R') {
                return -74;
            }
        }
        return 0;
    }
private:
    std::vector<std::string> Split(const std::string& str, char delimiter)
    {
        std::vector<std::string> result;
        std::stringstream ss(str);
        std::string item;

        while (std::getline(ss, item, delimiter)) {
            result.push_back(item);
        }
        return result;
    }

    int32_t ServerUp(uint32_t rkSize, std::string ip)
    {
        int server_fd;
        struct sockaddr_in address;
        int opt = 1;
        socklen_t addrlen = sizeof(address);

        if ((server_fd = ::socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            return -1;
        }
        localFd_ = server_fd;

        if (::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
            return -2;
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = inet_addr(ip.c_str());
        address.sin_port = htons(port_);
        if (::bind(server_fd, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)) < 0) {
            return -3;
        }
        if (::listen(server_fd, 200L) < 0) {
            return -4;
        }

        for (uint32_t i = 1; i < rkSize; i++) {
            int new_socket;
            if ((new_socket = ::accept(server_fd, reinterpret_cast<struct sockaddr *>(&address), &addrlen)) < 0) {
                return -5;
            }

            SetTimeout(new_socket);
            clientFd_.emplace_back(new_socket);
        }

        isServer_ = true;
        return 0;
    }

    int32_t ClientUp(uint32_t rkSize, std::string ip)
    {
        int sock = 0;
        struct sockaddr_in serv_addr;
        if ((sock = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            return -10;
        }
        localFd_ = sock;

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(ip.c_str());
        serv_addr.sin_port = htons(port_);

        int32_t connT = 30U;
        while (::connect(sock, reinterpret_cast<struct sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0) {
            if (--connT < 0) {
                return -11;
            }
            sleep(1);
        }
        return SetTimeout(sock);
    }

    int32_t SetTimeout(int fd)
    {
        struct timeval timeout;
        timeout.tv_sec = 300;
        timeout.tv_usec = 0;

        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            return -21;
        }
        return 0;
    }

    uint16_t port_ = 12562U;
    bool isServer_ = false;
    int localFd_ = -1;
    std::vector<int> clientFd_;
};

#endif // SMEM_BARRIER_UTIL_H
