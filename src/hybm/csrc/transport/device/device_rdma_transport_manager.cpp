/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "dl_hccp_api.h"
#include "device_rdma_common.h"
#include "device_rdma_helper.h"
#include "device_rdma_transport_manager.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
bool RdmaTransportManager::tsdOpened_ = false;
bool RdmaTransportManager::deviceIpRetired_ = false;
bool RdmaTransportManager::raInitialized_ = false;
void* RdmaTransportManager::storedRdmaHandle_ = nullptr;

RdmaTransportManager::~RdmaTransportManager()
{
    ClearAllRegisterMRs();
    tsdOpened_ = false;
    raInitialized_ = false;
    deviceIpRetired_ = false;
}

Result RdmaTransportManager::OpenDevice(const TransportOptions &options)
{
    int32_t deviceId = -1;

    BM_LOG_INFO("begin to open device with " << options);
    auto ret = DlAclApi::AclrtGetDevice(&deviceId);
    if (ret != 0 || deviceId < 0) {
        BM_LOG_ERROR("AclrtGetDevice() return=" << ret << ", output deviceId=" << deviceId);
        return BM_DL_FUNCTION_FAILED;
    }
    deviceId_ = static_cast<uint32_t>(deviceId);
    rankId_ = options.rankId;
    rankCount_ = options.rankCount;
    ret = ParseDeviceNic(options.nic, devicePort_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("parse input nic(" << options.nic << ") failed!");
        return BM_INVALID_PARAM;
    }

    if (!PrepareOpenDevice(deviceId_, rankCount_, deviceIp_, rdmaHandle_)) {
        BM_LOG_ERROR("PrepareOpenDevice failed.");
        return BM_ERROR;
    }

    nicInfo_ = GenerateDeviceNic(deviceIp_, devicePort_);

    sockaddr_in deviceAddr;
    deviceAddr.sin_family = AF_INET;
    deviceAddr.sin_addr = deviceIp_;
    deviceAddr.sin_port = devicePort_;
    connectionManager_ = std::make_shared<RdmaConnectionManager>(deviceId_, rankId_, rankCount_, deviceAddr);
    ret = connectionManager_->PrepareConnection(rdmaHandle_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("prepare connection failed: " << ret);
        connectionManager_ = nullptr;
        return ret;
    }

    BM_LOG_INFO("open device with " << options << " success.");
    return BM_OK;
}

Result RdmaTransportManager::CloseDevice()
{
    connectionManager_ = nullptr;
    return BM_OK;
}

Result RdmaTransportManager::RegisterMemoryRegion(const TransportMemoryRegion &mr)
{
    void *mrHandle = nullptr;
    HccpMrInfo info{};
    info.addr = (void *)(ptrdiff_t)mr.addr;
    info.size = mr.size;
    info.access = mr.access;
    auto ret = DlHccpApi::RaRegisterMR(rdmaHandle_, &info, mrHandle);
    if (ret != 0) {
        BM_LOG_ERROR("register MR=" << mr << " address=" << info.addr << " failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    RegMemResult result{mr.addr, mr.size, mrHandle, info.lkey, info.rkey};
    BM_LOG_DEBUG("register MR address=" << info.addr << ", result=" << result);

    std::unique_lock<std::mutex> uniqueLock{mrsMutex_};
    registerMRS_.emplace(mr.addr, result);
    return BM_OK;
}

Result RdmaTransportManager::UnregisterMemoryRegion(uint64_t addr)
{
    std::unique_lock<std::mutex> uniqueLock{mrsMutex_};
    auto pos = registerMRS_.find(addr);
    if (pos == registerMRS_.end()) {
        uniqueLock.unlock();
        BM_LOG_ERROR("input address not register!");
        return BM_INVALID_PARAM;
    }

    auto ret = DlHccpApi::RaDeregisterMR(rdmaHandle_, pos->second.mrHandle);
    if (ret != 0) {
        uniqueLock.unlock();
        BM_LOG_ERROR("Unregister MR addr failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    registerMRS_.erase(pos);
    return BM_OK;
}

Result RdmaTransportManager::QueryMemoryKey(uint64_t addr, TransportMemoryKey &key)
{
    RegMemKeyUnion keyUnion{};
    std::unique_lock<std::mutex> uniqueLock{mrsMutex_};
    auto pos = registerMRS_.lower_bound(addr);
    if (pos == registerMRS_.end() || pos->first + pos->second.size <= addr) {
        uniqueLock.unlock();
        BM_LOG_ERROR("input address not register!");
        return BM_INVALID_PARAM;
    }

    keyUnion.deviceKey = pos->second;
    uniqueLock.unlock();

    key = keyUnion.commonKey;
    return BM_OK;
}

Result RdmaTransportManager::Prepare(const HybmTransPrepareOptions &options)
{
    if (options.options.size() > rankCount_) {
        BM_LOG_ERROR("options size():" << options.options.size() << " larger than rank count: " << rankCount_);
        return BM_INVALID_PARAM;
    }

    if (options.options.find(rankId_) == options.options.end()) {
        BM_LOG_ERROR("options not contains self rankId: " << rankId_);
        return BM_INVALID_PARAM;
    }

    for (auto it = options.options.begin(); it != options.options.end(); ++it) {
        if (it->first >= rankCount_) {
            BM_LOG_ERROR("input options of nics contains rankId:" << it->first << ", rank count: " << rankCount_);
            return BM_INVALID_PARAM;
        }
    }

    sockaddr_in deviceNetwork;
    std::unordered_map<uint32_t, ConnectRankInfo> rankInfo;
    for (auto it = options.options.begin(); it != options.options.end(); ++it) {
        auto ret = ParseDeviceNic(it->second.nic, deviceNetwork);
        if (ret != BM_OK) {
            BM_LOG_ERROR("parse networks[" << it->first << "]=" << it->second.nic << " failed: " << ret);
            return BM_INVALID_PARAM;
        }

        rankInfo.emplace(it->first, ConnectRankInfo{deviceNetwork, it->second.memKeys});
    }

    auto ret = connectionManager_->StartServerListen(rankInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("start server listen on device failed: " << ret);
        return ret;
    }

    return BM_OK;
}

Result RdmaTransportManager::Connect()
{
    auto ret = AsyncConnect();
    if (ret != BM_OK) {
        BM_LOG_ERROR("AsyncConnect() failed: " << ret);
        return ret;
    }

    ret = WaitForConnected(-1L);
    if (ret != BM_OK) {
        BM_LOG_ERROR("WaitForConnected(-1) failed: " << ret);
        return ret;
    }

    return BM_OK;
}

Result RdmaTransportManager::AsyncConnect()
{
    if (connectionManager_ == nullptr) {
        BM_LOG_ERROR("server side not listen!");
        return BM_ERROR;
    }

    auto ret = connectionManager_->ClientConnectServers();
    if (ret != BM_OK) {
        BM_LOG_ERROR("connect to server on device failed: " << ret);
        return ret;
    }

    return BM_OK;
}

Result RdmaTransportManager::WaitForConnected(int64_t timeoutNs)
{
    if (connectionManager_ == nullptr) {
        BM_LOG_ERROR("server side not listen!");
        return BM_ERROR;
    }
    auto ret = connectionManager_->WaitServerSideConnected();
    if (ret != BM_OK) {
        BM_LOG_ERROR("wait for server side connected on device failed: " << ret);
        return ret;
    }

    return BM_OK;
}

Result RdmaTransportManager::UpdateRankOptions(const HybmTransPrepareOptions &options)
{
    if (connectionManager_ == nullptr) {
        BM_LOG_ERROR("server side not listen!");
        return BM_ERROR;
    }

    sockaddr_in deviceNetwork;
    std::unordered_map<uint32_t, ConnectRankInfo> ranksInfo;
    for (auto it = options.options.begin(); it != options.options.end(); ++it) {
        auto ret = ParseDeviceNic(it->second.nic, deviceNetwork);
        if (ret != BM_OK) {
            BM_LOG_ERROR("update rank network(" << it->second.nic << ") invalid.");
            return BM_INVALID_PARAM;
        }
        ranksInfo.emplace(it->first, ConnectRankInfo{deviceNetwork, it->second.memKeys});
    }

    auto ret = connectionManager_->UpdateRanks(ranksInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("update rank options failed: " << ret);
        return ret;
    }

    return BM_OK;
}

const std::string &RdmaTransportManager::GetNic() const
{
    return nicInfo_;
}

const void *RdmaTransportManager::GetQpInfo() const
{
    if (connectionManager_ == nullptr) {
        BM_LOG_ERROR("GetQpInfo(): connection manager not created.");
        return nullptr;
    }
    return connectionManager_->GetAiQpRMAQueueInfo();
}

Result RdmaTransportManager::ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_LOG_ERROR("DEVICE Side RDMA not support direct Read from host.");
    return BM_ERROR;
}

Result RdmaTransportManager::WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_LOG_ERROR("DEVICE Side RDMA not support direct Write from host.");
    return BM_ERROR;
}

bool RdmaTransportManager::PrepareOpenDevice(uint32_t device, uint32_t rankCount, in_addr &deviceIp, void *&rdmaHandle)
{
    // If can get rdmaHanle, maybe the device has beed opened, can try get rdmaHanle directly.
    if (DlHccpApi::RaRdevGetHandle(device, rdmaHandle) == 0) {
        if (rdmaHandle != nullptr) {
            if (!RetireDeviceIp(device, deviceIp)) {
                BM_LOG_ERROR("RetireDeviceIp failed.");
                return false;
            }
            BM_LOG_DEBUG("Had prepared device and get rdmaHandle success.");
            return true;
        }
        BM_LOG_INFO("Had prepared device, but RdmaHadle is null, need init again.");
    }
    if (!OpenTsd(device, rankCount)) {
        BM_LOG_ERROR("open tsd failed.");
        return false;
    }

    if (!RaInit(device)) {
        BM_LOG_ERROR("RaInit failed.");
        return false;
    }

    if (!RetireDeviceIp(device, deviceIp)) {
        BM_LOG_ERROR("RetireDeviceIp failed.");
        return false;
    }

    if (!RaRdevInit(device, deviceIp, rdmaHandle)) {
        BM_LOG_ERROR("RaRdevInit failed.");
        return false;
    }
    return true;
}

bool RdmaTransportManager::OpenTsd(uint32_t deviceId, uint32_t rankCount)
{
    if (tsdOpened_) {
        BM_LOG_INFO("tsd already opened.");
        return true;
    }
    auto res = DlHccpApi::TsdOpen(deviceId, rankCount);
    if (res != 0) {
        BM_LOG_ERROR("TsdOpen for (deviceId=" << deviceId << ", rankCount=" << rankCount << ") failed: " << res);
        return false;
    }

    tsdOpened_ = true;
    return true;
}

bool RdmaTransportManager::RaInit(uint32_t deviceId)
{
    if (raInitialized_) {
        BM_LOG_INFO("ra already initialized.");
        return true;
    }

    HccpRaInitConfig initConfig{};
    initConfig.phyId = deviceId;
    initConfig.nicPosition = static_cast<uint32_t>(HccpNetworkMode::NETWORK_OFFLINE);
    initConfig.hdcType = 6;  // HDC_SERVICE_TYPE_RDMA = 6
    BM_LOG_INFO("RaInit=" << initConfig);
    auto ret = DlHccpApi::RaInit(initConfig);
    if (ret != 0) {
        BM_LOG_ERROR("Hccp Init RA failed: " << ret);
        return false;
    }

    raInitialized_ = true;
    return true;
}

bool RdmaTransportManager::RetireDeviceIp(uint32_t deviceId, in_addr &deviceIp)
{
    static in_addr retiredIp{};

    if (deviceIpRetired_) {
        BM_LOG_INFO("device ip already retired : " << inet_ntoa(retiredIp));
        deviceIp = retiredIp;
        return true;
    }

    uint32_t count = 0;
    std::vector<HccpInterfaceInfo> infos;

    HccpRaGetIfAttr config;
    config.phyId = deviceId;
    config.nicPosition = static_cast<uint32_t>(HccpNetworkMode::NETWORK_OFFLINE);
    config.isAll = true;

    auto ret = DlHccpApi::RaGetIfNum(config, count);
    if (ret != 0 || count == 0) {
        BM_LOG_ERROR("get interface count failed: " << ret << ", count: " << count);
        return false;
    }

    infos.resize(count);
    ret = DlHccpApi::RaGetIfAddrs(config, infos.data(), count);
    if (ret != 0) {
        BM_LOG_ERROR("get interface information failed: " << ret);
        return false;
    }

    for (auto &info : infos) {
        if (info.family == AF_INET) {
            deviceIp = retiredIp = info.ifaddr.ip.addr;
            deviceIpRetired_ = true;
            BM_LOG_INFO("retire device ip success : " << inet_ntoa(deviceIp));
            return true;
        }
    }

    BM_LOG_ERROR("not found network device of AF_INET on NPU.");
    return false;
}

bool RdmaTransportManager::RaRdevInit(uint32_t deviceId, in_addr deviceIp, void *&rdmaHandle)
{
    if (storedRdmaHandle_ != nullptr) {
        BM_LOG_INFO("ra rdev already initialized.");
        rdmaHandle = storedRdmaHandle_;
        return true;
    }

    HccpRdevInitInfo info{};
    HccpRdev rdev{};

    info.mode = static_cast<int>(HccpNetworkMode::NETWORK_OFFLINE);
    info.notifyType = static_cast<uint32_t>(HccpNotifyType::NOTIFY);
    info.enabled2mbLite = true;
    rdev.phyId = deviceId;
    rdev.family = AF_INET;
    rdev.localIp.addr = deviceIp;
    BM_LOG_INFO("RaRdevInitV2, info=" << info << "rdev=" << rdev);
    auto ret = DlHccpApi::RaRdevInitV2(info, rdev, rdmaHandle);
    if (ret != 0) {
        BM_LOG_ERROR("Hccp Init RDev failed: " << ret);
        return false;
    }

    storedRdmaHandle_ = rdmaHandle;
    BM_LOG_INFO("initialize RDev success, rdmaHandle: " << rdmaHandle);
    return true;
}

void RdmaTransportManager::ClearAllRegisterMRs()
{
    std::unique_lock<std::mutex> uniqueLock{mrsMutex_};
    for (auto it = registerMRS_.begin(); it != registerMRS_.end(); ++it) {
        auto ret = DlHccpApi::RaDeregisterMR(rdmaHandle_, it->second.mrHandle);
        if (ret != 0) {
            BM_LOG_WARN("Unregister:" << (void *)(ptrdiff_t)it->first << " : " << it->second << " failed: " << ret);
        }
    }
    registerMRS_.clear();
}
}
}
}
}