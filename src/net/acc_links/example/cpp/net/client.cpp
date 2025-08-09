/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <getopt.h>
#include <unistd.h>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <cstring>

#include "acc_tcp_client.h"
#include "example_common.h"

using namespace ock::acc;
void parseOpts(int argc, char *const * argv, std::string& masterIp, int32_t& port, int32_t& rank)
{
    struct option options[] = {
        {"masterIp", required_argument, nullptr, 'i'},
        {"masterPort", required_argument, nullptr, 'p'},

        {"rankId", required_argument, nullptr, 'r'},
        {nullptr, 0, nullptr, 0},
    };

    int ret = 0;
    int index = 0;
    std::string str = "i:p:";
    while ((ret = getopt_long(argc, argv, str.c_str(), options, &index)) != -1) {
        switch (ret) {
            case 'i':
                masterIp.assign(optarg, strlen(optarg));
                break;
            case 'p':
                port = static_cast<int32_t>(strtoul(optarg, nullptr, 0));
                break;
            case 'r':
                rank = static_cast<int32_t>(strtoul(optarg, nullptr, 0));
                break;
            case '?':
                printf("unknown argument\n");
                break;
            default:
                break;
        }
    }
}

int main(int argc, char* argv[])
{
    const char* usage = "usage\n"
                        "       -i, --masterIp,             ip of master listen with\n"
                        "       -p, --masterPort,           port of master listen with\n"
                        "       -r, --rankId,               rankId of client\n";
    if (argc != 7) {
        printf("invalid param, %s for example %s -i XX.XX.XX.XX -p 9888 -r 1\n", usage, argv[0]);
        return -1;
    }

    std::string masterIp;
    int32_t port = 0;
    int32_t rank = 0;
    parseOpts(argc, argv, masterIp, port, rank);

    AccConnReq req{};
    req.magic = 0;
    req.version = 1;
    req.rankId = static_cast<uint64_t>(rank);
    AccTcpClientPtr client = AccTcpClient::Create(masterIp, port);
    auto result = client->Connect(req);
    if (result != ACC_OK) {
        printf("connect to server failed\n");
        return -1;
    }

    int32_t loop = 1000;
    char buf[1024];
    while (loop-- > 0) {
        result = client->Send(EX_MSG_OP, reinterpret_cast<uint8_t*>(const_cast<char*>(data)), strlen(data));
        if (result != ACC_OK) {
            printf("send msg to master failed\n");
        }

        bzero(buf, 1024);
        int16_t msgType;
        int16_t msgRet;
        uint32_t acLen;
        result = client->Receive(reinterpret_cast<uint8_t*>(buf), strlen(reply), msgType, msgRet, acLen);
        if (result != ACC_OK) {
            printf("master handle message failed:%d\n ", result);
        }
        printf("master response:%s\n", buf);
    }
    return 0;
}