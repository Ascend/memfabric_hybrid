/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <cstdint>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "hybm_define.h"

using namespace ock::mf;

extern "C" {
int ra_get_interface_version(uint32_t deviceId, uint32_t opcode, uint32_t *version)
{
    version = nullptr;
    return 0;
}

int ra_socket_init(HccpNetworkMode mode, HccpRdev rdev, void **socketHandle)
{
    *socketHandle = reinterpret_cast<void*>(0x3);
    return 0;
}

int ra_init(const HccpRaInitConfig *config)
{
    return 0;
}

int ra_socket_deinit(void *socketHandle)
{
    return 0;
}

int ra_rdev_init_v2(const HccpRdevInitInfo info, const HccpRdev rdev, void **rdmaHandle)
{
    *rdmaHandle = reinterpret_cast<void*>(0x1);
    return 0;
}

int ra_rdev_get_handle(uint32_t deviceId, void **rdmaHandle)
{
    return 0;
}

int ra_socket_batch_connect(HccpSocketConnectInfo conn[], uint32_t num)
{
    return 0;
}

int ra_socket_batch_close(HccpSocketCloseInfo conn[], uint32_t num)
{
    return 0;
}

int ra_socket_batch_abort(HccpSocketConnectInfo conn[], uint32_t num)
{
    return 0;
}

int ra_socket_listen_start(HccpSocketListenInfo conn[], uint32_t num)
{
    return 0;
}

int ra_socket_listen_stop(HccpSocketListenInfo conn[], uint32_t num)
{
    return 0;
}

int ra_get_sockets(uint32_t role, HccpSocketInfo conn[], uint32_t num, uint32_t *connectedNum)
{
    return 0;
}

int ra_socket_send(const void *fd, const void *data, uint64_t size, uint64_t *sent)
{
    return 0;
}

int ra_socket_recv(const void *fd, void *data, uint64_t size, uint64_t *received)
{
    return 0;
}

int ra_get_ifnum(const HccpRaGetIfAttr *config, uint32_t *num)
{
    *num = 1;
    return 0;
}

int ra_get_ifaddrs(const HccpRaGetIfAttr *config, HccpInterfaceInfo infos[], uint32_t *num)
{
    infos[0].family = AF_INET;
    inet_aton("127.0.0.1", &infos[0].ifaddr.ip.addr);
    return 0;
}

int ra_socket_white_list_add(void *socket, const HccpSocketWhiteListInfo list[], uint32_t num)
{
    return 0;
}

int ra_socket_white_list_del(void *socket, const HccpSocketWhiteListInfo list[], uint32_t num)
{
    return 0;
}

int ra_qp_create(void *rdmaHandle, int flag, int qpMode, void **qpHandle)
{
    *qpHandle = reinterpret_cast<void*>(0x5);
    return 0;
}

int ra_ai_qp_create(void *rdmaHandle, const HccpQpExtAttrs *attrs, HccpAiQpInfo *info, void **qpHandle)
{
    *qpHandle = reinterpret_cast<void*>(0x7);
    return 0;
}

int ra_qp_destroy(void *qpHandle)
{
    return 0;
}

int ra_get_qp_status(void *qpHandle, int *status)
{
    return 0;
}

int ra_qp_connect_async(void *qp, const void *socketFd)
{
    return 0;
}

int ra_register_mr(const void *rdmaHandle, HccpMrInfo *info, void **mrHandle)
{
    return 0;
}

int ra_deregister_mr(const void *rdmaHandle, void *mrHandle)
{
    return 0;
}

int ra_mr_reg(void *qpHandle, HccpMrInfo *info)
{
    return 0;
}

int ra_mr_dereg(void *qpHandle, HccpMrInfo *info)
{
    return 0;
}
}
