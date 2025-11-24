/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "compose_transport_manager.h"

#include <vector>
#include <string>

#include "hybm_logger.h"
#include "host_hcom_transport_manager.h"
#include "device_rdma_transport_manager.h"
#include "hybm_str_helper.h"

using namespace ock::mf;
using namespace ock::mf::transport;

namespace {
const char NIC_DELIMITER = ';';
const std::string HOST_TRANSPORT_TYPE = "host#";
const std::string DEVICE_TRANSPORT_TYPE = "device#";
}

Result ComposeTransportManager::OpenHostTransport(const TransportOptions &options)
{
    if (hostTransportManager_ != nullptr) {
        BM_LOG_ERROR("Failed to open host transport is opened");
        return BM_ERROR;
    }
    hostTransportManager_ = host::HcomTransportManager::GetInstance();
    return hostTransportManager_->OpenDevice(options);
}

Result ComposeTransportManager::OpenDeviceTransport(const TransportOptions &options)
{
    if (deviceTransportManager_ != nullptr) {
        BM_LOG_ERROR("Failed to open device transport is opened");
        return BM_ERROR;
    }
    deviceTransportManager_ = std::make_shared<device::RdmaTransportManager>();
    return deviceTransportManager_->OpenDevice(options);
}

Result ComposeTransportManager::OpenDevice(const TransportOptions &options)
{
    std::vector<std::string> nicVec = StrHelper::Split(options.nic, NIC_DELIMITER);

    for (const auto &nic: nicVec) {
        Result ret = BM_ERROR;
        if (StrHelper::StartsWith(nic, HOST_TRANSPORT_TYPE)) {
            TransportOptions option = options;
            option.nic = nic.substr(HOST_TRANSPORT_TYPE.length());
            ret = OpenHostTransport(option);
        }

        if (StrHelper::StartsWith(nic, DEVICE_TRANSPORT_TYPE)) {
            TransportOptions option = options;
            option.nic = nic.substr(DEVICE_TRANSPORT_TYPE.length());
            ret = OpenDeviceTransport(option);
        }

        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to open device transport nic: " << nic);
            CloseDevice();
            return BM_ERROR;
        }
    }

    std::stringstream ss;
    if (hostTransportManager_) {
        ss << HOST_TRANSPORT_TYPE << hostTransportManager_->GetNic() << NIC_DELIMITER;
    }
    if (deviceTransportManager_) {
        ss << DEVICE_TRANSPORT_TYPE << deviceTransportManager_->GetNic() << NIC_DELIMITER;
    }
    nicInfo_ = ss.str();

    return BM_OK;
}

Result ComposeTransportManager::CloseDevice()
{
    if (deviceTransportManager_) {
        deviceTransportManager_->CloseDevice();
    }

    if (hostTransportManager_) {
        hostTransportManager_->CloseDevice();
    }

    return BM_OK;
}

Result ComposeTransportManager::RegisterMemoryRegion(const TransportMemoryRegion &mr)
{
    auto type = GetTransportTypeFromFlag(mr.flags);
    auto transport = GetTransportFromType(type);
    BM_ASSERT_RETURN(transport != nullptr, BM_INVALID_PARAM);
    Result ret = transport->RegisterMemoryRegion(mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to register memory region " << mr);
        return ret;
    }
    ComposeMemoryRegion cmr{mr.addr, mr.size, type};
    std::unique_lock<std::mutex> uniqueLock{mrsMutex_};
    mrs_.emplace(mr.addr, cmr);
    return BM_OK;
}

Result ComposeTransportManager::UnregisterMemoryRegion(uint64_t addr)
{
    std::unique_lock<std::mutex> uniqueLock{mrsMutex_};
    auto pos = mrs_.find(addr);
    if (pos == mrs_.end()) {
        uniqueLock.unlock();
        BM_LOG_ERROR("input address not register!");
        return BM_INVALID_PARAM;
    }

    auto transport = GetTransportFromType(pos->second.type);
    BM_ASSERT_RETURN(transport != nullptr, BM_ERROR);
    auto ret = transport->UnregisterMemoryRegion(addr);
    if (ret != 0) {
        uniqueLock.unlock();
        BM_LOG_ERROR("Failed to unregister mr addr, ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    mrs_.erase(pos);
    return BM_OK;
}

bool ComposeTransportManager::QueryHasRegistered(uint64_t addr, uint64_t size)
{
    std::unique_lock<std::mutex> uniqueLock{mrsMutex_};
    auto pos = mrs_.find(addr);
    if (pos == mrs_.end()) {
        uniqueLock.unlock();
        return false;
    }
    auto transport = GetTransportFromType(pos->second.type);
    uniqueLock.unlock();
    if (transport == nullptr) {
        return false;
    }
    return transport->QueryHasRegistered(addr, size);
}

Result ComposeTransportManager::QueryMemoryKey(uint64_t addr, TransportMemoryKey &key)
{
    std::unique_lock<std::mutex> uniqueLock{mrsMutex_};
    auto pos = mrs_.find(addr);
    if (pos == mrs_.end()) {
        uniqueLock.unlock();
        BM_LOG_ERROR("input address not register!");
        return BM_INVALID_PARAM;
    }
    auto transport = GetTransportFromType(pos->second.type);
    BM_ASSERT_RETURN(transport != nullptr, BM_ERROR);
    uniqueLock.unlock();
    return transport->QueryMemoryKey(addr, key);
}

Result ComposeTransportManager::ParseMemoryKey(const TransportMemoryKey &key, uint64_t &addr, uint64_t &size)
{
    int ret = BM_ERROR;
    if (key.keys[0] == TT_HCCP && deviceTransportManager_ != nullptr) {
        return deviceTransportManager_->ParseMemoryKey(key, addr, size);
    }

    if (key.keys[0] == TT_HCOM && hostTransportManager_ != nullptr) {
        return hostTransportManager_->ParseMemoryKey(key, addr, size);
    }

    BM_LOG_ERROR("ParseMemoryKey with type: " << key.keys[0] << " failed!");
    return ret;
}

void ComposeTransportManager::GetHostPrepareOptions(const HybmTransPrepareOptions &param,
                                                    HybmTransPrepareOptions &hostOptions)
{
    auto options = param.options;
    for (const auto &item: options) {
        auto rankId = item.first;
        TransportRankPrepareInfo info{};
        std::vector<std::string> nicVec = StrHelper::Split(item.second.nic, NIC_DELIMITER);
        for (const auto &nic: nicVec) {
            if (StrHelper::StartsWith(nic, HOST_TRANSPORT_TYPE)) {
                info.nic = nic.substr(HOST_TRANSPORT_TYPE.length());
            }
        }

        for (auto &key : item.second.memKeys) {
            if (key.keys[0] == TT_HCOM) {
                info.memKeys.emplace_back(key);
            }
        }
        hostOptions.options.emplace(rankId, info);
    }
}

void ComposeTransportManager::GetDevicePrepareOptions(const HybmTransPrepareOptions &param,
                                                      HybmTransPrepareOptions &deviceOptions)
{
    auto options = param.options;
    for (const auto &item: options) {
        auto rankId = item.first;
        TransportRankPrepareInfo info{};
        std::vector<std::string> nicVec = StrHelper::Split(item.second.nic, NIC_DELIMITER);
        for (const auto &nic: nicVec) {
            if (StrHelper::StartsWith(nic, DEVICE_TRANSPORT_TYPE)) {
                info.nic = nic.substr(DEVICE_TRANSPORT_TYPE.length());
            }
        }

        for (auto &key : item.second.memKeys) {
            if (key.keys[0] == TT_HCCP) {
                info.memKeys.emplace_back(key);
            }
        }
        deviceOptions.options.emplace(rankId, info);
    }
}

Result ComposeTransportManager::Prepare(const HybmTransPrepareOptions &options)
{
    Result ret = BM_OK;
    if (hostTransportManager_) {
        HybmTransPrepareOptions hostOptions{};
        GetHostPrepareOptions(options, hostOptions);
        ret = hostTransportManager_->Prepare(hostOptions);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to prepare host ret: " << ret);
        }
    }
    if (deviceTransportManager_) {
        HybmTransPrepareOptions deviceOptions{};
        GetDevicePrepareOptions(options, deviceOptions);
        ret = deviceTransportManager_->Prepare(deviceOptions);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to prepare host ret: " << ret);
        }
    }
    return ret;
}

Result ComposeTransportManager::RemoveRanks(const std::vector<uint32_t> &removedRanks)
{
    Result lastResult = BM_OK;
    if (hostTransportManager_) {
        auto ret = hostTransportManager_->RemoveRanks(removedRanks);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed for host transport manager remove ranks ret: " << ret);
            lastResult = ret;
        }
    }

    if (deviceTransportManager_) {
        auto ret = deviceTransportManager_->RemoveRanks(removedRanks);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed for device transport manager remove ranks ret: " << ret);
            lastResult = ret;
        }
    }

    return lastResult;
}

Result ComposeTransportManager::Connect()
{
    if (hostTransportManager_) {
        auto ret = hostTransportManager_->Connect();
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to connect host ret: " << ret);
            return ret;
        }
    }

    if (deviceTransportManager_) {
        auto ret = deviceTransportManager_->Connect();
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to connect host ret: " << ret);
            return ret;
        }
    }
    return BM_OK;
}

Result ComposeTransportManager::AsyncConnect()
{
    if (hostTransportManager_) {
        auto ret = hostTransportManager_->AsyncConnect();
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to connect host ret: " << ret);
            return ret;
        }
    }

    if (deviceTransportManager_) {
        auto ret = deviceTransportManager_->AsyncConnect();
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to connect host ret: " << ret);
            return ret;
        }
    }
    return BM_OK;
}

Result ComposeTransportManager::WaitForConnected(int64_t timeoutNs)
{
    if (hostTransportManager_) {
        auto ret = hostTransportManager_->WaitForConnected(timeoutNs);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to connect host ret: " << ret);
            return ret;
        }
    }

    if (deviceTransportManager_) {
        auto ret = deviceTransportManager_->WaitForConnected(timeoutNs);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to connect host ret: " << ret);
            return ret;
        }
    }
    return BM_OK;
}

const std::string &ComposeTransportManager::GetNic() const
{
    return nicInfo_;
}

Result ComposeTransportManager::ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    auto transport = GetTransportFromAddress(lAddr);
    BM_ASSERT_RETURN(transport != nullptr, BM_ERROR);
    return transport->ReadRemote(rankId, lAddr, rAddr, size);
}

Result ComposeTransportManager::WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    auto transport = GetTransportFromAddress(lAddr);
    BM_ASSERT_RETURN(transport != nullptr, BM_ERROR);
    return transport->WriteRemote(rankId, lAddr, rAddr, size);
}

Result ComposeTransportManager::ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_LOG_ERROR("not support ReadRemoteAsync!");
    return BM_ERROR;
}

Result ComposeTransportManager::WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_LOG_ERROR("not support WriteRemoteAsync!");
    return BM_ERROR;
}

Result ComposeTransportManager::Synchronize(uint32_t rankId)
{
    BM_LOG_ERROR("not support Synchronize!");
    return BM_ERROR;
}

std::shared_ptr<TransportManager> ComposeTransportManager::GetTransportFromType(TransportType type)
{
    switch (type) {
        case TT_HCCP:
            return deviceTransportManager_;
        case TT_HCOM:
            return hostTransportManager_;
        default:
            return nullptr;
    }
}

std::shared_ptr<TransportManager> ComposeTransportManager::GetTransportFromAddress(uint64_t addr)
{
    TransportType type = TT_BUTT;
    std::unique_lock<std::mutex> uniqueLock{mrsMutex_};
    for (const auto &item: mrs_) {
        auto mr = item.second;
        if (mr.addr <= addr && mr.addr + mr.size >= addr) {
            type = mr.type;
            break;
        }
    }
    switch (type) {
        case TT_HCCP:
            return deviceTransportManager_;
        case TT_HCOM:
            return hostTransportManager_;
        default:
            return nullptr;
    }
}

TransportType ComposeTransportManager::GetTransportTypeFromFlag(uint32_t flags)
{
    // 取index开始的后两位作为TransportType
    static uint8_t index = 0;
    auto type = flags & ((3) << index);
    if (type == REG_MR_FLAG_DRAM) {
        return TT_HCOM;
    }
    if (type == REG_MR_FLAG_HBM) {
        return TT_HCCP;
    }
    return TT_BUTT;
}

Result ComposeTransportManager::UpdateRankOptions(const HybmTransPrepareOptions &options)
{
    Result ret = BM_OK;
    if (hostTransportManager_) {
        HybmTransPrepareOptions hostOptions{};
        GetHostPrepareOptions(options, hostOptions);
        ret = hostTransportManager_->UpdateRankOptions(hostOptions);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to prepare host ret: " << ret);
        }
    }
    if (deviceTransportManager_) {
        HybmTransPrepareOptions deviceOptions{};
        GetDevicePrepareOptions(options, deviceOptions);
        ret = deviceTransportManager_->UpdateRankOptions(deviceOptions);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to prepare host ret: " << ret);
        }
    }
    return ret;
}
