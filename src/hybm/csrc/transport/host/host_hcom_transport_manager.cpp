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
#include "host_hcom_transport_manager.h"

#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <regex>
#include <sstream>
#include <arpa/inet.h>
#include "dl_hcom_api.h"
#include "host_hcom_common.h"
#include "host_hcom_helper.h"
#include "mf_tls_util.h"
#include "hybm_ptracer.h"
#include "hybm_va_manager.h"

using namespace ock::mf;
using namespace ock::mf::transport;
using namespace ock::mf::transport::host;

namespace {
#if defined(NO_XPU)
constexpr uint64_t HCOM_RECV_DATA_SIZE = 1024 * 1024UL;
constexpr uint64_t HCOM_SEND_QUEUE_SIZE = 16384UL;
constexpr uint64_t HCOM_RECV_QUEUE_SIZE = 16384UL;
constexpr uint64_t HCOM_COMPLETE_QUEUE_SIZE = 8192;
constexpr uint64_t HCOM_QUEUE_PRE_POST_SIZE = 1024UL;
constexpr uint8_t HCOM_TRANS_EP_SIZE = 1;
constexpr int8_t HCOM_THREAD_PRIORITY = -20;
#else
constexpr uint64_t HCOM_RECV_DATA_SIZE = 8192UL;
constexpr uint64_t HCOM_SEND_QUEUE_SIZE = 512UL;
constexpr uint64_t HCOM_RECV_QUEUE_SIZE = 512UL;
constexpr uint64_t HCOM_COMPLETE_QUEUE_SIZE = 8192;
constexpr uint64_t HCOM_QUEUE_PRE_POST_SIZE = 256UL;
constexpr uint8_t HCOM_TRANS_EP_SIZE = 16;
constexpr int8_t HCOM_THREAD_PRIORITY = -20;
#endif
const char *HCOM_RPC_SERVICE_NAME = "hybm_hcom_service";
} // namespace

hybm_tls_config HcomTransportManager::tlsConfig_ = {};
char HcomTransportManager::keyPass_[KEYPASS_MAX_LEN] = {0};
std::mutex HcomTransportManager::keyPassMutex = {};
thread_local HcomCounterStreamPtr HcomTransportManager::stream_ = nullptr;

static void CopyHcomOneSideKey(const OneSideKey &from, TransportMemoryKey &to)
{
    std::copy_n(from.keys, std::size(from.keys), to.keys);
    std::copy_n(from.tokens, std::size(from.tokens), to.keys + std::size(from.keys));
#if defined(NO_XPU)
    auto offset = std::size(from.tokens) + std::size(from.keys);
    std::copy_n(from.eid, URMA_EID_LENGTH, reinterpret_cast<uint8_t *>(to.keys + offset));
#endif
}

static void CopyHcomOneSideKey(TransportMemoryKey &from, OneSideKey &to)
{
    std::copy_n(from.keys, std::size(to.keys), to.keys);
    std::copy_n(from.keys + std::size(to.keys), std::size(to.tokens), to.tokens);
#if defined(NO_XPU)
    auto offset = std::size(to.tokens) + std::size(to.keys);
    std::copy_n(reinterpret_cast<uint8_t *>(from.keys + offset), URMA_EID_LENGTH, to.eid);
#endif
}

Result HcomTransportManager::OpenDevice(const TransportOptions &options)
{
    BM_ASSERT_RETURN(rpcService_ == 0, BM_OK);
    BM_ASSERT_RETURN(CheckTransportOptions(options) == BM_OK, BM_INVALID_PARAM);
    Service_Options opt{};
    opt.workerGroupMode = C_SERVICE_BUSY_POLLING;
    opt.maxSendRecvDataSize = HCOM_RECV_DATA_SIZE;
    opt.workerThreadPriority = HCOM_THREAD_PRIORITY;
    Service_Type enumProtocolType = HostHcomHelper::HybmDopTransHcomProtocol(options.protocol, options.nic);
    int ret = DlHcomApi::ServiceCreate(enumProtocolType, HCOM_RPC_SERVICE_NAME, opt, &rpcService_);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to create hcom service, nic: " << options.nic << " type: " << enumProtocolType
                                                            << " ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    BM_LOG_INFO("Create hcom service successful, nic: " << options.nic << " type: " << enumProtocolType);
    tlsConfig_ = options.tlsOption;
    DlHcomApi::ServiceSetTlsOptions(rpcService_, options.tlsOption.tlsEnable, C_SERVICE_TLS_1_3, C_SERVICE_AES_GCM_256,
                                    GetCertCallBack, GetPrivateKeyCallBack, GetCACallBack);
    DlHcomApi::SetUbsModeFunc(rpcService_, UbsHcomServiceUbcMode::C_SERVICE_HIGHBANDWIDTH);
    DlHcomApi::ServiceSetSendQueueSize(rpcService_, HCOM_SEND_QUEUE_SIZE);
    DlHcomApi::ServiceSetRecvQueueSize(rpcService_, HCOM_RECV_QUEUE_SIZE);
    DlHcomApi::ServiceSetQueuePrePostSize(rpcService_, HCOM_QUEUE_PRE_POST_SIZE);
    DlHcomApi::ServiceSetCompletionQueueDepth(rpcService_, HCOM_COMPLETE_QUEUE_SIZE);
    DlHcomApi::ServiceRegisterChannelBrokerHandler(rpcService_, TransportRpcHcomEndPointBroken, C_CHANNEL_RECONNECT, 1);
    DlHcomApi::ServiceRegisterHandler(rpcService_, C_SERVICE_REQUEST_RECEIVED, TransportRpcHcomRequestReceived, 1);
    DlHcomApi::ServiceRegisterHandler(rpcService_, C_SERVICE_REQUEST_POSTED, TransportRpcHcomRequestPosted, 1);
    DlHcomApi::ServiceRegisterHandler(rpcService_, C_SERVICE_READWRITE_DONE, TransportRpcHcomOneSideDone, 1);

    if (enumProtocolType != Service_Type::C_SERVICE_UBC) {
        std::string ipMask = localIp_ + "/32";
        DlHcomApi::ServiceSetDeviceIpMask(rpcService_, ipMask.c_str());
    }

    DlHcomApi::ServiceBind(rpcService_, localNic_.c_str(), TransportRpcHcomNewEndPoint);
    ret = DlHcomApi::ServiceStart(rpcService_);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to start hcom service, nic: " << localNic_ << " type: " << enumProtocolType
                                                           << " ret: " << ret);
        DlHcomApi::ServiceDestroy(rpcService_, HCOM_RPC_SERVICE_NAME);
        rpcService_ = 0;
        return BM_DL_FUNCTION_FAILED;
    }
    bmOptype_ = static_cast<hybm_data_op_type>(options.protocol);
    rankId_ = options.rankId;
    rankCount_ = options.rankCount;
    mrMutex_ = std::vector<std::mutex>(rankCount_);
    mrs_ = std::vector<std::vector<HcomMemoryRegion>>(rankCount_);
    channelMutex_ = std::vector<std::mutex>(rankCount_);
    nics_ = std::vector<std::string>(rankCount_, "");
    channels_ = std::vector<Hcom_Channel>(rankCount_, 0);
    return BM_OK;
}

Result HcomTransportManager::CloseDevice()
{
    DlHcomApi::SetExternalLogger([]([[maybe_unused]] int level, [[maybe_unused]] const char *msg) {});
    BM_ASSERT_RETURN(rpcService_ != 0, BM_OK);
    for (uint32_t i = 0; i < rankCount_; ++i) {
        if (channels_[i] != 0) {
            DisConnectHcomChannel(i, channels_[i]);
        }
    }

    mf::MfTlsUtil::CloseTlsLib();
    rpcService_ = 0;
    localNic_ = "";
    localIp_ = "";
    rankId_ = UINT32_MAX;
    rankCount_ = 0;
    mrMutex_.clear();
    mrs_.clear();
    channelMutex_.clear();
    nics_.clear();
    channels_.clear();
    return BM_OK;
}

#if defined(ASCEND_NPU) || defined(NVIDIA_GPU)
Result HcomTransportManager::RegisterMemoryRegion(const TransportMemoryRegion &mr)
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    BM_ASSERT_RETURN(mr.addr != 0 && mr.size != 0 && (mr.flags & transport::REG_MR_FLAG_DRAM), BM_INVALID_PARAM);

    HcomMemoryRegion info{};
    if (GetMemoryRegionByAddr(rankId_, mr.addr, info) == BM_OK) {
        BM_LOG_ERROR("Failed to register mem region, addr already registered");
        return BM_ERROR;
    }

    Service_MemoryRegion memoryRegion;
    int32_t ret = DlHcomApi::ServiceRegisterAssignMemoryRegion(rpcService_, mr.addr, mr.size, &memoryRegion);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to register mem region, size: " << mr.size << " addr:" << std::hex << mr.addr
                                                             << " service: " << rpcService_ << " ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    Service_MemoryRegionInfo memoryRegionInfo;
    ret = DlHcomApi::ServiceGetMemoryRegionInfo(memoryRegion, &memoryRegionInfo);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to get mem region info, size: " << mr.size << " service: " << rpcService_
                                                             << " ret: " << ret);
        DlHcomApi::ServiceDestroyMemoryRegion(rpcService_, memoryRegion);
        return BM_DL_FUNCTION_FAILED;
    }

    HcomMemoryRegion mrInfo{};
    mrInfo.addr = mr.addr;
    mrInfo.size = mr.size;
    mrInfo.mr = memoryRegion;
    std::copy_n(memoryRegionInfo.lKey.keys, sizeof(memoryRegionInfo.lKey.keys) / sizeof(memoryRegionInfo.lKey.keys[0]),
                mrInfo.lKey.keys);
    {
        std::unique_lock<std::mutex> lock(mrMutex_[rankId_]);
        mrs_[rankId_].push_back(mrInfo);
    }
    if ((mr.flags & REG_MR_FLAG_SELF) == 0) {
        auto type = (mr.flags & REG_MR_FLAG_DRAM) ? HYBM_MEM_TYPE_HOST : HYBM_MEM_TYPE_DEVICE;
        ret = HybmVaManager::GetInstance().AddVaInfoFromExternal({mrInfo.addr, mr.size, type, mr.addr}, rankId_);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Add va info failed, ret: " << ret << ", gva: " << memoryRegionInfo.lAddress);
            DlHcomApi::ServiceDestroyMemoryRegion(rpcService_, memoryRegion);
            return ret;
        }
    }
    BM_LOG_DEBUG("Success to register to mr info size: " << mrInfo.size << " lKey: " << mrInfo.lKey.keys[0]);
    return BM_OK;
}
#else
Result HcomTransportManager::RegisterMemoryRegion(const TransportMemoryRegion &mr)
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    BM_ASSERT_RETURN(mr.addr != 0 && mr.size != 0 && (mr.flags & transport::REG_MR_FLAG_DRAM), BM_INVALID_PARAM);

    HcomMemoryRegion info{};
    if (GetMemoryRegionByAddr(rankId_, mr.addr, info) == BM_OK) {
        BM_LOG_ERROR("Failed to register mem region, addr already registered");
        return BM_ERROR;
    }

    Service_MemoryRegion memoryRegion;
    int32_t ret = DlHcomApi::ServiceRegisterAssignMemoryRegion(rpcService_, mr.addr, mr.size, &memoryRegion);
    // 单rank不需要hcom,目的是在无网卡的情况下也可以测试
    if (ret != 0) {
        BM_LOG_WARN("Failed to register mem region, size: " << mr.size << " addr:" << std::hex << mr.addr
                                                            << " service: " << rpcService_ << " ret: " << ret);
    }
    Service_MemoryRegionInfo memoryRegionInfo;
    if (ret == 0) {
        ret = DlHcomApi::ServiceGetMemoryRegionInfo(memoryRegion, &memoryRegionInfo);
    }
    if (ret != 0) {
        BM_LOG_WARN("Failed to get mem region info, size: " << mr.size << " service: " << rpcService_
                                                            << " ret: " << ret);
    }

    HcomMemoryRegion mrInfo{};
    mrInfo.addr = mr.addr;
    mrInfo.size = mr.size;
    mrInfo.mr = memoryRegion;
    CopyHcomOneSideKey(memoryRegionInfo.lKey, mrInfo.lKey);
    {
        std::unique_lock<std::mutex> lock(mrMutex_[rankId_]);
        mrs_[rankId_].push_back(mrInfo);
    }
    BM_LOG_DEBUG("Success to register to mr info size: " << mrInfo.size << " lKey: " << mrInfo.lKey.keys[0]);
    return BM_OK;
}
#endif

Result HcomTransportManager::UnregisterMemoryRegion(uint64_t addr)
{
    BM_ASSERT_RETURN(addr != 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    HybmVaManager::GetInstance().RemoveOneVaInfo(addr);

    std::unique_lock<std::mutex> lock(mrMutex_[rankId_]);
    auto localMrs = mrs_[rankId_];
    for (uint32_t i = 0; i < localMrs.size(); ++i) {
        if (localMrs[i].addr == addr) {
            DlHcomApi::ServiceDestroyMemoryRegion(rpcService_, localMrs[i].mr);
            localMrs.erase(localMrs.begin() + i);
            return BM_OK;
        }
    }
    return BM_ERROR;
}

bool HcomTransportManager::QueryHasRegistered(uint64_t addr, uint64_t size)
{
    std::unique_lock<std::mutex> lock(mrMutex_[rankId_]);
    for (const auto &mrInfo : mrs_[rankId_]) {
        if (mrInfo.addr <= addr && mrInfo.addr + mrInfo.size >= addr + size) {
            return true;
        }
    }
    return false;
}

Result HcomTransportManager::QueryMemoryKey(uint64_t addr, TransportMemoryKey &key)
{
    HcomMemoryRegion mrInfo{};
    if (GetMemoryRegionByAddr(rankId_, addr, mrInfo) != BM_OK) {
        BM_LOG_ERROR("Failed to query memory region");
        return BM_ERROR;
    }
    RegMemoryKeyUnion hostKey{};
    hostKey.hostKey.type = TT_HCOM;
    hostKey.hostKey.hcomInfo.lAddress = mrInfo.addr;
    CopyHcomOneSideKey(mrInfo.lKey, hostKey.hostKey.hcomInfo.lKey);
    hostKey.hostKey.hcomInfo.size = mrInfo.size;
    key = hostKey.commonKey;
    return BM_OK;
}

Result HcomTransportManager::ParseMemoryKey(const TransportMemoryKey &key, uint64_t &addr, uint64_t &size)
{
    RegMemoryKeyUnion keyUnion{};
    keyUnion.commonKey = key;
    if (keyUnion.hostKey.type != TT_HCOM) {
        BM_LOG_ERROR("parse key type invalid: " << keyUnion.hostKey.type);
        return BM_ERROR;
    }

    addr = keyUnion.hostKey.hcomInfo.lAddress;
    size = keyUnion.hostKey.hcomInfo.size;
    return BM_OK;
}

Result HcomTransportManager::Prepare(const HybmTransPrepareOptions &param)
{
    auto options = param.options;
    for (const auto &item : options) {
        auto rankId = item.first;
        if (rankId >= rankCount_) {
            BM_LOG_ERROR("Failed to update rank info ranId: " << rankId << " not match rank count: " << rankCount_);
            return BM_INVALID_PARAM;
        }
    }

    for (const auto &item : options) {
        auto rankId = item.first;
        auto nic = item.second.nic;
        nics_[rankId] = nic;

        RegMemoryKeyUnion keyUnion{};
        keyUnion.commonKey = item.second.memKeys[0];
        HcomMemoryRegion mrInfo{};
        mrInfo.addr = keyUnion.hostKey.hcomInfo.lAddress;
        mrInfo.size = keyUnion.hostKey.hcomInfo.size;
        if (rankId != rankId_ && (bmOptype_ & HYBM_DOP_TYPE_HOST_URMA)) {
            auto ret =
                DlHcomApi::ImportUrmaSegFunc(rpcService_, mrInfo.addr, mrInfo.size, &keyUnion.hostKey.hcomInfo.lKey);
            BM_ASSERT_RETURN(ret == 0, ret);
            BM_LOG_DEBUG("hcom returned, tokens: " << keyUnion.hostKey.hcomInfo.lKey.tokens[0]);
        }
        CopyHcomOneSideKey(keyUnion.hostKey.hcomInfo.lKey, mrInfo.lKey);
        {
            std::unique_lock<std::mutex> lock(mrMutex_[rankId]);
            mrs_[rankId].push_back(mrInfo);
        }
        BM_LOG_DEBUG("Success to register to mr info size: " << mrInfo.size << " lKey: " << mrInfo.lKey.keys[0]
                                                             << ", rankId: " << rankId);
    }
    return BM_OK;
}

Result HcomTransportManager::RemoveRanks(const std::vector<uint32_t> &removedRanks)
{
    BM_LOG_WARN("HCOM transport manager remove ranks not implements!");
    return BM_OK;
}

Result HcomTransportManager::Connect()
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    for (uint32_t i = 0; i < rankCount_; ++i) {
        if (rankId_ == i || nics_[i].empty()) {
            continue;
        }
        const auto ret = ConnectHcomChannel(i, nics_[i]);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to connect remote service, rankId" << i << " nic: " << nics_[i] << " ret: " << ret);
            return ret;
        }
        BM_LOG_DEBUG("connect remote service, rankId" << i << " nic: " << nics_[i] << " ret: " << ret);
    }
    return BM_OK;
}

Result HcomTransportManager::AsyncConnect()
{
    return BM_OK;
}

Result HcomTransportManager::WaitForConnected(int64_t timeoutNs)
{
    return BM_OK;
}

Result HcomTransportManager::UpdateRankMrInfos(const std::unordered_map<uint32_t, TransportRankPrepareInfo> &opt)
{
    for (const auto &item : opt) {
        auto rankId = item.first;
        if (rankId == rankId_) {
            continue;
        }
        RegMemoryKeyUnion keyUnion{};
        keyUnion.commonKey = item.second.memKeys[0];
        HcomMemoryRegion mrInfo{};
        mrInfo.addr = keyUnion.hostKey.hcomInfo.lAddress;
        mrInfo.size = keyUnion.hostKey.hcomInfo.size;
        if (rankId != rankId_ && (bmOptype_ & HYBM_DOP_TYPE_HOST_URMA)) {
            auto ret =
                DlHcomApi::ImportUrmaSegFunc(rpcService_, mrInfo.addr, mrInfo.size, &keyUnion.hostKey.hcomInfo.lKey);
            BM_ASSERT_RETURN(ret == 0, ret);
            BM_LOG_DEBUG("hcom returned, tokens: " << keyUnion.hostKey.hcomInfo.lKey.tokens[0]);
        }
        CopyHcomOneSideKey(keyUnion.hostKey.hcomInfo.lKey, mrInfo.lKey);
        {
            std::unique_lock<std::mutex> lock(mrMutex_[rankId]);
            mrs_[rankId].clear();
            mrs_[rankId].push_back(mrInfo);
        }
        BM_LOG_DEBUG("Success to register to mr info rankId: " << rankId << " size: " << mrInfo.size
                                                               << " lKey: " << mrInfo.lKey.keys[0]);
    }
    return BM_OK;
}

Result HcomTransportManager::UpdateRankConnectInfos(const std::unordered_map<uint32_t, TransportRankPrepareInfo> &opt)
{
    std::vector<uint32_t> removeRankList;
    std::vector<uint32_t> addRankList;
    for (uint32_t i = 0; i < rankCount_; ++i) {
        if (i == rankId_) {
            continue;
        }
        auto it = opt.find(i);
        if (channels_[i] == 0 && it != opt.end()) {
            nics_[i] = it->second.nic;
            const auto ret = ConnectHcomChannel(i, nics_[i]);
            if (ret != BM_OK) {
                BM_LOG_ERROR("Failed to connect remote service, rankId" << i << " nic: " << nics_[i]
                                                                        << " ret: " << ret);
                return ret;
            }
        }
    }
    return BM_OK;
}

Result HcomTransportManager::UpdateRankOptions(const HybmTransPrepareOptions &param)
{
    auto options = param.options;
    for (const auto &item : options) {
        auto rankId = item.first;
        if (rankId >= rankCount_) {
            BM_LOG_ERROR("Failed to update rank info ranId: " << rankId << " not match rank count: " << rankCount_);
            return BM_INVALID_PARAM;
        }
    }
    auto ret = UpdateRankMrInfos(param.options);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to update rank mr info ret: " << ret);
        return ret;
    }
    ret = UpdateRankConnectInfos(param.options);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to update rank connect info ret: " << ret);
        return ret;
    }
    return BM_OK;
}

const std::string &HcomTransportManager::GetNic() const
{
    return localNic_;
}

Result HcomTransportManager::InnerReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    BM_ASSERT_RETURN(rankId < rankCount_, BM_INVALID_PARAM);
    Hcom_Channel channel = channels_[rankId];
    if (channel == 0) {
        BM_LOG_ERROR("Failed to write remote, rankId: " << rankId << " is not connect");
        return BM_ERROR;
    }
    Channel_OneSideRequest req;
    req.rAddress = (void *)rAddr;
    req.lAddress = (void *)lAddr;
    req.size = (uint32_t)size;

    HcomMemoryRegion mr{};
    auto ret = GetMemoryRegionByAddr(rankId_, lAddr, mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to find lKey, lAddr: is not register");
        return BM_ERROR;
    }
    std::copy_n(mr.lKey.keys, std::size(req.lKey.keys), req.lKey.keys);
    ret = GetMemoryRegionByAddr(rankId, rAddr, mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to find rKey, rankId: " << rankId << " is not set");
        return BM_ERROR;
    }
    CopyHcomOneSideKey(mr.lKey, req.rKey);
    BM_LOG_DEBUG("Try to read remote rankId: " << rankId << " channel: " << (void *)channel
                                               << " lKey:" << req.lKey.keys[0] << " rKey: " << req.rKey.keys[0]
                                               << " size: " << size);
    return DlHcomApi::ChannelGet(channel, req, nullptr);
}

Result HcomTransportManager::InnerWriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    BM_ASSERT_RETURN(rankId < rankCount_, BM_INVALID_PARAM);
    Hcom_Channel channel = channels_[rankId];
    if (channel == 0) {
        BM_LOG_ERROR("Failed to write remote, rankId: " << rankId << " is not connect");
        return BM_ERROR;
    }
    Channel_OneSideRequest req;
    req.rAddress = (void *)rAddr;
    req.lAddress = (void *)lAddr;
    req.size = (uint32_t)size;

    HcomMemoryRegion mr{};
    auto ret = GetMemoryRegionByAddr(rankId_, lAddr, mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to find lKey, lAddr is not register");
        return BM_ERROR;
    }
    std::copy_n(mr.lKey.keys, sizeof(req.lKey.keys) / sizeof(req.lKey.keys[0]), req.lKey.keys);
    ret = GetMemoryRegionByAddr(rankId, rAddr, mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to find rKey, rankId: " << rankId << " is not set");
        return BM_ERROR;
    }
    CopyHcomOneSideKey(mr.lKey, req.rKey);
    BM_LOG_DEBUG("Try to write remote rankId: " << rankId << " channel: " << (void *)channel
                                                << " lKey:" << req.lKey.keys[0] << " rKey: " << req.rKey.keys[0]
                                                << " size: " << size);
    return DlHcomApi::ChannelPut(channel, req, nullptr);
}

int HcomTransportManager::PrepareThreadLocalStream()
{
    lock_.LockRead();
    if (stream_ != nullptr) {
        lock_.UnLock();
        return BM_OK;
    }
    lock_.UnLock();
    WriteGuard lockGuard(lock_);
    stream_ = std::make_shared<HostHcomCounterStream>(0);
    return BM_OK;
}

Result HcomTransportManager::ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    BM_ASSERT_RETURN(rankId < rankCount_, BM_INVALID_PARAM);
    Hcom_Channel channel = channels_[rankId];
    if (channel == 0) {
        BM_LOG_ERROR("Failed to write remote, rankId: " << rankId << " is not connect");
        return BM_ERROR;
    }
    Channel_OneSideRequest req;
    req.rAddress = reinterpret_cast<void *>(rAddr);
    req.lAddress = reinterpret_cast<void *>(lAddr);
    req.size = static_cast<uint32_t>(size);
    HcomMemoryRegion mr{};
    auto ret = GetMemoryRegionByAddr(rankId_, lAddr, mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to find lKey, rankId: " << rankId << ", size: " << req.size << ", rAddr: " << rAddr);
        return BM_ERROR;
    }
    CopyHcomOneSideKey(mr.lKey, req.lKey);
    mr.lKey = {};
    ret = GetMemoryRegionByAddr(rankId, rAddr, mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to find rKey, rankId: " << rankId << ", size: " << req.size << ", rAddr: " << rAddr);
        return BM_ERROR;
    }
    CopyHcomOneSideKey(mr.lKey, req.rKey);
    BM_LOG_DEBUG("Try to read remote rankId: " << rankId << " channel: " << (void *)channel
                                               << " lKey:" << req.lKey.keys[0] << " rKey: " << req.rKey.keys[0]
                                               << " size: " << size << " tokens: " << req.rKey.tokens[0]);
    ret = PrepareThreadLocalStream();
    if (ret != BM_OK) {
        BM_LOG_ERROR("prepare stream error rankId: " << rankId);
        return ret;
    }
    BM_ASSERT_RETURN(stream_.get() != nullptr, BM_ERROR);
    Channel_Callback channelCallback;
    channelCallback.arg = stream_.get();
    channelCallback.cb = [](void *arg, Service_Context context) -> void {
        (void)context;
        static_cast<HostHcomCounterStream *>(arg)->FinishOne();
    };
    stream_->SubmitTasks();
    TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_HCOM_CH_GET);
    ret = DlHcomApi::ChannelGet(channel, req, &channelCallback);
    if (ret != 0) {
        stream_->FinishOne(false);
    }
    TP_TRACE_END(TP_HYBM_HOST_RDMA_HCOM_CH_GET, ret);
    return ret;
}

Result HcomTransportManager::WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    BM_ASSERT_RETURN(rankId < rankCount_, BM_INVALID_PARAM);
    Hcom_Channel channel = channels_[rankId];
    if (channel == 0) {
        BM_LOG_ERROR("Failed to write remote, rankId: " << rankId << " is not connect");
        return BM_ERROR;
    }
    Channel_OneSideRequest req;
    req.rAddress = reinterpret_cast<void *>(rAddr);
    req.lAddress = reinterpret_cast<void *>(lAddr);
    req.size = static_cast<uint32_t>(size);
    HcomMemoryRegion mr{};
    auto ret = GetMemoryRegionByAddr(rankId_, lAddr, mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to find lKey, lAddr is not register");
        return BM_ERROR;
    }
    std::copy_n(mr.lKey.keys, std::size(req.lKey.keys), req.lKey.keys);
    mr.lKey = {};
    ret = GetMemoryRegionByAddr(rankId, rAddr, mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to find rKey, rankId: " << rankId << " is not set");
        return BM_ERROR;
    }
    CopyHcomOneSideKey(mr.lKey, req.rKey);
    BM_LOG_DEBUG("Try to write remote rankId: " << rankId << " channel: " << (void *)channel
                                                << " lKey:" << req.lKey.keys[0] << " rKey: " << req.rKey.keys[0]
                                                << " size: " << size << " tokens: " << req.rKey.tokens[0]);
    ret = PrepareThreadLocalStream();
    if (ret != BM_OK) {
        BM_LOG_ERROR("prepare stream error rankId: " << rankId);
        return ret;
    }
    BM_ASSERT_RETURN(stream_.get() != nullptr, BM_ERROR);
    Channel_Callback channelCallback;
    channelCallback.arg = stream_.get();
    channelCallback.cb = [](void *arg, Service_Context context) -> void {
        (void)context;
        static_cast<HostHcomCounterStream *>(arg)->FinishOne();
    };
    stream_->SubmitTasks();
    ret = DlHcomApi::ChannelPut(channel, req, &channelCallback);
    if (ret != BM_OK) {
        stream_->FinishOne(false);
    }
    return ret;
}

Result HcomTransportManager::Synchronize(const uint32_t rankId)
{
    stream_->Synchronize(static_cast<int32_t>(rankId));
    return BM_OK;
}

Result HcomTransportManager::CheckTransportOptions(const TransportOptions &options)
{
    std::string protocol;
    uint32_t basePort;
    auto ret = HostHcomHelper::AnalysisNic(options.nic, protocol, localIp_, basePort);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to check nic, nic: " << options.nic << " ret: " << ret);
        return ret;
    }
    const auto hcomAutoPort = basePort + options.rankId;
    BM_LOG_INFO("hcom base port: " << basePort << ", hcom auto port with rank: " << hcomAutoPort);
    localNic_ = protocol + localIp_ + ":" + std::to_string(hcomAutoPort);
    return BM_OK;
}

Result HcomTransportManager::TransportRpcHcomNewEndPoint(Hcom_Channel newCh, uint64_t usrCtx, const char *payLoad)
{
    BM_LOG_DEBUG("New hcom ch, ch: " << newCh << " usrCtx: " << usrCtx << " payLoad: " << payLoad);
    return BM_OK;
}

Result HcomTransportManager::TransportRpcHcomEndPointBroken(Hcom_Channel ch, uint64_t usrCtx, const char *payLoad)
{
    BM_LOG_DEBUG("Broken on hcom ch, ch: " << ch << " usrCtx: " << usrCtx << " payLoad: " << payLoad);
    uint32_t rankId = UINT32_MAX;
    if (!StrUtil::String2Uint<uint32_t>(payLoad, rankId)) {
        BM_LOG_ERROR("Failed to get rankId payLoad: " << payLoad);
        return BM_ERROR;
    }
    GetInstance()->DisConnectHcomChannel(rankId, ch);
    return BM_OK;
}

Result HcomTransportManager::TransportRpcHcomRequestReceived(Service_Context ctx, uint64_t usrCtx)
{
    BM_LOG_DEBUG("Receive hcom req, ctx: " << ctx << " usrCtx: " << usrCtx);
    return BM_OK;
}

Result HcomTransportManager::TransportRpcHcomRequestPosted(Service_Context ctx, uint64_t usrCtx)
{
    BM_LOG_DEBUG("Post hcom req, ctx: " << ctx << " usrCtx: " << usrCtx);
    return BM_OK;
}

Result HcomTransportManager::TransportRpcHcomOneSideDone(Service_Context ctx, uint64_t usrCtx)
{
    BM_LOG_DEBUG("Done hcom one side, ctx: " << ctx << " usrCtx: " << usrCtx);
    return BM_OK;
}

Result HcomTransportManager::ConnectHcomChannel(uint32_t rankId, const std::string &url)
{
    std::unique_lock<std::mutex> lock(channelMutex_[rankId]);
    if (channels_[rankId] != 0) {
        BM_LOG_WARN("Stop connect to hcom service rankId: " << rankId << " url: " << url << " is connected");
        return BM_OK;
    }
    Hcom_Channel channel;
    Service_ConnectOptions options;
    options.clientGroupId = 0;
    options.serverGroupId = 0;
    if (StrUtil::StartWith(url, UBC_PROTOCOL_PREFIX)) {
        options.linkCount = 1UL;
    } else {
        options.linkCount = HCOM_TRANS_EP_SIZE;
    }
    auto rankIdStr = std::to_string(rankId);
    std::copy_n(rankIdStr.c_str(), rankIdStr.size() + 1, options.payLoad);
    do {
        auto ret = DlHcomApi::ServiceConnect(rpcService_, url.c_str(), &channel, options);
        if (ret != 0) {
            BM_LOG_ERROR("Failed to connect remote service, rankId" << rankId << " url: " << url << " ret: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }
    } while (0);
    channels_[rankId] = channel;
    BM_LOG_DEBUG("Success to connect to hcom service rankId: " << rankId << " url: " << url
                                                               << " channel: " << (void *)channel);
    return BM_OK;
}

void HcomTransportManager::DisConnectHcomChannel(uint32_t rankId, Hcom_Channel ch)
{
    if (channels_.empty()) {
        return;
    }
    if (rankId >= rankCount_ || ch == 0) {
        BM_LOG_ERROR_LIMIT("Failed to remove channel invalid rankId" << rankId << " ch: " << ch);
        return;
    }
    std::unique_lock<std::mutex> lock(channelMutex_[rankId]);
    if (GetInstance()->rpcService_ != 0) {
        DlHcomApi::ServiceDisConnect(GetInstance()->rpcService_, ch);
    }
    if (channels_[rankId] == ch) {
        channels_[rankId] = 0;
    }
}

void HcomTransportManager::ForceReConnectHcomChannel(uint32_t rankId)
{
    if (rankId >= rankCount_) {
        BM_LOG_ERROR("Failed to remove channel invalid rankId" << rankId);
        return;
    }
    std::unique_lock<std::mutex> lock(channelMutex_[rankId]);
    channels_[rankId] = 0;
    lock.unlock();
    auto ret = ConnectHcomChannel(rankId, nics_[rankId]);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to connect channel ret: " << rankId);
    }
}

Result HcomTransportManager::ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    constexpr uint32_t kMaxRetries = 3u;
    for (uint32_t attempt = 0; attempt < kMaxRetries; ++attempt) {
        Result ret = InnerReadRemote(rankId, lAddr, rAddr, size);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Failed to ReadRemote, ret: " << ret << ", attempt: " << attempt << ", rank: " << rankId);
        if (ret > 0 || channels_[rankId] == 0) {
            ForceReConnectHcomChannel(rankId);
        }
        // 退避延迟：第 0 次不等，第 1 次等 1s，第 2 次等 2s（避免忙等）
        if (attempt < kMaxRetries - 1) {
            std::this_thread::sleep_for(std::chrono::seconds(attempt + 1));
        }
    }
    return BM_ERROR;
}

Result HcomTransportManager::WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    constexpr uint32_t kMaxRetries = 3u;
    for (uint32_t attempt = 0; attempt < kMaxRetries; ++attempt) {
        Result ret = InnerWriteRemote(rankId, lAddr, rAddr, size);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Failed to WriteRemote, ret: " << ret << ", attempt: " << attempt << ", rank: " << rankId);
        if (ret > 0 || channels_[rankId] == 0) {
            ForceReConnectHcomChannel(rankId);
        }
        // 退避延迟：第 0 次不等，第 1 次等 1s，第 2 次等 2s（避免忙等）
        if (attempt < kMaxRetries - 1) {
            std::this_thread::sleep_for(std::chrono::seconds(attempt + 1));
        }
    }
    return BM_ERROR;
}

Result HcomTransportManager::GetMemoryRegionByAddr(const uint32_t &rankId, const uint64_t &addr, HcomMemoryRegion &mr)
{
    std::unique_lock<std::mutex> lock(mrMutex_[rankId]);
    for (const auto &mrInfo : mrs_[rankId]) {
        if (mrInfo.addr <= addr && mrInfo.addr + mrInfo.size > addr) {
            mr = mrInfo;
            return BM_OK;
        }
    }
    return BM_ERROR;
}

int HcomTransportManager::GetCACallBack(const char *name, char **caPath, char **crlPath,
                                        Hcom_PeerCertVerifyType *verifyType, Hcom_TlsCertVerify *verify)
{
    if (caPath == nullptr || crlPath == nullptr || verifyType == nullptr || verify == nullptr) {
        BM_LOG_ERROR("Invalid input");
        return 1;
    }
    *caPath = tlsConfig_.caPath;
    *crlPath = tlsConfig_.crlPath;
    *verifyType = C_VERIFY_BY_DEFAULT;
    *verify = CertVerifyCallBack;
    return 0;
}

int HcomTransportManager::GetCertCallBack(const char *name, char **certPath)
{
    if (certPath == nullptr) {
        BM_LOG_ERROR("certPath is nullptr");
        return 1;
    }
    *certPath = tlsConfig_.certPath;
    return 0;
}

int HcomTransportManager::GetPrivateKeyCallBack(const char *name, char **priKeyPath, char **keyPass,
                                                Hcom_TlsKeyPassErase *erase)
{
    std::lock_guard<std::mutex> lock(keyPassMutex);

    if (priKeyPath == nullptr || keyPass == nullptr || erase == nullptr) {
        BM_LOG_ERROR("Invalid input");
        return 1;
    }

    *priKeyPath = tlsConfig_.keyPath;

    std::ifstream fileStream;
    fileStream.open(tlsConfig_.keyPassPath);
    if (!fileStream.is_open()) {
        BM_LOG_ERROR("Failed to open keyPassFile");
        return 1;
    }
    char encryptedKeyPass[KEYPASS_MAX_LEN] = {};
    fileStream.getline(encryptedKeyPass, KEYPASS_MAX_LEN);
    fileStream.close();
    DecryptFunc func;
    if (std::string(tlsConfig_.decrypterLibPath).empty()) {
        BM_LOG_WARN("No decrypter provided, using default decrypter handler");
        func = static_cast<DecryptFunc>(MfTlsUtil::DefaultDecrypter);
    } else {
        func = MfTlsUtil::LoadDecryptFunction(tlsConfig_.decrypterLibPath);
        if (func == nullptr) {
            BM_LOG_ERROR("failed to load customized decrypt function");
            return 1;
        }
    }
    if (func(encryptedKeyPass, KEYPASS_MAX_LEN, keyPass_, KEYPASS_MAX_LEN) != 0) {
        BM_LOG_ERROR("failed to decrypt key pass");
        return 1;
    }

    *keyPass = keyPass_;
    *erase = KeyPassEraseCallBack;

    return 0;
}

int HcomTransportManager::CertVerifyCallBack(void *x509, const char *crlPath)
{
    return 0;
}

void HcomTransportManager::KeyPassEraseCallBack(char *keyPass, int len)
{
    for (int i = 0; i < len; i++) {
        keyPass[i] = 0;
    }
}