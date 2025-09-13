/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include "hybm_define.h"
#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "dl_hccp_api.h"
#include "device_rdma_common.h"
#include "device_rdma_helper.h"
#include "fixed_ranks_qp_manager.h"
#include "dynamic_ranks_qp_manager.h"
#include "device_rdma_transport_manager.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
bool RdmaTransportManager::tsdOpened_ = false;
bool RdmaTransportManager::deviceIpRetired_ = false;
bool RdmaTransportManager::raInitialized_ = false;
void* RdmaTransportManager::storedRdmaHandle_ = nullptr;

thread_local HybmStreamPtr RdmaTransportManager::stream_ = nullptr;

RdmaTransportManager::~RdmaTransportManager()
{
    ClearAllRegisterMRs();
    tsdOpened_ = false;
    raInitialized_ = false;
    deviceIpRetired_ = false;
}

int RdmaTransportManager::PrepareThreadLocalStream()
{
    if (stream_ != nullptr) {
        return BM_OK;
    }

    stream_ = HybmStreamManager::CreateStream(deviceId_, 0, 0);
    auto ret = stream_->Initialize();
    if (ret != BM_OK) {
        BM_LOG_ERROR("HybmStream init failed: " << ret);
        return ret;
    }
    return BM_OK;
}

Result RdmaTransportManager::OpenDevice(const TransportOptions &options)
{
    int32_t deviceId = -1;

    BM_LOG_DEBUG("begin to open device with " << options);
    auto ret = DlAclApi::AclrtGetDevice(&deviceId);
    if (ret != 0 || deviceId < 0) {
        BM_LOG_ERROR("AclrtGetDevice() return=" << ret << ", output deviceId=" << deviceId);
        return BM_DL_FUNCTION_FAILED;
    }
    deviceId_ = static_cast<uint32_t>(deviceId);
    rankId_ = options.rankId;
    rankCount_ = options.rankCount;
    role_ = options.role;
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
    if (role_ == HYBM_ROLE_PEER) {
        qpManager_ = std::make_shared<FixedRanksQpManager>(deviceId_, rankId_, rankCount_, deviceAddr);
    } else {
        qpManager_ = std::make_shared<DynamicRanksQpManager>(deviceId_, rankId_, rankCount_, deviceAddr,
                                                             role_ == HYBM_ROLE_RECEIVER);
    }

    deviceChipInfo_ = std::make_shared<DeviceChipInfo>(deviceId_);
    ret = deviceChipInfo_->Init();
    if (ret != BM_OK) {
        BM_LOG_ERROR("device info init failed: " << ret);
        return ret;
    }
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
    info.addr = (void *)(ptrdiff_t)mr.addr;
    info.size = mr.size;
    info.access = mr.access;
    auto ret = DlHccpApi::RaRegisterMR(rdmaHandle_, &info, mrHandle);
    if (ret != 0) {
        BM_LOG_ERROR("register MR=" << mr << " failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    RegMemResult result{mr.addr, mr.size, mrHandle, info.lkey, info.rkey};
    BM_LOG_DEBUG("register MR result=" << result);

    registerMRS_.emplace(mr.addr, result);
    ret = qpManager_->SetLocalMemories(registerMRS_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("qp manager set mr failed: " << ret);
        return ret;
    }

    return BM_OK;
}

Result RdmaTransportManager::UnregisterMemoryRegion(uint64_t addr)
{
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

    registerMRS_.erase(pos);
    ret = qpManager_->SetLocalMemories(registerMRS_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("qp manager set mr failed: " << ret);
        return ret;
    }
    return BM_OK;
}

Result RdmaTransportManager::QueryMemoryKey(uint64_t addr, TransportMemoryKey &key)
{
    RegMemKeyUnion keyUnion{};
    auto pos = registerMRS_.lower_bound(addr);
    if (pos == registerMRS_.end() || pos->first + pos->second.size <= addr) {
        BM_LOG_ERROR("input address not register");
        return BM_INVALID_PARAM;
    }

    keyUnion.deviceKey = pos->second;

    key = keyUnion.commonKey;
    return BM_OK;
}

Result RdmaTransportManager::ParseMemoryKey(const TransportMemoryKey &key, uint64_t &addr, uint64_t &size)
{
    RegMemKeyUnion keyUnion{};
    keyUnion.commonKey = key;
    if (keyUnion.deviceKey.type != TT_HCCP) {
        BM_LOG_ERROR("parse memory key type invalid: " << keyUnion.deviceKey.type);
        return BM_ERROR;
    }

    addr = keyUnion.deviceKey.address;
    size = keyUnion.deviceKey.size;
    return BM_OK;
}

Result RdmaTransportManager::Prepare(const HybmTransPrepareOptions &options)
{
    BM_LOG_DEBUG("RdmaTransportManager Prepare with : " << options);
    int ret;
    if ((ret = CheckPrepareOptions(options)) != 0) {
        return ret;
    }

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
    BM_LOG_DEBUG("SetRemoteRankInfo rankInfo.size=" << rankInfo.size());

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

Result RdmaTransportManager::UpdateRankOptions(const HybmTransPrepareOptions &options)
{
    BM_LOG_DEBUG("RdmaTransportManager Prepare with : " << options);
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
        BM_LOG_INFO("UpdateRankOptions update rank: " << it->first);
        ranksInfo.emplace(it->first, ConnectRankInfo{it->second.role, deviceNetwork, it->second.memKeys});
    }
    BM_LOG_DEBUG("UpdateRankOptions ranksInfo.size=" << ranksInfo.size());

    auto ret = qpManager_->SetRemoteRankInfo(ranksInfo);
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
    if (qpManager_ == nullptr) {
        BM_LOG_ERROR("GetQpInfo(): connection manager not created.");
        return nullptr;
    }
    return qpManager_->GetQpInfoAddress();
}

Result RdmaTransportManager::ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_LOG_DEBUG("=========== read remote for rankId=" << rankId << ", size=" << size);
    auto ret = RemoteIO(rankId, lAddr, rAddr, size, false);
    if (ret != BM_OK) {
        BM_LOG_ERROR("ReadRemote() failed: " << ret);
        return ret;
    }

    BM_LOG_INFO("ReadRemote() success.");
    return BM_OK;
}

Result RdmaTransportManager::WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_LOG_DEBUG("=========== write remote for rankId=" << rankId << ", size=" << size);
    auto ret = RemoteIO(rankId, lAddr, rAddr, size, true);
    if (ret != BM_OK) {
        BM_LOG_ERROR("WriteRemote() failed: " << ret);
        return ret;
    }

    BM_LOG_INFO("WriteRemote() success.");
    return BM_OK;
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

    BM_LOG_DEBUG("open tsd for device id: " << deviceId << ", rank count: " << rankCount << " success.");
    tsdOpened_ = true;
    return true;
}

bool RdmaTransportManager::RaInit(uint32_t deviceId)
{
    if (raInitialized_) {
        BM_LOG_INFO("ra already initialized.");
        return true;
    }
    const std::chrono::seconds WAIT_TIME(3);
    HccpRaInitConfig initConfig{};
    initConfig.phyId = deviceId;
    initConfig.nicPosition = NETWORK_OFFLINE;
    initConfig.hdcType = 6;  // HDC_SERVICE_TYPE_RDMA = 6
    BM_LOG_DEBUG("RaInit=" << initConfig);
    std::this_thread::sleep_for(WAIT_TIME); // avoid hccl init conflict
    auto ret = DlHccpApi::RaInit(initConfig);
    if (ret != 0) {
        BM_LOG_WARN("Hccp Init RA failed: " << ret);
        // maybe hccl have already initialized ra, wait 3s then return true.
        std::this_thread::sleep_for(WAIT_TIME);
        raInitialized_ = true;
        return true;
    }

    BM_LOG_DEBUG("ra init for device id: " << deviceId << " success.");
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
            deviceIpRetired_ = true;
            BM_LOG_DEBUG("retire device ip success : " << inet_ntoa(deviceIp));
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

    info.mode = NETWORK_OFFLINE;
    info.notifyType = NOTIFY;
    info.enabled2mbLite = true;
    rdev.phyId = deviceId;
    rdev.family = AF_INET;
    rdev.localIp.addr = deviceIp;
    BM_LOG_DEBUG("RaRdevInitV2, info=" << info << "rdev=" << rdev);
    auto ret = DlHccpApi::RaRdevInitV2(info, rdev, rdmaHandle);
    if (ret != 0) {
        BM_LOG_ERROR("Hccp Init RDev failed: " << ret);
        return false;
    }

    storedRdmaHandle_ = rdmaHandle;
    BM_LOG_INFO("initialize RDev success.");
    return true;
}

void RdmaTransportManager::ClearAllRegisterMRs()
{
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

int RdmaTransportManager::RemoteIO(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size, bool write)
{
    if (qpManager_ == nullptr) {
        BM_LOG_ERROR("ReadRemote(): connection manager not created.");
        return BM_ERROR;
    }
    auto qpHandle = qpManager_->GetQpHandleWithRankId(rankId);
    if (qpHandle == nullptr) {
        BM_LOG_ERROR("no qp to rankId: " << rankId);
        return BM_ERROR;
    }
    auto ret = PrepareThreadLocalStream();
    if (ret != BM_OK) {
        BM_LOG_ERROR("prepare stream error rankId: " << rankId);
        return ret;
    }

    struct send_wr wr = {};
    struct sg_list sgList = {.addr = lAddr, .len = (uint32_t)size, .lkey = 0};
    wr.buf_list = &sgList;
    wr.buf_num = 1;  // 此处list只有一个，设置为1
    wr.dst_addr = rAddr;
    wr.op = write ? 0 : 4; /* RDMA_WRITE: 0  RDMA_READ: 4 */
    wr.send_flag = RA_SEND_SIGNALED;
    send_wr_rsp rspInfo{};
    ret = DlHccpApi::RaSendWr(qpHandle, &wr, &rspInfo);
    if (ret != 0) {
        BM_LOG_ERROR("DlHccpApi::RaSendWr(handle, &wr, &opRsp) failed: " << ret);
        return ret;
    }

    StreamTask task;
    task.type = STREAM_TASK_TYPE_RDMA;
    ConstructSqeNoSinkModeForRdmaDbSendTask(rspInfo, task.sqe);
    ret = stream_->SubmitTasks(task);

    ret = stream_->Synchronize();
    if (ret != BM_OK) {
        BM_LOG_ERROR("stream_->Synchronize() failed: " << ret);
        return ret;
    }
    return BM_OK;
}

void RdmaTransportManager::ConstructSqeNoSinkModeForRdmaDbSendTask(const send_wr_rsp &rspInfo, rtStarsSqe_t &command)
{
    static std::atomic<uint32_t> taskIdGenerator{1};
    auto sqe = &command.writeValueSqe;
    const uint8_t RT_STARS_SQE_TYPE_WRITE_VALUE = 8;
    const uint8_t RT_STARS_WRITE_VALUE_SUB_TYPE_RDMA_DB_SEND = 2;

    auto taskId = taskIdGenerator.fetch_add(1);
    std::fill(reinterpret_cast<unsigned char*>(sqe), reinterpret_cast<unsigned char*>(sqe) + sizeof(rtStarsSqe_t), 0);
    sqe->header.type = RT_STARS_SQE_TYPE_WRITE_VALUE;
    sqe->header.ie = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.pre_p = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.post_p = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.wr_cqe = 0;  // stream->GetStarsWrCqeFlag();
    sqe->header.rt_stream_id = static_cast<uint16_t>(stream_->GetId());
    sqe->header.task_id = taskId;

    sqe->va = 0U;
    sqe->kernel_credit = RT_STARS_DEFAULT_KERNEL_CREDIT;
    sqe->awsize = 3U;    // RT_STARS_WRITE_VALUE_SIZE_TYPE_64BIT;
    sqe->sub_type = RT_STARS_WRITE_VALUE_SUB_TYPE_RDMA_DB_SEND;

    uint64_t dbVal = rspInfo.db.db_info;
    uint64_t dbAddr = GetRoceDbAddrForRdmaDbSendTask();
    if (dbAddr == 0ULL) {
        sqe->header.type = 63U;  // RT_STARS_SQE_TYPE_INVALID;
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
    uint32_t deviceId = deviceId_;

    auto chipId = deviceChipInfo_->GetChipId();
    auto dieId = deviceChipInfo_->GetDieId();
    auto chipAddr = deviceChipInfo_->GetChipAddr();
    auto chipOffset = deviceChipInfo_->GetChipOffset();
    auto dieOffset = deviceChipInfo_->GetDieOffset();

    constexpr uint64_t RT_ASCEND910B1_ROCEE_BASE_ADDR = 0x2000000000UL;
    constexpr uint64_t RT_ASCEND910B1_ROCEE_VF_DB_CFG0_REG = 0x230UL;

    uint64_t dbAddr = RT_ASCEND910B1_ROCEE_BASE_ADDR +
                      RT_ASCEND910B1_ROCEE_VF_DB_CFG0_REG +
                      chipOffset * static_cast<uint64_t>(chipId) +
                      dieOffset * dieId +
                      chipAddr;
    BM_LOG_INFO("deviceId=" << deviceId << ", die_id=" << dieId << ", db=0x" << std::hex << dbAddr);

    return dbAddr;
}
}
}
}
}
