/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <getopt.h>
#include <unistd.h>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <semaphore.h>
#include <iostream>
#include <cstring>
#include <algorithm>

#include "acc_tcp_server.h"
#include "example_common.h"

using namespace ock::acc;

sem_t sem;
int connect2Peer(const AccTcpServerPtr &myServer, const std::string &peerIp, uint16_t peerPort,
                 AccTcpLinkComplexPtr &link)
{
    if (myServer.Get() == nullptr) {
        std::cout << " myServer is nullptr" << std::endl;
        return -1;
    }
    if (peerIp.empty()) {
        std::cout << " peerIp is empty" << std::endl;
        return -1;
    }
    if (peerPort <= 1024) {
        std::cout << " peerPort is invalid " << std::endl;
        return -1;
    }

    AccConnReq req{};
    req.magic = 0;
    req.version = 1;
    req.rankId = 6;
    auto result = myServer->ConnectToPeerServer(peerIp, peerPort, req, link);
    if (result != 0) {
        std::cout << " failed to connect to " << peerIp << ":" << peerPort << std::endl;
        return -1;
    }

    std::cout << "connected to " << peerIp << ":" << peerPort << std::endl;
    return 0;
}

int sendDataToPeer(const AccTcpLinkComplexPtr &link)
{
    if (link.Get() == nullptr) {
        std::cout << "link is nullptr" << std::endl;
        return -1;
    }

    auto data2send = AccDataBuffer::Create(10);
    if (data2send.Get() == nullptr) {
        std::cout << " data2send is nullptr" << std::endl;
        return -1;
    }

    const char* words = "aaaa";
    std::copy(words, words + 4U, static_cast<char*>(data2send->DataPtrVoid()));
    data2send->SetDataSize(4L);
    return link->NonBlockSend(EX_MSG_OP, data2send, nullptr);
}

void waitForReply()
{
    std::cout << "wait for reply " << std::endl;
    sem_wait(&sem);
    std::cout << "got reply \n" << std::endl;
}

int parseOpts(int argc, char *const * argv, std::string& peerServerIp, int32_t& peerServerPort)
{
    const char* usage = "usage\n"
                        "       -i, --peerServerIp,                 ip of peer server listen with\n"
                        "       -p, --peerServerPort,               port of peer server listen with\n";
    
    if (argc != 5) {
        printf("invalid param, %s for example %s -i XX.XX.XX.XX -p 9888\n", usage, argv[0]);
        return -1;
    }

    struct option options[] = {
        {"peerServerIp", required_argument, nullptr, 'i'},
        {"peerServerPort", required_argument, nullptr, 'p'},

        {nullptr, 0, nullptr, 0},
    };

    int ret = 0;
    int index = 0;
    std::string str = "i:p:";
    while ((ret = getopt_long(argc, argv, str.c_str(), options, &index)) != -1) {
        switch (ret) {
            case 'i':
                peerServerIp.assign(optarg, strlen(optarg));
                break;
            case 'p':
                peerServerPort = static_cast<int32_t>(strtoul(optarg, nullptr, 0));
                break;
            case '?':
                printf("unknown argument\n");
                break;
            default:
                break;
        }
    }

    if (peerServerIp.empty()) {
        std::cout << "invalid peerServerIp " << peerServerIp << std::endl;
        return -1;
    }

    if (peerServerPort <= 1024) {
        std::cout << "invalid peerServerPort " << peerServerPort << std::endl;
        return -1;
    }

    printf("log:%s->%d\n", peerServerIp.c_str(), peerServerPort);
    return 0;
}

int StartServer(AccTcpServerPtr& server)
{
    AccTcpServerOptions opts;
    opts.enableListener = false;
    opts.linkSendQueueSize = 100;
    opts.magic = 0;
    opts.version = 1;
    opts.workerCount = 2;
    auto result = server->Start(opts);
    if (result != 0) {
        std::cout << "failed to start server with listener " << std::endl;
        return -1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    std::string peerServerIp;
    int32_t peerServerPort = 0;
    if (parseOpts(argc, argv, peerServerIp, peerServerPort) != 0) {
        return -1;
    }

    std::mutex mutex_;
    std::unordered_map<int, AccTcpLinkComplexPtr> rankLinkMap;
    sem_init(&sem, 0, 0);

    AccTcpServerPtr server = AccTcpServer::Create();
    server->RegisterNewRequestHandler(EX_MSG_OP, [&](const AccTcpRequestContext &context) -> int {
        printf("receive client msg, type:%d, len:%d, seqNo:%d\n", context.MsgType(), context.DataLen(),
               context.SeqNo());
        sem_post(&sem);
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

    if (StartServer(server) != 0) {
        return -1;
    }

    /* connect to peer server */
    AccTcpLinkComplexPtr link;
    auto result = connect2Peer(server, peerServerIp, peerServerPort, link);
    if (result != 0) {
        std::cout << "Failed to connect peer " << std::endl;
        return -1;
    }

    for (int i = 0; i < 10; i++) {
        /* send data to peer */
        result = sendDataToPeer(link);
        if (result != 0) {
            std::cout << "Failed to send data to peer " << std::endl;
            return -1;
        }

        /* wait for reply */
        waitForReply();
    }

    getchar();

    std::cout << "stopping server " << std::endl;

    server->Stop();

    return 0;
}