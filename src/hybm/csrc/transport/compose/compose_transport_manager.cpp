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
#include "compose_transport_manager.h"

#include <vector>
#include <string>

#include "hybm_logger.h"
#include "host_hcom_transport_manager.h"
#include "device_rdma_transport_manager.h"
#include "mf_str_util.h"

using namespace ock::mf;
using namespace ock::mf::transport;

namespace {
const char NIC_DELIMITER = ';';
const std::string HOST_TRANSPORT_TYPE = "host#";
const std::string DEVICE_TRANSPORT_TYPE = "device#";
const uint32_t HOST_PROTOCOL = HYBM_DOP_TYPE_HOST_TCP | HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_URMA;
} // namespace

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
    options_ = options;
    Result ret = BM_ERROR;
    if (options_.protocol & HOST_PROTOCOL) {
        ret = OpenHostTransport(options);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to open device transport nic: " << options.nic);
            CloseDevice();
            return BM_ERROR;
        }
    }

    if (options_.protocol & HYBM_DOP_TYPE_DEVICE_RDMA) {
        ret = OpenDeviceTransport(options);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to open device transport nic: " << options.nic);
            CloseDevice();
            return BM_ERROR;
        }
    }

    std::stringstream ss;
    if ((options_.protocol & HOST_PROTOCOL)) {
        if (hostTransportManager_ == nullptr) {
            BM_LOG_ERROR("Failed to open host transport nic: " << options.nic);
            CloseDevice();
            return BM_ERROR;
        }
        ss << HOST_TRANSPORT_TYPE << hostTransportManager_->GetNic() << NIC_DELIMITER;
    }
    if ((options_.protocol & HYBM_DOP_TYPE_DEVICE_RDMA)) {
        if (deviceTransportManager_ == nullptr) {
            BM_LOG_ERROR("Failed to open device transport nic: " << options.nic);
            CloseDevice();
            return BM_ERROR;
        }
        ss << DEVICE_TRANSPORT_TYPE << deviceTransportManager_->GetNic() << NIC_DELIMITER;
    }
    nicInfo_ = ss.str();
    BM_LOG_INFO("Success to open device rankId:" << options_.rankId
        << " protocol:" << options_.protocol << " nic:" << nicInfo_);
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
    if (deviceTransportManager_) {
        Result ret = deviceTransportManager_->RegisterMemoryRegion(mr);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to register memory region " << mr);
            return ret;
        }
    }
    if (hostTransportManager_) {
        Result ret = hostTransportManager_->RegisterMemoryRegion(mr);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to register memory region " << mr);
            return ret;
        }
    }
    ComposeMemoryRegion cmr{mr.addr, mr.size, TT_COMPOSE};
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
    if (deviceTransportManager_) {
        Result ret = deviceTransportManager_->UnregisterMemoryRegion(addr);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to unregister mr addr, ret: " << ret);
            return ret;
        }
    }
    if (hostTransportManager_) {
        Result ret = hostTransportManager_->UnregisterMemoryRegion(addr);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to unregister mr addr, ret: " << ret);
            return ret;
        }
    }
    mrs_.erase(pos);
    return BM_OK;
}

bool ComposeTransportManager::QueryHasRegistered(uint64_t addr, uint64_t size)
{
    std::unique_lock<std::mutex> uniqueLock{mrsMutex_};
    auto pos = mrs_.lower_bound(addr);
    if (pos == mrs_.end() || pos->first + pos->second.size < addr + size) {
        uniqueLock.unlock();
        return false;
    }
    return true;
}

Result ComposeTransportManager::QueryMemoryKey(uint64_t addr, TransportMemoryKey &key)
{
    // device固定占0~13， host占14~27
    if (deviceTransportManager_) {
        TransportMemoryKey tmp{};
        auto ret = deviceTransportManager_->QueryMemoryKey(addr, tmp);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to query device transport memKey addr:" << std::hex << addr);
            return ret;
        }
        WriteDeviceRdmaMemoryKey(tmp, key);
    }

    if (hostTransportManager_) {
        TransportMemoryKey tmp{};
        auto ret = hostTransportManager_->QueryMemoryKey(addr, tmp);
        if (ret != BM_OK) {
            BM_LOG_WARN("Failed to query host transport memKey addr:" << std::hex << addr);
            // HCOM 无法处理HBM池，这里直接返回，兼容即开启HBM池又要走HCOM通信的场景
        }
        WriteHcomMemoryKey(tmp, key);
    }

    return BM_OK;
}

void ComposeTransportManager::GetHostPrepareOptions(const HybmTransPrepareOptions &param,
                                                    HybmTransPrepareOptions &hostOptions)
{
    auto options = param.options;
    for (const auto &item : options) {
        auto rankId = item.first;
        uint32_t opType = tagManager_->GetRank2RankOpType(rankId, options_.rankId);
        if (!(opType & HOST_PROTOCOL)) {
            BM_LOG_INFO("remote rank:" << rankId << " to local rank:" << options_.rankId
                << " use protocol:" << opType << " skip host connect");
            continue;
        }
        TransportRankPrepareInfo info{};
        std::vector<std::string> nicVec = StrUtil::Split(item.second.nic, NIC_DELIMITER);
        for (const auto &nic : nicVec) {
            if (StrUtil::StartWith(nic, HOST_TRANSPORT_TYPE)) {
                info.nic = nic.substr(HOST_TRANSPORT_TYPE.length());
            }
        }

        for (auto &key : item.second.memKeys) {
            TransportMemoryKey tmp{};
            ReadHcomMemoryKey(key, tmp);
            info.memKeys.emplace_back(tmp);
        }
        hostOptions.options.emplace(rankId, info);
    }
}

void ComposeTransportManager::GetDevicePrepareOptions(const HybmTransPrepareOptions &param,
                                                      HybmTransPrepareOptions &deviceOptions)
{
    auto options = param.options;
    for (const auto &item : options) {
        auto rankId = item.first;
        uint32_t opType = tagManager_->GetRank2RankOpType(rankId, options_.rankId);
        if (!(opType & HYBM_DOP_TYPE_DEVICE_RDMA)) {
            BM_LOG_INFO("remote rank:" << rankId << " to local rank:" << options_.rankId
                                       << " use protocol:" << opType << " skip device_rdma connect");
            continue;
        }
        TransportRankPrepareInfo info{};
        std::vector<std::string> nicVec = StrUtil::Split(item.second.nic, NIC_DELIMITER);
        for (const auto &nic : nicVec) {
            if (StrUtil::StartWith(nic, DEVICE_TRANSPORT_TYPE)) {
                info.nic = nic.substr(DEVICE_TRANSPORT_TYPE.length());
            }
        }

        for (auto &key : item.second.memKeys) {
            TransportMemoryKey tmp{};
            ReadDeviceRdmaMemoryKey(key, tmp);
            info.memKeys.emplace_back(key);
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
            return ret;
        }
    }
    if (deviceTransportManager_) {
        HybmTransPrepareOptions deviceOptions{};
        GetDevicePrepareOptions(options, deviceOptions);
        ret = deviceTransportManager_->Prepare(deviceOptions);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to prepare host ret: " << ret);
            return ret;
        }
    }
    return BM_OK;
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
    uint32_t opType = tagManager_->GetRank2RankOpType(rankId, options_.rankId);
    // 传输顺序 device_rdma -> host_rdma
    if ((opType & HYBM_DOP_TYPE_DEVICE_RDMA) && deviceTransportManager_ != nullptr) {
        auto ret = deviceTransportManager_->ReadRemote(rankId, lAddr, rAddr, size);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Failed to ReadRemote by device transport ret:" << ret);
    }

    if (opType & HOST_PROTOCOL) {
        auto ret = hostTransportManager_->ReadRemote(rankId, lAddr, rAddr, size);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Failed to ReadRemote by host transport ret:" << ret);
    }

    BM_LOG_ERROR("Failed to ReadRemote.");
    return BM_ERROR;
}

Result ComposeTransportManager::WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    uint32_t opType = tagManager_->GetRank2RankOpType(rankId, options_.rankId);
    if ((opType & HYBM_DOP_TYPE_DEVICE_RDMA) && deviceTransportManager_ != nullptr) {
        auto ret = deviceTransportManager_->WriteRemote(rankId, lAddr, rAddr, size);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Failed to WriteRemote by device transport ret:" << ret);
    }

    if (opType & HOST_PROTOCOL) {
        auto ret = hostTransportManager_->WriteRemote(rankId, lAddr, rAddr, size);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Failed to WriteRemote by host transport ret:" << ret);
    }

    BM_LOG_ERROR("Failed to WriteRemote.");
    return BM_ERROR;
}

Result ComposeTransportManager::ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    uint32_t opType = tagManager_->GetRank2RankOpType(rankId, options_.rankId);
    if ((opType & HYBM_DOP_TYPE_DEVICE_RDMA) && deviceTransportManager_ != nullptr) {
        auto ret = deviceTransportManager_->ReadRemoteAsync(rankId, lAddr, rAddr, size);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Failed to ReadRemoteAsync by device transport ret:" << ret);
    }

    if (opType & HOST_PROTOCOL) {
        auto ret = hostTransportManager_->ReadRemoteAsync(rankId, lAddr, rAddr, size);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Failed to ReadRemoteAsync by host transport ret:" << ret <<
            " remote rankId:" << rankId << " lAddr:" << std::hex << lAddr << " rAddr:" << rAddr << " size:" << size);
    }

    BM_LOG_ERROR("Failed to ReadRemote.");
    return BM_ERROR;
}

Result ComposeTransportManager::WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    uint32_t opType = tagManager_->GetRank2RankOpType(rankId, options_.rankId);
    if ((opType & HYBM_DOP_TYPE_DEVICE_RDMA) && deviceTransportManager_ != nullptr) {
        auto ret = deviceTransportManager_->WriteRemoteAsync(rankId, lAddr, rAddr, size);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Failed to ReadRemoteAsync by device transport ret:" << ret);
    }

    if (opType & HOST_PROTOCOL) {
        auto ret = hostTransportManager_->WriteRemoteAsync(rankId, lAddr, rAddr, size);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Failed to ReadRemoteAsync by host transport ret:" << ret);
    }

    BM_LOG_ERROR("Failed to WriteRemote.");
    return BM_ERROR;
}

Result ComposeTransportManager::Synchronize(uint32_t rankId)
{
    uint32_t opType = tagManager_->GetRank2RankOpType(rankId, options_.rankId);
    if ((opType & HYBM_DOP_TYPE_DEVICE_RDMA) && deviceTransportManager_ != nullptr) {
        auto ret = deviceTransportManager_->Synchronize(rankId);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Failed to ReadRemoteAsync by device transport ret:" << ret);
    }

    if ((opType & HOST_PROTOCOL) && hostTransportManager_ != nullptr) {
        auto ret = hostTransportManager_->Synchronize(rankId);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Failed to ReadRemoteAsync by host transport ret:" << ret);
    }

    BM_LOG_ERROR("Failed to WriteRemote.");
    return BM_ERROR;
}

Result ComposeTransportManager::UpdateRankOptions(const HybmTransPrepareOptions &options)
{
    Result ret = BM_OK;
    if (hostTransportManager_) {
        HybmTransPrepareOptions hostOptions{};
        GetHostPrepareOptions(options, hostOptions);
        BM_LOG_INFO("Try to update host transport rank options: " << hostOptions);
        ret = hostTransportManager_->UpdateRankOptions(hostOptions);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to prepare host ret: " << ret);
        }
    }
    if (deviceTransportManager_) {
        HybmTransPrepareOptions deviceOptions{};
        GetDevicePrepareOptions(options, deviceOptions);
        BM_LOG_INFO("Try to update host transport rank options: " << deviceOptions);
        ret = deviceTransportManager_->UpdateRankOptions(deviceOptions);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to prepare host ret: " << ret);
        }
    }
    return ret;
}
