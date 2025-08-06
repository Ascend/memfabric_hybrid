/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <getopt.h>
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <cstring>

#include "acc_tcp_server.h"
#include "example_common.h"

void Log(int32_t logLevel, const char* msg)
{
    static std::string levelStr[] = {"debug", "info", "warn", "error"};
    std::cout << "myLogFunc " << levelStr[logLevel] << " " << msg << std::endl;
}

using namespace ock::acc;

int parseOpts(int argc, char *const * argv, std::string& masterIp, int32_t& port)
{
    const char* usage = "usage\n"
                    "       -i, --masterIp,             ip of master listen with\n"
                    "       -p, --masterPort,           port of master listen with\n";

    if (argc != 5) {
        printf("invalid param, %s for example %s -i XX.XX.XX.XX -p 9888\n", usage, argv[0]);
        return -1;
    }

    struct option options[] = {
        {"masterIp", required_argument, nullptr, 'i'},
        {"masterPort", required_argument, nullptr, 'p'},

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
            case '?':
                printf("unknown argument\n");
                break;
            default:
                break;
        }
    }
    return 0;
}

void PrepareServerOptions(AccTcpServerOptions& opts, std::string& masterIp, int32_t& port)
{
    opts.enableListener = true;
    opts.linkSendQueueSize = 100;
    opts.listenIp = masterIp;
    opts.listenPort = port;
    opts.reusePort = true;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = 10;
}

int main(int argc, char* argv[])
{
    std::string masterIp;
    int32_t port = 0;
    if (parseOpts(argc, argv, masterIp, port) != 0) {
        return -1;
    }
    printf("log:%s->%d\n", masterIp.c_str(), port);

    AccSetExternalLog(Log);
    AccSetLogLevel(2);

    std::mutex mutex_;
    std::unordered_map<int, AccTcpLinkComplexPtr> rankLinkMap;

    AccTcpServerPtr server = AccTcpServer::Create();
    server->RegisterNewLinkHandler(
        [&mutex_, &rankLinkMap](const AccConnReq &req, const AccTcpLinkComplexPtr &link) -> int {
            mutex_.lock();
            rankLinkMap[req.rankId] = link;
            mutex_.unlock();
            return ACC_OK;
        });

    server->RegisterNewRequestHandler(EX_MSG_OP, [&](const AccTcpRequestContext &context) -> int {
        printf("receive client msg, type:%d, len:%d, seqNo:%d\n", context.MsgType(), context.DataLen(),
               context.SeqNo());
        const auto buffer = AccDataBuffer::Create(reply, strlen(reply));
        context.Reply(ACC_OK, buffer);
        return ACC_OK;
    });

    server->RegisterLinkBrokenHandler([&mutex_, &rankLinkMap](const AccTcpLinkComplexPtr &link) -> int {
        mutex_.lock();
        printf("broken link:%d\n", link->Id());
        mutex_.unlock();
        return ACC_OK;
    });

    server->RegisterRequestSentHandler(
        EX_MSG_OP, [](AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr&) -> int {
            printf("send reply for message:%d result:%d\n", header.type, result);
            return ACC_OK;
        });

    AccTcpServerOptions opts;
    PrepareServerOptions(opts, masterIp, port);

    server->Start(opts);
    getchar();

    server->Stop();
    return 0;
}