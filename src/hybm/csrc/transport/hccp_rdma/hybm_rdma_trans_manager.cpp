/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <vector>

#include "hybm_logger.h"
#include "dl_hccp_api.h"
#include "hybm_rdma_trans_manager.h"

namespace ock {
namespace mf {
TransHandlePtr RdmaTransportManager::OpenDevice(const TransDeviceOptions &options)
{
    BM_LOG_INFO("open device with options(type=" << options.transType << ", devId=" << options.deviceId << ")");
    if (!OpenTsd()) {
        return nullptr;
    }

    if (!RaInit()) {
        return nullptr;
    }

    if (!RetireDeviceIp()) {
        return nullptr;
    }

    if (!RaRdevInit()) {
        return nullptr;
    }

    return nullptr;
}

bool RdmaTransportManager::OpenTsd()
{
    auto res = RuntimeHccpApi::TsdOpen(deviceId_, 2U);
    if (res != 0) {
        BM_LOG_ERROR("TsdOpen failed: " << res);
        return false;
    }
    return true;
}

bool RdmaTransportManager::RaInit()
{
    HccpRaInitConfig initConfig{};
    initConfig.phyId = deviceId_;
    initConfig.nicPosition = NETWORK_OFFLINE;
    initConfig.hdcType = 6;
    BM_LOG_INFO("RaInit, config(phyId=" << initConfig.phyId << ", nicPosition=" << initConfig.nicPosition
                                        << ", hdcType=" << initConfig.hdcType << ")");
    auto ret = RuntimeHccpApi::RaInit(initConfig);
    if (ret != 0) {
        BM_LOG_ERROR("Hccp Init RA failed: " << ret);
        return false;
    }
    return true;
}

bool RdmaTransportManager::RetireDeviceIp()
{
    uint32_t count = 0;
    std::vector<HccpInterfaceInfo> infos;

    HccpRaGetIfAttr config;
    config.phyId = deviceId_;
    config.nicPosition = HccpNetworkMode::NETWORK_OFFLINE;
    config.isAll = true;

    auto ret = RuntimeHccpApi::RaGetIfNum(config, count);
    if (ret != 0 || count == 0) {
        BM_LOG_ERROR("get interface count failed: " << ret << ", count: " << count);
        return false;
    }

    infos.resize(count);
    ret = RuntimeHccpApi::RaGetIfAddrs(config, infos.data(), count);
    if (ret != 0) {
        BM_LOG_ERROR("get interface information failed: " << ret);
        return false;
    }

    for (auto &info : infos) {
        if (info.family == AF_INET) {
            deviceIp_ = info.ifaddr.ip.addr;
            return true;
        }
    }

    return false;
}

bool RdmaTransportManager::RaRdevInit()
{
    HccpRdevInitInfo info{};
    HccpRdev rdev{};

    info.mode = NETWORK_OFFLINE;
    info.notifyType = NOTIFY;
    info.enabled2mbLite = true;
    rdev.phyId = deviceId_;
    rdev.family = AF_INET;
    rdev.localIp.addr = deviceIp_;
    BM_LOG_INFO("RaRdevInitV2, info(mode=" << info.mode << ", notify=" << info.notifyType << ", enabled910aLite="
                                           << info.enabled910aLite << ", disabledLiteThread=" << info.disabledLiteThread
                                           << ", enabled2mbLite=" << info.enabled2mbLite
                                           << "), rdev(phyId=" << rdev.phyId << ", family=" << rdev.family
                                           << ", rdev.ip=" << inet_ntoa(rdev.localIp.addr) << ")");
    auto ret = RuntimeHccpApi::RaRdevInitV2(info, rdev, rdmaHandle_);
    if (ret != 0) {
        BM_LOG_ERROR("Hccp Init RDev failed: " << ret);
        return false;
    }

    BM_LOG_INFO("initialize RDev success, rdmaHandle: " << rdmaHandle_);
    return true;
}
}
}