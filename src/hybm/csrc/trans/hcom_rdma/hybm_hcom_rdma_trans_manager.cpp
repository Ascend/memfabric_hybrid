/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_hcom_rdma_trans_manager.h"
#include <iostream>
#include <string>
#include <regex>
#include <sstream>
#include <arpa/inet.h>
#include "dl_hcom_api.h"

using namespace ock::mf;

namespace {
constexpr uint64_t HCOM_RECV_DATA_SIZE = 8192UL;
constexpr uint64_t HCOM_SEND_QUEUE_SIZE = 512UL;
constexpr uint64_t HCOM_RECV_QUEUE_SIZE = 512UL;
constexpr uint64_t HCOM_QUEUE_PRE_POST_SIZE = 256UL;
constexpr uint8_t HCOM_TRANS_EP_SIZE = 16;
const char *HCOM_RPC_SERVICE_NAME = "hybm_hcom_service";
}

Result HcomRdmaTransManager::OpenDevice(HybmTransOptions &options)
{
    BM_ASSERT_RETURN(rpcService_ == 0, BM_OK);
    BM_ASSERT_RETURN(CheckHybmTransOptions(options), BM_INVALID_PARAM);

    Service_Options opt {};
    opt.workerGroupMode = C_SERVICE_BUSY_POLLING;
    opt.maxSendRecvDataSize = HCOM_RECV_DATA_SIZE;
    Service_Type enumProtocolType = C_SERVICE_RDMA;
    int ret = DlHcomApi::ServiceCreate(enumProtocolType, HCOM_RPC_SERVICE_NAME, opt, &rpcService_);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to create hcom service, nic: " << options.nic << " type: " << enumProtocolType
                     << " ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    DlHcomApi::ServiceSetSendQueueSize(rpcService_, HCOM_SEND_QUEUE_SIZE);
    DlHcomApi::ServiceSetRecvQueueSize(rpcService_, HCOM_RECV_QUEUE_SIZE);
    DlHcomApi::ServiceSetQueuePrePostSize(rpcService_, HCOM_QUEUE_PRE_POST_SIZE);
    DlHcomApi::ServiceRegisterChannelBrokerHandler(rpcService_, TransRpcHcomEndPointBroken, C_CHANNEL_RECONNECT, 1);
    DlHcomApi::ServiceRegisterHandler(rpcService_, C_SERVICE_REQUEST_RECEIVED, TransRpcHcomRequestReceived, 1);
    DlHcomApi::ServiceRegisterHandler(rpcService_, C_SERVICE_REQUEST_POSTED, TransRpcHcomRequestPosted, 1);
    DlHcomApi::ServiceRegisterHandler(rpcService_, C_SERVICE_READWRITE_DONE, TransRpcHcomOneSideDone, 1);

    DlHcomApi::ServiceBind(rpcService_, options.nic.c_str(), TransRpcHcomNewEndPoint);
    ret = DlHcomApi::ServiceStart(rpcService_);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to start hcom service, nic: " << options.nic << " type: " << enumProtocolType
                     << " ret: " << ret);
        DlHcomApi::ServiceDestroy(rpcService_, HCOM_RPC_SERVICE_NAME);
        rpcService_ = 0;
        return BM_DL_FUNCTION_FAILED;
    }

    localNic_ = options.nic;
    return BM_OK;
}

Result HcomRdmaTransManager::CloseDevice()
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_OK);
    DlHcomApi::ServiceDestroy(rpcService_, HCOM_RPC_SERVICE_NAME);
    rpcService_ = 0;
    localNic_ = "";
    return BM_OK;
}

Result HcomRdmaTransManager::RegisterMemoryRegion(const HybmTransMemReg &input, HybmTransKey &key)
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);
    BM_ASSERT_RETURN(input.addr != 0 && input.size != 0, BM_INVALID_PARAM);

    auto it = mrInfoMap_.find((uint64_t)input.addr);
    if (it != mrInfoMap_.end()) {
        BM_LOG_ERROR("Failed to register mem region, addr: " << input.addr << " already registered");
        return BM_ERROR;
    }

    Service_MemoryRegion mr;
    int32_t ret = DlHcomApi::ServiceRegisterAssignMemoryRegion(rpcService_, (uintptr_t) input.addr, input.size, &mr);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to register mem region, addr: " << input.addr << " size: " << input.size
                     << " service: " << rpcService_ << " ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    Service_MemoryRegionInfo mrInfo;
    ret = DlHcomApi::ServiceGetMemoryRegionInfo(mr, &mrInfo);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to get mem region info, addr: " << input.addr << " size: " << input.size
                     << " service: " << rpcService_ << " ret: " << ret);
        DlHcomApi::ServiceDestroyMemoryRegion(rpcService_, mr);
        return BM_DL_FUNCTION_FAILED;
    }

    HcomMrInfo info{};
    info.memAddr = input.addr;
    info.size = input.size;
    info.lAddress = mrInfo.lAddress;
    info.lKey = mrInfo.lKey;
    info.mr = mr;

    mrInfoMap_[info.memAddr] = info;
    return BM_OK;
}

Result HcomRdmaTransManager::UnregisterMemoryRegion(const void *addr)
{
    BM_ASSERT_RETURN(addr != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);

    auto it = mrInfoMap_.find((uint64_t) addr);
    if (it != mrInfoMap_.end()) {
        BM_LOG_WARN("Failed to find mem region, addr: " << addr);
        return BM_OK;
    }

    auto mrInfo = it->second;
    DlHcomApi::ServiceDestroyMemoryRegion(rpcService_, mrInfo.mr);

    mrInfoMap_.erase(it);
    return BM_OK;
}

Result HcomRdmaTransManager::Prepare(const HybmTransPrepareOptions &options)
{
    return BM_OK;
}

Result HcomRdmaTransManager::Connect(const std::unordered_map<uint32_t, std::string> &nics, int mode)
{
    BM_ASSERT_RETURN(rpcService_ != 0, BM_ERROR);

    for (const auto &item: nics) {
        auto rankId = item.first;
        auto nic = item.second;
        auto ret = ConnectHcomService(rankId, nic);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to connect remote service, rankId" << rankId << " nic: " << nic << " ret: " << ret);
            continue;
        }
    }
    return BM_OK;
}

Result HcomRdmaTransManager::AsyncConnect(const std::unordered_map<uint32_t, std::string> &nics, int mode)
{
    return BM_OK;
}

Result HcomRdmaTransManager::WaitForConnected(int64_t timeoutNs)
{
    return BM_OK;
}

std::string HcomRdmaTransManager::GetNic()
{
    return localNic_;
}

Result HcomRdmaTransManager::QueryKey(void *addr, HybmTransKey& key)
{
    auto it = mrInfoMap_.find((uint64_t) addr);
    if (it == mrInfoMap_.end()) {
        BM_LOG_ERROR("Failed to find trans key, addr: " << addr);
        return BM_ERROR;
    }
    auto mrInfo = it->second;
    std::copy_n(mrInfo.lKey.keys, sizeof(uint32_t) * 4, key.keys);
    return BM_OK;
}

static Result CheckNic(const std::string &nic)
{
    const std::regex pattern(R"(^tcp://(\d{1,3}\.){3}\d{1,3}:(\d{1,5})$)");

    if (!std::regex_match(nic, pattern)) {
        BM_LOG_ERROR("Failed to match nic, nic: " << nic);
        return BM_INVALID_PARAM;
    }

    size_t colonPos = nic.find("tcp://");
    size_t atPos = nic.find(':');

    std::string ipStr = nic.substr(atPos + 7, colonPos - (atPos + 7));
    std::string portStr = nic.substr(colonPos + 1);

    try {
        int port = std::stoi(portStr);
        if (port < 1 || port > 65535) {
            BM_LOG_ERROR("Failed to check port, portStr: " << portStr << " nic: " << nic);
            return BM_INVALID_PARAM;
        }
    } catch (...) {
        BM_LOG_ERROR("Failed to get port, portStr: " << portStr << " nic: " << nic);
        return BM_INVALID_PARAM;
    }

    in_addr ip{};
    if (inet_aton(ipStr.c_str(), &ip) == 0) {
        BM_LOG_ERROR("Failed to check ip, nic: " << nic << " ipStr: " << ipStr);
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

Result HcomRdmaTransManager::CheckHybmTransOptions(HybmTransOptions &options)
{
    auto ret = CheckNic(options.nic);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to check nic, nic: " << options.nic << " ret: " << ret);
        return ret;
    }

    return BM_OK;
}

Result HcomRdmaTransManager::TransRpcHcomNewEndPoint(Hcom_Channel newCh, uint64_t usrCtx, const char *payLoad)
{
    BM_LOG_DEBUG("New hcom ch, ch: " << newCh << " usrCtx: " << usrCtx << " payLoad: " << payLoad);
    return BM_OK;
}

Result HcomRdmaTransManager::TransRpcHcomEndPointBroken(Hcom_Channel ch, uint64_t usrCtx, const char *payLoad)
{
    BM_LOG_DEBUG("Broken on hcom ch, ch: " << ch << " usrCtx: " << usrCtx << " payLoad: " << payLoad);
    if (GetInstance()->rpcService_ != 0) {
        DlHcomApi::ServiceDisConnect(GetInstance()->rpcService_, ch);
    }
    GetInstance()->RemoveChannel(ch);
    return BM_OK;
}

Result HcomRdmaTransManager::TransRpcHcomRequestReceived(Service_Context ctx, uint64_t usrCtx)
{
    BM_LOG_DEBUG("Receive hcom req, ctx: " << ctx << " usrCtx: " << usrCtx);
    return BM_OK;
}

Result HcomRdmaTransManager::TransRpcHcomRequestPosted(Service_Context ctx, uint64_t usrCtx)
{
    BM_LOG_DEBUG("Post hcom req, ctx: " << ctx << " usrCtx: " << usrCtx);
    return BM_OK;
}

Result HcomRdmaTransManager::TransRpcHcomOneSideDone(Service_Context ctx, uint64_t usrCtx)
{
    BM_LOG_DEBUG("Done hcom one side, ctx: " << ctx << " usrCtx: " << usrCtx);
    return BM_OK;
}

Result HcomRdmaTransManager::ConnectHcomService(uint32_t rankId, const std::string &url)
{
    auto it = connectMap_.find(rankId);
    if (it != connectMap_.end()) {
        BM_LOG_WARN("Stop connect to hcom service rankId: " << rankId << " url: " << url << " is connected");
        return BM_OK;
    }
    Hcom_Channel channel;
    Service_ConnectOptions options;
    options.clientGroupId = 0;
    options.serverGroupId = 0;
    options.linkCount = HCOM_TRANS_EP_SIZE;
    do {
        auto ret = DlHcomApi::ServiceConnect(rpcService_, url.c_str(), &channel, options);
        if (ret != 0) {
            BM_LOG_ERROR("Failed to connect remote service, rankId" << rankId << " url: " << url << " ret: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }
    } while (0);
    connectMap_[rankId] = channel;
    return BM_OK;
}

void HcomRdmaTransManager::RemoveChannel(Hcom_Channel ch)
{
    for (const auto &item: connectMap_) {
        if (item.second == ch) {
            connectMap_.erase(item.first);
            return;
        }
    }
}