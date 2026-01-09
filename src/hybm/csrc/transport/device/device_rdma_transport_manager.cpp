/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#include "device_rdma_transport_manager.h"

#include <unistd.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>

#include "hybm_common_include.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "dl_hccp_api.h"
#include "hybm_ptracer.h"
#include "device_rdma_common.h"
#include "device_rdma_helper.h"
#include "fixed_ranks_qp_manager.h"
#include "bipartite_ranks_qp_manager.h"
#include "joinable_ranks_qp_manager.h"
#include "hybm_gva.h"
#include "hybm_va_manager.h"

namespace {
constexpr auto QP_READY_CHECK_TIMEOUT_BASE = std::chrono::seconds(30);
constexpr auto QP_READY_CHECK_TIMEOUT_PER_RANK = std::chrono::milliseconds(100);
constexpr auto QP_READY_CHECK_INTERVAL = std::chrono::milliseconds(5);
} // namespace

namespace ock {
namespace mf {

namespace transport {
namespace device {

constexpr int RT_STARS_WRITE_VALUE_SIZE_TYPE_64BIT = 3;
constexpr int RT_STARS_WRITE_VALUE_SUB_TYPE_RDMA_DB_SEND = 2;
constexpr int RT_STARS_SQE_TYPE_INVALID = 63;
constexpr unsigned long RT_ASCEND910B1_ROCEE_BASE_ADDR = 0x2000000000UL;
constexpr unsigned long RT_ASCEND910B1_ROCEE_VF_DB_CFG0_REG = 0x230UL;

thread_local HybmStreamNotifyPtr RdmaTransportManager::notify_ = nullptr;

RdmaTransportManager::~RdmaTransportManager()
{
    ClearAllRegisterMRs();
}

Result RdmaTransportManager::OpenDevice(const TransportOptions &options)
{
    int32_t userId = -1;
    int32_t logicId = -1;

    BM_LOG_DEBUG("begin to open device with " << options);
    auto ret = DlAclApi::AclrtGetDevice(&userId);
    BM_ASSERT_LOG_AND_RETURN(ret == 0 && userId >= 0,
                             "AclrtGetDevice() return=" << ret << ", output deviceId=" << userId,
                             BM_DL_FUNCTION_FAILED);

    ret = DlAclApi::RtGetLogicDevIdByUserDevId(userId, &logicId);
    BM_ASSERT_LOG_AND_RETURN(ret == 0 && logicId >= 0,
                             "RtGetLogicDevIdByUserDevId() return=" << ret << ", output deviceId=" << logicId,
                             BM_DL_FUNCTION_FAILED);

    deviceId_ = static_cast<uint32_t>(logicId);
    rankId_ = options.rankId;
    rankCount_ = options.rankCount;
    role_ = options.role;
    ret = ParseDeviceNic(options.nic, devicePort_);
    BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "parse input nic(" << options.nic << ") failed!", BM_INVALID_PARAM);

    if (!PrepareOpenDevice(userId, deviceId_, rankCount_, deviceIp_, rdmaHandle_)) {
        BM_LOG_ERROR("PrepareOpenDevice failed.");
        return BM_ERROR;
    }

    nicInfo_ = GenerateDeviceNic(deviceIp_, devicePort_);
    BM_ASSERT_LOG_AND_RETURN(
        !nicInfo_.empty(),
        "GenerateDeviceNic failed, deviceIp=" << DescribeIPv4(deviceIp_) << ", devicePort=" << devicePort_, BM_ERROR);

    sockaddr_in deviceAddr{};
    deviceAddr.sin_family = AF_INET;
    deviceAddr.sin_addr = deviceIp_;
    deviceAddr.sin_port = devicePort_;
    if (role_ == HYBM_ROLE_PEER) {
        if (options.initialType == HYBM_TYPE_AI_CORE_INITIATE) {
            qpManager_ = std::make_shared<FixedRanksQpManager>(userId, rankId_, rankCount_, deviceAddr);
        } else {
            qpManager_ = std::make_shared<JoinableRanksQpManager>(userId, deviceId_, rankId_, rankCount_, deviceAddr);
        }
    } else {
        qpManager_ = std::make_shared<BipartiteRanksQpManager>(userId, deviceId_, rankId_, rankCount_, deviceAddr,
                                                               role_ == HYBM_ROLE_RECEIVER);
    }

    deviceChipInfo_ = std::make_shared<DeviceChipInfo>(userId);
    ret = deviceChipInfo_->Init();
    BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "device info init failed: " << ret, ret);

    ret = InitStreamNotifyBuf();
    BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "notify init failed: " << ret, ret);
    ranksMRs_.resize(rankCount_);
    BM_LOG_INFO("open device with " << options << " success.");
    return BM_OK;
}

Result RdmaTransportManager::CloseDevice()
{
    if (qpManager_ != nullptr) {
        qpManager_->Shutdown();
        qpManager_ = nullptr;
    }
    return BM_OK;
}

Result RdmaTransportManager::RegisterMemoryRegion(const TransportMemoryRegion &mr)
{
    void *mrHandle = nullptr;
    HccpMrInfo info{};

    auto ret = ConvertHccpMrInfo(mr, info);
    if (ret != BM_OK) {
        return ret;
    }

    ret = DlHccpApi::RaRegisterMR(rdmaHandle_, &info, mrHandle);
    if (ret != 0) {
        BM_LOG_ERROR("register MR=" << mr << " failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    RegMemResult result{mr.addr, (uint64_t)(ptrdiff_t)info.addr, mr.size, mrHandle, info.lkey, info.rkey};
    BM_LOG_DEBUG("register MR result=" << result);
    if ((mr.flags & REG_MR_FLAG_SELF) == 0) {
        auto type = (mr.flags & REG_MR_FLAG_DRAM) ? HYBM_MEM_TYPE_HOST : HYBM_MEM_TYPE_DEVICE;
        ret = HybmVaManager::GetInstance().AddVaInfoFromExternal({result.regAddress, mr.size, type, mr.addr}, rankId_);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Add va info failed, ret: " << ret << ", gva: " << result.regAddress);
            DlHccpApi::RaDeregisterMR(rdmaHandle_, mrHandle);
            return ret;
        }
    }

    WriteGuard lockGuard(lock_);
    registerMRS_.emplace(mr.addr, result);
    return BM_OK;
}

Result RdmaTransportManager::UnregisterMemoryRegion(uint64_t addr)
{
    HybmVaManager::GetInstance().RemoveOneVaInfo(addr);
    WriteGuard lockGuard(lock_);
    auto pos = registerMRS_.find(addr);
    if (pos == registerMRS_.end()) {
        BM_LOG_ERROR("input address not register!");
        return BM_INVALID_PARAM;
    }

    auto ret = DlHccpApi::RaDeregisterMR(rdmaHandle_, pos->second.mrHandle);
    if (ret != 0) {
        BM_LOG_ERROR("Unregister MR addr failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    if (pos->second.address != pos->second.regAddress) {
        ret = DlHalApi::HalHostUnregisterEx((void *)(ptrdiff_t)pos->second.address, deviceId_, HOST_MEM_MAP_DEV);
        if (ret != 0) {
            BM_LOG_ERROR("HalHostUnregister failed: " << ret);
        }
    }

    registerMRS_.erase(pos);
    return BM_OK;
}

bool RdmaTransportManager::QueryHasRegistered(uint64_t addr, uint64_t size)
{
    ReadGuard lockGuard(lock_);
    auto pos = registerMRS_.lower_bound(addr);
    if (pos == registerMRS_.end() || pos->first + pos->second.size < addr + size) {
        return false;
    }
    return true;
}

Result RdmaTransportManager::QueryMemoryKey(uint64_t addr, TransportMemoryKey &key)
{
    RegMemKeyUnion keyUnion{};
    ReadGuard lockGuard(lock_);
    auto pos = registerMRS_.lower_bound(addr);
    if (pos == registerMRS_.end() || pos->first + pos->second.size <= addr) {
        BM_LOG_ERROR("input address not register, addr:" << std::hex << addr);
        return BM_INVALID_PARAM;
    }

    keyUnion.deviceKey = pos->second;
    keyUnion.deviceKey.notifyAddr = notifyInfo_.srcAddr;
    keyUnion.deviceKey.notifyRkey = notifyInfo_.srcRkey;

    key = keyUnion.commonKey;
    BM_LOG_INFO("Success to query memory key rank:" << rankId_ << " addr:" << std::hex << keyUnion.deviceKey.address
        << " size:" << keyUnion.deviceKey.size);
    return BM_OK;
}

Result RdmaTransportManager::Prepare(const HybmTransPrepareOptions &options)
{
    int ret;
    if ((ret = CheckPrepareOptions(options)) != 0) {
        return ret;
    }
    BM_ASSERT_RETURN(qpManager_ != nullptr, BM_MALLOC_FAILED);
    sockaddr_in deviceNetwork;
    std::unordered_map<uint32_t, ConnectRankInfo> rankInfo;
    for (auto it = options.options.begin(); it != options.options.end(); ++it) {
        ret = ParseDeviceNic(it->second.nic, deviceNetwork);
        if (ret != BM_OK) {
            BM_LOG_ERROR("parse networks[" << it->first << "]=" << it->second.nic << " failed: " << ret);
            return BM_INVALID_PARAM;
        }

        rankInfo.emplace(it->first, ConnectRankInfo{it->second.role, deviceNetwork, it->second.memKeys});
    }

    ret = qpManager_->SetRemoteRankInfo(rankInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("qp manager set remote rank info failed: " << ret);
        return ret;
    }

    ret = qpManager_->Startup(rdmaHandle_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("qp manager startup failed: " << ret);
        return ret;
    }

    OptionsToRankMRs(options);
    return BM_OK;
}

Result RdmaTransportManager::RemoveRanks(const std::vector<uint32_t> &removedRanks)
{
    BM_ASSERT_RETURN(qpManager_ != nullptr, BM_MALLOC_FAILED);
    std::unordered_set<uint32_t> ranksSet;
    std::unordered_map<uint32_t, MemoryRegionMap> removedRankRegions;

    HybmStreamManager::DestroyAllThreadHybmStream(); // clean all stream, maybe has some task on stream

    WriteGuard lockGuard(lock_);
    for (auto rank : removedRanks) {
        if (rank >= rankCount_) {
            BM_LOG_ERROR("input rank is large than world size! rk:" << rank << " world_size:" << rankCount_);
            continue;
        }
        auto &pos = ranksMRs_[rank];
        if (!pos.empty()) {
            ranksSet.emplace(rank);
            removedRankRegions.emplace(rank, pos);
            pos.clear();
        }
        notifyRemoteInfo_[rank] = std::make_pair(0U, 0U);
    }

    if (ranksSet.empty()) {
        return BM_OK;
    }

    auto ret = qpManager_->RemoveRanks(ranksSet);
    if (ret != BM_OK) {
        BM_LOG_ERROR("qp manager remove ranks failed: " << ret);
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

    return WaitQpReady();
}

Result RdmaTransportManager::AsyncConnect()
{
    return BM_OK;
}

Result RdmaTransportManager::WaitForConnected(int64_t timeoutNs)
{
    if (qpManager_ == nullptr) {
        BM_LOG_ERROR("server side not listen!");
        return BM_ERROR;
    }

    auto ret = qpManager_->WaitingConnectionReady();
    if (ret != BM_OK) {
        BM_LOG_ERROR("wait for server side connected on device failed: " << ret);
        return ret;
    }

    return BM_OK;
}

Result RdmaTransportManager::WaitQpReady()
{
    BM_ASSERT_RETURN(qpManager_ != nullptr, BM_MALLOC_FAILED);
    std::vector<uint32_t> rankIds;
    {
        ReadGuard lockGuard(lock_);
        for (uint32_t i = 0; i < rankCount_; i++) {
            if (!ranksMRs_[i].empty()) {
                rankIds.emplace_back(i);
            }
        }
    }

    auto timeout = QP_READY_CHECK_TIMEOUT_BASE + QP_READY_CHECK_TIMEOUT_PER_RANK * rankIds.size();
    auto expire_time = std::chrono::steady_clock::now() + timeout;
    uint64_t tries = 0;
    while (std::chrono::steady_clock::now() < expire_time) {
        if (qpManager_->CheckQpReady(rankIds)) {
            return BM_OK;
        }
        tries++;
        std::this_thread::sleep_for(QP_READY_CHECK_INTERVAL);
    }
    BM_LOG_ERROR("CheckQpReady timeout: " << (std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count())
                                          << "ms after " << tries << " tries.");
    return BM_TIMEOUT;
}

Result RdmaTransportManager::UpdateRankOptions(const HybmTransPrepareOptions &options)
{
    if (qpManager_ == nullptr) {
        BM_LOG_ERROR("qp manager not created");
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
        ranksInfo.emplace(it->first, ConnectRankInfo{it->second.role, deviceNetwork, it->second.memKeys});
    }

    auto ret = qpManager_->SetRemoteRankInfo(ranksInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("update rank options failed: " << ret);
        return ret;
    }

    OptionsToRankMRs(options);

    return WaitQpReady();
}

const std::string &RdmaTransportManager::GetNic() const
{
    return nicInfo_;
}

const void *RdmaTransportManager::GetQpInfo() const
{
    if (qpManager_ == nullptr) {
        BM_LOG_ERROR("GetQpInfo(): connection manager not created.");
        return nullptr;
    }
    return qpManager_->GetQpInfoAddress();
}

Result RdmaTransportManager::ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    auto ret = RemoteIO(rankId, lAddr, rAddr, size, false, true);
    if (ret != BM_OK) {
        BM_LOG_ERROR("ReadRemote() failed: " << ret);
        return ret;
    }

    BM_LOG_DEBUG("ReadRemote() success. size=" << size);
    return BM_OK;
}

Result RdmaTransportManager::WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    auto ret = RemoteIO(rankId, lAddr, rAddr, size, true, true);
    if (ret != BM_OK) {
        BM_LOG_ERROR("WriteRemote() failed: " << ret);
        return ret;
    }

    BM_LOG_DEBUG("WriteRemote() success. size=" << size);
    return BM_OK;
}

Result RdmaTransportManager::ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    TP_TRACE_BEGIN(TP_HYBM_DEV_RDMA_ASYNC_READ);
    auto ret = RemoteIO(rankId, lAddr, rAddr, size, false, false);
    TP_TRACE_END(TP_HYBM_DEV_RDMA_ASYNC_READ, ret);
    if (ret != BM_OK) {
        BM_LOG_ERROR("ReadRemoteAsync() failed: " << ret);
        return ret;
    }
    return BM_OK;
}

Result RdmaTransportManager::WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    TP_TRACE_BEGIN(TP_HYBM_DEV_RDMA_ASYNC_WRITE);
    auto ret = RemoteIO(rankId, lAddr, rAddr, size, true, false);
    TP_TRACE_END(TP_HYBM_DEV_RDMA_ASYNC_WRITE, ret);
    if (ret != BM_OK) {
        BM_LOG_ERROR("WriteRemoteAsync() failed: " << ret);
        return ret;
    }
    return BM_OK;
}

Result RdmaTransportManager::Synchronize(uint32_t rankId)
{
    BM_ASSERT_RETURN(qpManager_ != nullptr, BM_MALLOC_FAILED);
    auto qp = qpManager_->GetQpHandleWithRankId(rankId);
    if (qp == nullptr) {
        BM_LOG_ERROR("no qp to rankId: " << rankId);
        return BM_ERROR;
    }

    auto ret = Synchronize(qp->qpHandle, rankId);
    qpManager_->PutQpHandle(qp);
    return ret;
}

bool RdmaTransportManager::PrepareOpenDevice(uint32_t userId, uint32_t device, uint32_t rankCount, in_addr &deviceIp,
                                             void *&rdmaHandle)
{
    // If can get rdmaHandle, maybe the device has been opened, can try get rdmaHandle directly.
    if (DlHccpApi::RaRdevGetHandle(device, rdmaHandle) == 0) {
        if (rdmaHandle != nullptr) {
            if (!RetireDeviceIp(device, deviceIp)) {
                BM_LOG_ERROR("RetireDeviceIp failed.");
                return false;
            }
            BM_LOG_INFO("Had prepared device and get rdmaHandle success.");
            return true;
        }
        BM_LOG_INFO("Had prepared device, but rdmaHandle is null, need init again.");
    }
    // OpenTsd need userDeviceId
    if (!OpenTsd(userId, rankCount)) {
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
    static bool tsdOpened = false;
    if (tsdOpened) {
        BM_LOG_INFO("tsd already opened.");
        return true;
    }

    auto res = DlHccpApi::TsdOpen(deviceId, rankCount);
    if (res != 0) {
        BM_LOG_ERROR("TsdOpen for (deviceId=" << deviceId << ", rankCount=" << rankCount << ") failed: " << res);
        return false;
    }

    BM_LOG_DEBUG("open tsd for device id: " << deviceId << ", rank count: " << rankCount << " success.");
    tsdOpened = true;
    return true;
}

bool RdmaTransportManager::RaInit(uint32_t deviceId)
{
    static bool raInitialized = false;
    if (raInitialized) {
        BM_LOG_INFO("ra already initialized.");
        return true;
    }
    const std::chrono::seconds WAIT_TIME(3);
    HccpRaInitConfig initConfig{};
    initConfig.phyId = deviceId;
    initConfig.nicPosition = NETWORK_OFFLINE;
    initConfig.hdcType = 6; // HDC_SERVICE_TYPE_RDMA = 6  HDC_SERVICE_TYPE_RDMA_V2=18
    BM_LOG_DEBUG("RaInit=" << initConfig);
    std::this_thread::sleep_for(WAIT_TIME); // avoid hccl init conflict
    auto ret = DlHccpApi::RaInit(initConfig);
    if (ret != 0) {
        BM_LOG_WARN("Hccp Init RA failed: " << ret << " devid:" << deviceId);
        std::this_thread::sleep_for(WAIT_TIME);
        raInitialized = true;
        return true;
    }

    BM_LOG_DEBUG("ra init for device id: " << deviceId << " success.");
    raInitialized = true;
    return true;
}

bool RdmaTransportManager::RetireDeviceIp(uint32_t deviceId, in_addr &deviceIp)
{
    static bool deviceIpRetired = false;
    static in_addr retiredIp{};

    if (deviceIpRetired) {
        BM_LOG_INFO("device ip already retired : " << DescribeIPv4(retiredIp));
        deviceIp = retiredIp;
        return true;
    }

    uint32_t count = 0;
    std::vector<HccpInterfaceInfo> infos;

    HccpRaGetIfAttr config;
    config.phyId = deviceId;
    config.nicPosition = NETWORK_OFFLINE;
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
            deviceIpRetired = true;
            BM_LOG_DEBUG("retire device ip success : " << DescribeIPv4(deviceIp));
            return true;
        }
    }

    BM_LOG_ERROR("not found network device of AF_INET on NPU.");
    return false;
}

bool RdmaTransportManager::RaRdevInit(uint32_t deviceId, in_addr deviceIp, void *&rdmaHandle)
{
    static void *storedRdmaHandle = nullptr;
    if (storedRdmaHandle != nullptr) {
        BM_LOG_INFO("ra rdev already initialized.");
        rdmaHandle = storedRdmaHandle;
        return true;
    }

    HccpRdevInitInfo info{};
    HccpRdev rdev{};

    info.mode = NETWORK_OFFLINE;
    info.notifyType = NOTIFY;
    info.enabled2mbLite = false;
    rdev.phyId = deviceId;
    rdev.family = AF_INET;
    rdev.localIp.addr = deviceIp;
    BM_LOG_DEBUG("RaRdevInitV2, info=" << info << "rdev=" << rdev);
    auto ret = DlHccpApi::RaRdevInitV2(info, rdev, rdmaHandle);
    if (ret != 0) {
        BM_LOG_ERROR("Hccp Init RDev failed: " << ret);
        return false;
    }

    storedRdmaHandle = rdmaHandle;
    BM_LOG_INFO("initialize RDev success.");
    return true;
}

void RdmaTransportManager::ClearAllRegisterMRs()
{
    WriteGuard lockGuard(lock_);
    for (auto it = registerMRS_.begin(); it != registerMRS_.end(); ++it) {
        auto ret = DlHccpApi::RaDeregisterMR(rdmaHandle_, it->second.mrHandle);
        if (ret != 0) {
            BM_LOG_WARN("Unregister:" << (void *)(ptrdiff_t)it->first << " : " << it->second << " failed: " << ret);
        }
    }
    registerMRS_.clear();
}

int RdmaTransportManager::CheckPrepareOptions(const ock::mf::transport::HybmTransPrepareOptions &options)
{
    if (role_ != HYBM_ROLE_PEER) {
        BM_LOG_INFO("transport role: " << role_ << " check options passed.");
        return BM_OK;
    }

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

    return BM_OK;
}

int RdmaTransportManager::RemoteIO(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size, bool write,
                                   bool sync)
{
    if (qpManager_ == nullptr) {
        BM_LOG_ERROR("ReadRemote(): connection manager not created.");
        return BM_ERROR;
    }
    auto qp = qpManager_->GetQpHandleWithRankId(rankId);
    if (qp == nullptr) {
        BM_LOG_ERROR("no qp to rankId: " << rankId);
        return BM_ERROR;
    }

    auto hStream = HybmStreamManager::GetThreadHybmStream(HybmGetInitedLogicDeviceId());
    BM_ASSERT_RETURN(hStream != nullptr, BM_ERROR);

    struct send_wr_v2 wr = {};
    struct sg_list sgList = {.addr = lAddr, .len = (uint32_t)size, .lkey = 0};
    wr.buf_list = &sgList;
    wr.buf_num = 1; // 此处list只有一个，设置为1
    wr.dst_addr = rAddr;
    wr.op = write ? 0 : 4; /* RDMA_WRITE: 0  RDMA_READ: 4 */
    wr.send_flag = RA_SEND_SIGNALED;
    wr.wr_id = wrIdx_.fetch_add(1U);
    auto ret = CorrectHostRegWr(rankId, lAddr, rAddr, size, wr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("CorrectHostRegWr failed : " << ret);
        qpManager_->PutQpHandle(qp);
        return ret;
    }

    send_wr_rsp rspInfo{};
    TP_TRACE_BEGIN(TP_HYBM_DEV_SEND_WR);
    ret = DlHccpApi::RaSendWrV2(qp->qpHandle, &wr, &rspInfo);
    TP_TRACE_END(TP_HYBM_DEV_SEND_WR, ret);
    if (ret != 0) {
        BM_LOG_ERROR("DlHccpApi::RaSendWr(handle, &wr, &opRsp) failed: " << ret);
        qpManager_->PutQpHandle(qp);
        return ret;
    }

    StreamTask task;
    task.type = STREAM_TASK_TYPE_RDMA;
    ConstructSqeNoSinkModeForRdmaDbSendTask(rspInfo, task.sqe, hStream);
    TP_TRACE_BEGIN(TP_HYBM_DEV_SUBMIT_TASK);
    ret = hStream->SubmitTasks(task);
    TP_TRACE_END(TP_HYBM_DEV_SUBMIT_TASK, ret);
    if (ret != BM_OK) {
        BM_LOG_ERROR("SubmitTasks(task) failed: " << ret);
        qpManager_->PutQpHandle(qp);
        return ret;
    }

    if (sync) {
        ret = Synchronize(qp->qpHandle, rankId);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Synchronize failed: " << ret);
        }
    }
    qpManager_->PutQpHandle(qp);
    return ret;
}

int RdmaTransportManager::GetRegAddress(const MemoryRegionMap &map, uint64_t inputAddr, uint64_t size, bool isLocal,
                                        uint64_t &outputAddr, uint32_t &mrKey) const
{
    auto pos = map.lower_bound(inputAddr);
    if (pos == map.end() || pos->first + pos->second.size < inputAddr + size) {
        BM_LOG_ERROR("[GetRegAddress] Input address not register: size: " << size);
        return BM_INVALID_PARAM;
    }
    outputAddr = pos->second.regAddress + (inputAddr - pos->first);
    mrKey = (isLocal ? pos->second.lkey : pos->second.rkey);
    return BM_OK;
}

int RdmaTransportManager::CorrectHostRegWr(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size,
                                           send_wr_v2 &wr)
{
    ReadGuard lockGuard(lock_);
    auto ret = GetRegAddress(registerMRS_, lAddr, size, true, wr.buf_list->addr, wr.buf_list->lkey);
    if (ret != BM_OK) {
        BM_LOG_ERROR("lAddr not register: size: " << size);
        return ret;
    }
    auto &it = ranksMRs_[rankId];
    if (it.empty()) {
        BM_LOG_ERROR("input rankId: " << rankId << " not found.");
        return BM_INVALID_PARAM;
    }
    ret = GetRegAddress(it, rAddr, size, false, wr.dst_addr, wr.rkey);
    if (ret != BM_OK) {
        BM_LOG_ERROR("rAddr not register: size: " << size);
        return ret;
    }

    return BM_OK;
}

int RdmaTransportManager::ConvertHccpMrInfo(const TransportMemoryRegion &mr, HccpMrInfo &info)
{
    auto addr = mr.addr;
    // need register: dram except gvm
    if ((mr.flags & REG_MR_FLAG_DRAM) || (mr.flags & REG_MR_FLAG_ACL_DRAM)) {
        auto input = (void *)(ptrdiff_t)addr;
        void *output = nullptr;
        auto ret = DlHalApi::HalHostRegister(input, mr.size, HOST_MEM_MAP_DEV, deviceId_, &output);
        if (ret != 0) {
            BM_LOG_ERROR("HalHostRegister failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }
        addr = (uint64_t)(ptrdiff_t)output;
    }

    info.addr = (void *)(ptrdiff_t)addr;
    info.size = mr.size;
    info.access = mr.access;
    info.lkey = 0;
    info.rkey = 0;

    return BM_OK;
}

void RdmaTransportManager::OptionsToRankMRs(const HybmTransPrepareOptions &options)
{
    RegMemKeyUnion keyUnion{};
    WriteGuard lockGuard(lock_);
    for (auto it = options.options.begin(); it != options.options.end(); ++it) {
        auto node = it->first;
        if (node >= rankCount_) {
            BM_LOG_ERROR("input rank is large than world size! rk:" << node << " world_size:" << rankCount_);
            continue;
        }

        auto &pos = ranksMRs_[node];
        for (auto &key : it->second.memKeys) {
            keyUnion.commonKey = key;
            auto &devKey = keyUnion.deviceKey;
            BM_LOG_INFO("Success to query memory key rank:" << node << " addr:" << std::hex
                << keyUnion.deviceKey.address << " size:" << keyUnion.deviceKey.size);
            auto addrIter = pos.find(devKey.address);
            if (addrIter == pos.end()) {
                pos.emplace(devKey.address, devKey);
            } else {
                BM_LOG_INFO("OptionsToRankMRs devKey address: " << (void *)(ptrdiff_t)devKey.address
                                                                << " already exists, skip emplace.");
                continue;
            }
            if (devKey.notifyAddr != 0ULL) {
                notifyRemoteInfo_[node] = std::make_pair(devKey.notifyAddr, devKey.notifyRkey);
            }
        }
    }
}

void RdmaTransportManager::ConstructSqeNoSinkModeForRdmaDbSendTask(const send_wr_rsp &rspInfo, rtStarsSqe_t &command,
                                                                   HybmStreamPtr st)
{
    static std::atomic<uint32_t> taskIdGenerator{1};
    auto sqe = &command.writeValueSqe;

    auto taskId = taskIdGenerator.fetch_add(1);
    explicit_bzero(sqe, sizeof(rtStarsSqe_t));
    sqe->header.type = RT_STARS_SQE_TYPE_WRITE_VALUE;
    sqe->header.ie = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.pre_p = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.post_p = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.wr_cqe = st->GetWqeFlag();
    sqe->header.rt_stream_id = static_cast<uint16_t>(st->GetId());
    sqe->header.task_id = taskId;

    sqe->va = 0U;
    sqe->kernel_credit = RT_STARS_DEFAULT_KERNEL_CREDIT;
    sqe->awsize = RT_STARS_WRITE_VALUE_SIZE_TYPE_64BIT;
    sqe->sub_type = RT_STARS_WRITE_VALUE_SUB_TYPE_RDMA_DB_SEND;

    uint64_t dbVal = rspInfo.db.db_info;
    uint64_t dbAddr = GetRoceDbAddrForRdmaDbSendTask();
    if (dbAddr == 0ULL) {
        sqe->header.type = RT_STARS_SQE_TYPE_INVALID;
        BM_LOG_ERROR("generate db address is zero.");
        return;
    }

    sqe->write_value_part0 = static_cast<uint32_t>(dbVal & MASK_32_BIT);
    sqe->write_value_part1 = static_cast<uint32_t>(dbVal >> UINT32_BIT_NUM);
    sqe->write_addr_low = static_cast<uint32_t>(dbAddr & MASK_32_BIT);
    sqe->write_addr_high = static_cast<uint32_t>((dbAddr >> UINT32_BIT_NUM) & MASK_17_BIT);
}

uint64_t RdmaTransportManager::GetRoceDbAddrForRdmaDbSendTask()
{
    BM_ASSERT_RETURN(deviceChipInfo_ != nullptr, BM_MALLOC_FAILED);

    auto chipId = deviceChipInfo_->GetChipId();
    auto dieId = deviceChipInfo_->GetDieId();
    auto chipAddr = deviceChipInfo_->GetChipAddr();
    auto chipOffset = deviceChipInfo_->GetChipOffset();
    auto dieOffset = deviceChipInfo_->GetDieOffset();

    const uint64_t dbAddr = RT_ASCEND910B1_ROCEE_BASE_ADDR + RT_ASCEND910B1_ROCEE_VF_DB_CFG0_REG +
                            chipOffset * static_cast<uint64_t>(chipId) + dieOffset * dieId + chipAddr;
    return dbAddr;
}

int32_t RdmaTransportManager::InitStreamNotifyBuf()
{
    uint32_t notifySize = 4U;
    uint32_t notifyVal = 1U;
    uint64_t va;
    uint64_t size;
    void *ptr;
    auto ret = DlHccpApi::RaGetNotifyBaseAddr(rdmaHandle_, &va, &size);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "RaGetNotifyBaseAddr failed.", ret);

    HccpMrInfo info;
    ret = DlHccpApi::RaGetNotifyMrInfo(rdmaHandle_, &info);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "get notify mr failed.", ret);

    ret = DlAclApi::AclrtMalloc(&ptr, HYBM_LARGE_PAGE_SIZE, 0);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "alloc notify buf failed.", ret);

    ret = DlAclApi::AclrtMemcpy(ptr, notifySize, &notifyVal, notifySize, ACL_MEMCPY_HOST_TO_DEVICE);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "set notify val failed.", ret);

    void *mrHandle = nullptr;
    HccpMrInfo info2{ptr, HYBM_LARGE_PAGE_SIZE, RA_ACCESS_NORMAL, 0, 0};
    ret = DlHccpApi::RaRegisterMR(rdmaHandle_, &info2, mrHandle);
    if (ret != 0) {
        BM_LOG_ERROR("register notify mr failed: " << ret);
        DlAclApi::AclrtFree(ptr);
        return BM_DL_FUNCTION_FAILED;
    }

    notifyInfo_.notifyAddr = va;
    notifyInfo_.len = notifySize;
    notifyInfo_.notifyLkey = info.lkey;
    notifyInfo_.srcAddr = reinterpret_cast<uint64_t>(ptr);
    notifyInfo_.srcRkey = info2.rkey;

    notifyRemoteInfo_.resize(rankCount_);
    for (uint32_t i = 0; i < rankCount_; i++) {
        notifyRemoteInfo_[i] = std::make_pair(0U, 0U);
    }
    return BM_OK;
}

int32_t RdmaTransportManager::Synchronize(void *qpHandle, uint32_t rankId)
{
    auto hStream = HybmStreamManager::GetThreadHybmStream(HybmGetInitedLogicDeviceId());
    BM_ASSERT_RETURN(hStream != nullptr, BM_ERROR);
    auto &remoteMr = notifyRemoteInfo_[rankId];
    BM_ASSERT_LOG_AND_RETURN(remoteMr.second != 0, "remote notify not set! rank:" << rankId, BM_ERROR);

    if (notify_ == nullptr || notify_->GetStream() != hStream) {
        notify_ = std::make_shared<HybmStreamNotify>(hStream);
        BM_ASSERT_LOG_AND_RETURN(notify_ != nullptr, "notify create failed.", BM_ERROR);

        auto ret = notify_->Init();
        BM_ASSERT_LOG_AND_RETURN(ret == 0, "notify init failed.", ret);
    }

    struct send_wr_v2 wr = {};
    struct sg_list sgList = {
        .addr = notifyInfo_.notifyAddr + notify_->GetOffset(), .len = notifyInfo_.len, .lkey = notifyInfo_.notifyLkey};
    wr.wr_id = wrIdx_.fetch_add(1U);
    wr.buf_list = &sgList;
    wr.buf_num = 1; // 此处list只有一个，设置为1
    wr.dst_addr = remoteMr.first;
    wr.rkey = remoteMr.second;
    wr.op = 4; /* RDMA_WRITE: 0  RDMA_READ: 4 */
    wr.send_flag = RA_SEND_SIGNALED | RA_SEND_FENCE;

    send_wr_rsp rspInfo{};
    auto ret = DlHccpApi::RaSendWrV2(qpHandle, &wr, &rspInfo);
    if (ret != 0) {
        BM_LOG_ERROR("send notify wr failed: " << ret);
        return ret;
    }

    StreamTask task;
    task.type = STREAM_TASK_TYPE_RDMA;
    ConstructSqeNoSinkModeForRdmaDbSendTask(rspInfo, task.sqe, hStream);
    ret = hStream->SubmitTasks(task);
    if (ret != BM_OK) {
        BM_LOG_ERROR("SubmitTasks(task) failed: " << ret);
        return ret;
    }

    ret = notify_->Wait();
    return ret;
}

} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock