/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "host_hcom_transport_manager.h"
#include <iostream>
#include <string>
#include <regex>
#include <sstream>
#include <arpa/inet.h>
#include "dl_hcom_api.h"
#include "host_hcom_common.h"
#include "host_hcom_helper.h"

using namespace ock::mf;
using namespace ock::mf::transport;
using namespace ock::mf::transport::host;

namespace {
constexpr uint64_t HCOM_RECV_DATA_SIZE = 8192UL;
constexpr uint64_t HCOM_SEND_QUEUE_SIZE = 512UL;
constexpr uint64_t HCOM_RECV_QUEUE_SIZE = 512UL;
constexpr uint64_t HCOM_QUEUE_PRE_POST_SIZE = 256UL;
constexpr uint8_t HCOM_TRANS_EP_SIZE = 16;
const char *HCOM_RPC_SERVICE_NAME = "hybm_hcom_service";
}

Result HcomTransportManager::OpenDevice(const TransportOptions &options)
{
    BM_ASSERT_RETURN(rpcService_ == 0, BM_OK);
    BM_ASSERT_RETURN(CheckTransportOptions(options) == BM_OK, BM_INVALID_PARAM);

    Service_Options opt{};
    opt.workerGroupMode = C_SERVICE_BUSY_POLLING;
    opt.maxSendRecvDataSize = HCOM_RECV_DATA_SIZE;
    Service_Type enumProtocolType = HostHcomHelper::HybmDopTransHcomProtocol(options.protocol);
    int ret = DlHcomApi::ServiceCreate(enumProtocolType, HCOM_RPC_SERVICE_NAME, opt, &rpcService_);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to create hcom service, nic: " << options.nic << " type: " << enumProtocolType
                                                            << " ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    DlHcomApi::ServiceSetSendQueueSize(rpcService_, HCOM_SEND_QUEUE_SIZE);
    DlHcomApi::ServiceSetRecvQueueSize(rpcService_, HCOM_RECV_QUEUE_SIZE);
    DlHcomApi::ServiceSetQueuePrePostSize(rpcService_, HCOM_QUEUE_PRE_POST_SIZE);
    DlHcomApi::ServiceRegisterChannelBrokerHandler(rpcService_, TransportRpcHcomEndPointBroken, C_CHANNEL_RECONNECT, 1);
    DlHcomApi::ServiceRegisterHandler(rpcService_, C_SERVICE_REQUEST_RECEIVED, TransportRpcHcomRequestReceived, 1);
    DlHcomApi::ServiceRegisterHandler(rpcService_, C_SERVICE_REQUEST_POSTED, TransportRpcHcomRequestPosted, 1);
    DlHcomApi::ServiceRegisterHandler(rpcService_, C_SERVICE_READWRITE_DONE, TransportRpcHcomOneSideDone, 1);

    std::string ipMask = localIp_ + "/32";
    DlHcomApi::ServiceSetDeviceIpMask(rpcService_, ipMask.c_str());
    DlHcomApi::ServiceBind(rpcService_, localNic_.c_str(), TransportRpcHcomNewEndPoint);
    ret = DlHcomApi::ServiceStart(rpcService_);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to start hcom service, nic: " << localNic_ << " type: " << enumProtocolType
                                                           << " ret: " << ret);
        DlHcomApi::ServiceDestroy(rpcService_, HCOM_RPC_SERVICE_NAME);
        rpcService_ = 0;
        return BM_DL_FUNCTION_FAILED;
    }

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
    BM_ASSERT_RETURN(rpcService_ != 0, BM_OK);
    for (uint32_t i = 0; i < rankCount_; ++i) {
        DisConnectHcomChannel(i, channels_[i]);
    }
    DlHcomApi::ServiceDestroy(rpcService_, HCOM_RPC_SERVICE_NAME);
    rpcService_ = 0;
    localNic_ = "";
    protocol = "";
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

Result HcomTransportManager::RegisterMemoryRegion(const TransportMemoryRegion &mr)
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    BM_ASSERT_RETURN(mr.addr != 0 && mr.size != 0, BM_INVALID_PARAM);

    HcomMemoryRegion info{};
    if (GetMemoryRegionByAddr(rankId_, mr.addr, info) == BM_OK) {
        BM_LOG_ERROR("Failed to register mem region, addr already registered");
        return BM_ERROR;
    }

    Service_MemoryRegion memoryRegion;
    int32_t ret = DlHcomApi::ServiceRegisterAssignMemoryRegion(rpcService_, mr.addr, mr.size, &memoryRegion);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to register mem region, size: " << mr.size << " service: " << rpcService_
            << " ret: " << ret);
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
    BM_LOG_DEBUG("Success to register to mr info addr");
    return BM_OK;
}

Result HcomTransportManager::UnregisterMemoryRegion(uint64_t addr)
{
    BM_ASSERT_RETURN(addr != 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);

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
    std::copy_n(mrInfo.lKey.keys, sizeof(hostKey.hostKey.hcomInfo.lKey.keys) /
                sizeof(hostKey.hostKey.hcomInfo.lKey.keys[0]), hostKey.hostKey.hcomInfo.lKey.keys);
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
    for (const auto &item: options) {
        auto rankId = item.first;
        if (rankId >= rankCount_) {
            BM_LOG_ERROR("Failed to update rank info ranId: " << rankId << " not match rank count: " << rankCount_);
            return BM_INVALID_PARAM;
        }
    }

    for (const auto &item: options) {
        auto rankId = item.first;
        auto nic = item.second.nic;
        nics_[rankId] = nic;

        RegMemoryKeyUnion keyUnion{};
        keyUnion.commonKey = item.second.memKeys[0];
        HcomMemoryRegion mrInfo{};
        mrInfo.addr = keyUnion.hostKey.hcomInfo.lAddress;
        mrInfo.size = keyUnion.hostKey.hcomInfo.size;
        std::copy_n(keyUnion.hostKey.hcomInfo.lKey.keys,
                    sizeof(keyUnion.hostKey.hcomInfo.lKey.keys) / sizeof(mrInfo.lKey.keys[0]), mrInfo.lKey.keys);
        {
            std::unique_lock<std::mutex> lock(mrMutex_[rankId]);
            mrs_[rankId].push_back(mrInfo);
        }
        BM_LOG_DEBUG("Success to register to mr info addr");
    }
    return BM_OK;
}

Result HcomTransportManager::Connect()
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    for (uint32_t i = 0; i < rankCount_; ++i) {
        if (rankId_ == i || nics_[i].empty()) {
            continue;
        }
        auto ret = ConnectHcomChannel(i, nics_[i]);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to connect remote service, rankId" << i << " nic: " << nics_[i] << " ret: " << ret);
            continue;
        }
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
    for (const auto &item: opt) {
        auto rankId = item.first;
        if (rankId == rankId_) {
            continue;
        }
        RegMemoryKeyUnion keyUnion{};
        keyUnion.commonKey = item.second.memKeys[0];
        HcomMemoryRegion mrInfo{};
        mrInfo.addr = keyUnion.hostKey.hcomInfo.lAddress;
        mrInfo.size = keyUnion.hostKey.hcomInfo.size;
        std::copy_n(keyUnion.hostKey.hcomInfo.lKey.keys,
                    sizeof(keyUnion.hostKey.hcomInfo.lKey.keys) / sizeof(mrInfo.lKey.keys[0]), mrInfo.lKey.keys);
        {
            std::unique_lock<std::mutex> lock(mrMutex_[rankId]);
            mrs_[rankId].clear();
            mrs_[rankId].push_back(mrInfo);
        }
        BM_LOG_DEBUG("Success to register to mr info rankId: " << rankId);
    }
    return BM_OK;
}

Result HcomTransportManager::UpdateRankConnectInfos(const std::unordered_map<uint32_t, TransportRankPrepareInfo> &opt)
{
    for (uint32_t i = 0; i < rankCount_; ++i) {
        if (i == rankId_) {
            continue;
        }
        auto it = opt.find(i);
        if (channels_[i] == 0 && it != opt.end()) {
            nics_[i] = it->second.nic;
            auto ret = ConnectHcomChannel(i, nics_[i]);
            if (ret != BM_OK) {
                BM_LOG_ERROR("Failed to connect remote service, rankId" << i << " nic: " << nics_[i]
                                                                        << " ret: " << ret);
                continue;
            }
        }
    }
    return BM_OK;
}

Result HcomTransportManager::UpdateRankOptions(const HybmTransPrepareOptions &param)
{
    auto options = param.options;
    for (const auto &item: options) {
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

Result HcomTransportManager::ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    BM_ASSERT_RETURN(rankId < rankCount_, BM_INVALID_PARAM);
    Hcom_Channel channel = channels_[rankId];
    if (channel == 0) {
        BM_LOG_ERROR("Failed to write remote, rankId: " << rankId << " is not connect");
        return BM_ERROR;
    }
    Channel_OneSideRequest req;
    req.rAddress = (void *) rAddr;
    req.lAddress = (void *) lAddr;
    req.size = (uint32_t) size;

    HcomMemoryRegion mr{};
    auto ret = GetMemoryRegionByAddr(rankId_, lAddr, mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to find lKey, lAddr is not register");
        return BM_ERROR;
    }
    std::copy_n(mr.lKey.keys, sizeof(req.lKey.keys) / sizeof(req.lKey.keys[0]), req.lKey.keys);
    ret = GetMemoryRegionByAddr(rankId, rAddr, mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to find rKey, rankId: " << rankId << " rAddr is not set");
        return BM_ERROR;
    }
    std::copy_n(mr.lKey.keys, sizeof(req.rKey.keys) / sizeof(req.rKey.keys[0]), req.rKey.keys);
    BM_LOG_DEBUG("Try to read remote rankId: " << rankId << " size: " << size);
    return DlHcomApi::ChannelGet(channel, req, nullptr);
}

Result HcomTransportManager::WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    BM_ASSERT_RETURN(rankId < rankCount_, BM_INVALID_PARAM);
    Hcom_Channel channel = channels_[rankId];
    if (channel == 0) {
        BM_LOG_ERROR("Failed to write remote, rankId: " << rankId << " is not connect");
        return BM_ERROR;
    }
    Channel_OneSideRequest req;
    req.rAddress = (void *) rAddr;
    req.lAddress = (void *) lAddr;
    req.size = (uint32_t) size;

    HcomMemoryRegion mr{};
    auto ret = GetMemoryRegionByAddr(rankId_, lAddr, mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to find lKey, lAddr is not register");
        return BM_ERROR;
    }
    std::copy_n(mr.lKey.keys, sizeof(req.lKey.keys) / sizeof(req.lKey.keys[0]), req.lKey.keys);
    ret = GetMemoryRegionByAddr(rankId, rAddr, mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to find rKey, rankId: " << rankId << " rAddr is not set");
        return BM_ERROR;
    }
    std::copy_n(mr.lKey.keys, sizeof(req.rKey.keys) / sizeof(req.rKey.keys[0]), req.rKey.keys);
    BM_LOG_DEBUG("Try to write remote rankId: " << rankId << " size: " << size);
    return DlHcomApi::ChannelPut(channel, req, nullptr);
}

Result HcomTransportManager::CheckTransportOptions(const TransportOptions &options)
{
    auto ret = HostHcomHelper::AnalysisNic(options.nic, protocol, localIp_, localPort_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to check nic, nic: " << options.nic << " ret: " << ret);
        return ret;
    }
    localNic_ = protocol + localIp_ + ":" + std::to_string(localPort_);
    return BM_OK;
}

Result HcomTransportManager::TransportRpcHcomNewEndPoint(Hcom_Channel newCh, uint64_t usrCtx, const char *payLoad)
{
    BM_LOG_DEBUG("New hcom ch, ch: " << newCh << " usrCtx: " << usrCtx);
    return BM_OK;
}

Result HcomTransportManager::TransportRpcHcomEndPointBroken(Hcom_Channel ch, uint64_t usrCtx, const char *payLoad)
{
    BM_LOG_DEBUG("Broken on hcom ch, ch: " << ch << " usrCtx: " << usrCtx);
    uint32_t rankId = UINT32_MAX;
    try {
        rankId = static_cast<uint32_t>(std::stoul(payLoad));
    } catch (...) {
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
    options.linkCount = HCOM_TRANS_EP_SIZE;
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
                                                               << " channel: " << (void *) channel);
    return BM_OK;
}

void HcomTransportManager::DisConnectHcomChannel(uint32_t rankId, Hcom_Channel ch)
{
    if (rankId >= rankCount_ || ch == 0) {
        BM_LOG_WARN("Failed to remove channel invalid rankId" << rankId << " ch: " << ch);
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

Result HcomTransportManager::GetMemoryRegionByAddr(const uint32_t &rankId, const uint64_t &addr, HcomMemoryRegion &mr)
{
    std::unique_lock<std::mutex> lock(mrMutex_[rankId]);
    for (const auto &mrInfo: mrs_[rankId]) {
        if (mrInfo.addr <= addr && mrInfo.addr + mrInfo.size > addr) {
            mr = mrInfo;
            return BM_OK;
        }
    }
    return BM_ERROR;
}