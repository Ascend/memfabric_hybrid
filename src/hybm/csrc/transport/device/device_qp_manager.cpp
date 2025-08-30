/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include "hybm_logger.h"
#include "dl_hccp_api.h"
#include "device_qp_manager.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
DeviceQpManager::DeviceQpManager(uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet,
                                 hybm_role_type role) noexcept
    : deviceId_{deviceId},
      rankId_{rankId},
      rankCount_{rankCount},
      deviceAddress_{devNet},
      rankRole_{role}
{
}

int DeviceQpManager::WaitingConnectionReady() noexcept
{
    return BM_OK;
}

const void *DeviceQpManager::GetQpInfoAddress() const noexcept
{
    return nullptr;
}

void *DeviceQpManager::CreateLocalSocket() noexcept
{
    void *socketHandle = nullptr;
    HccpRdev rdev;
    rdev.phyId = deviceId_;
    rdev.family = AF_INET;
    rdev.localIp.addr = deviceAddress_.sin_addr;
    auto ret = DlHccpApi::RaSocketInit(HccpNetworkMode::NETWORK_OFFLINE, rdev, socketHandle);
    if (ret != 0) {
        BM_LOG_ERROR("initialize socket handle failed: " << ret);
        return nullptr;
    }

    return socketHandle;
}

int DeviceQpManager::CreateServerSocket() noexcept
{
    if (serverSocketHandle_ != nullptr) {
        return BM_OK;
    }

    auto socketHandle = CreateLocalSocket();
    if (socketHandle == nullptr) {
        BM_LOG_ERROR("create local socket handle failed.");
        return BM_DL_FUNCTION_FAILED;
    }

    HccpSocketListenInfo listenInfo{};
    listenInfo.handle = socketHandle;
    listenInfo.port = deviceAddress_.sin_port;
    bool successListen = false;
    while (listenInfo.port <= std::numeric_limits<uint16_t>::max()) {
        auto ret = DlHccpApi::RaSocketListenStart(&listenInfo, 1);
        if (ret == 0) {
            deviceAddress_.sin_port = listenInfo.port;
            successListen = true;
            break;
        }
        listenInfo.port++;
    }
    if (!successListen) {
        BM_LOG_ERROR("start to listen server socket failed.");
        DlHccpApi::RaSocketDeinit(socketHandle);
        return BM_DL_FUNCTION_FAILED;
    }

    BM_LOG_INFO("start to listen on port: " << listenInfo.port << " success.");
    serverSocketHandle_ = socketHandle;
    return BM_OK;
}

void DeviceQpManager::DestroyServerSocket() noexcept
{
    if (serverSocketHandle_ == nullptr) {
        return;
    }

    HccpSocketListenInfo listenInfo{};
    listenInfo.handle = serverSocketHandle_;
    listenInfo.port = deviceAddress_.sin_port;
    auto ret = DlHccpApi::RaSocketListenStop(&listenInfo, 1);
    if (ret != 0) {
        BM_LOG_INFO("stop to listen on port: " << listenInfo.port << " return: " << ret);
    }

    ret = DlHccpApi::RaSocketDeinit(serverSocketHandle_);
    if (ret != 0) {
        BM_LOG_INFO("deinit server socket return: " << ret);
    }
    serverSocketHandle_ = nullptr;
}
}
}
}

}