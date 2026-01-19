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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <memory>
#include <thread>
#include <algorithm>
#include <cstring>

#include "hybm.h"
#include "hybm_big_mem.h"
#include "hybm_data_op.h"
#include "smem_net_common.h"
#include "smem_store_factory.h"
#include "smem_trans_entry_manager.h"
#include "smem_trans_entry.h"

namespace ock {
namespace smem {
SmemTransEntryPtr SmemTransEntry::Create(const std::string &name, const std::string &storeUrl,
                                         const smem_trans_config_t &config)
{
    /* create entry and initialize */
    SmemTransEntryPtr transEntry;
    auto result = SmemTransEntryManager::Instance().CreateEntryByName(name, storeUrl, config, transEntry);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("create trans entry failed, probably out of memory");
        return nullptr;
    }

    /* initialize */
    result = transEntry->Initialize(config);
    if (result != SM_OK) {
        SmemTransEntryManager::Instance().RemoveEntryByName(name);
        SM_LOG_AND_SET_LAST_ERROR("initialize trans entry failed, result " << result);
        return nullptr;
    }

    return transEntry;
}

SmemTransEntry::~SmemTransEntry()
{
    UnInitialize();
    if (entity_ != nullptr) {
        hybm_destroy_entity(entity_, 0);
        entity_ = nullptr;
    }
}

int32_t SmemTransEntry::Initialize(const smem_trans_config_t &config)
{
    entityId_ = (16U << 3U) + 1U;
    if (!ParseTransName(name_, workerUniqueId_.address, workerUniqueId_.port)) {
        return SM_INVALID_PARAM;
    }

    auto ret = storeHelper_.Initialize(entityId_, static_cast<int32_t>(config.initTimeout), config.startConfigServer);
    SM_VALIDATE_RETURN(ret == SM_OK, "store helper initialize failed: " << ret, ret);

    ret = storeHelper_.GenerateRankId(config, rankId_);
    SM_VALIDATE_RETURN(ret == SM_OK, "store helper generate rankId failed: " << ret, ret);

    config_ = config;
    auto options = GenerateHybmOptions();
    options.bmDataOpType = static_cast<hybm_data_op_type>(HYBM_DOP_TYPE_DEFAULT);
    if (config.dataOpType & SMEMB_DATA_OP_SDMA) {
#if !defined(ASCEND_NPU)
        SM_LOG_ERROR("current memfabric-hybrid binary is not built for ascend npu, can not use device_sdma optype.");
        return SM_ERROR;
#endif
        auto temp = static_cast<uint32_t>(options.bmDataOpType) | HYBM_DOP_TYPE_SDMA;
        options.bmDataOpType = static_cast<hybm_data_op_type>(temp);
    }
    if (config.dataOpType & SMEMB_DATA_OP_DEVICE_RDMA) {
#if !defined(ASCEND_NPU)
        SM_LOG_ERROR("current memfabric-hybrid binary is not built for ascend npu, can not use device_rdma optype.");
        return SM_ERROR;
#endif
        auto temp = static_cast<uint32_t>(options.bmDataOpType) | HYBM_DOP_TYPE_DEVICE_RDMA;
        options.bmDataOpType = static_cast<hybm_data_op_type>(temp);
    }
    entity_ = hybm_create_entity(entityId_, &options, 0);
    SM_VALIDATE_RETURN(entity_ != nullptr, "create new entity failed.", SM_ERROR);

    ret = hybm_export(entity_, nullptr, 0, &deviceInfo_);
    SM_VALIDATE_RETURN(ret == SM_OK, "HybmExport device info failed: " << ret, SM_ERROR);

    size_t outputSize;
    ret = hybm_export_slice_size(entity_, &outputSize);
    SM_VALIDATE_RETURN(ret == SM_OK, "HybmExport device info failed: " << ret, SM_ERROR);
    sliceInfoSize_ = outputSize;
    storeHelper_.SetSliceExportSize(outputSize);

    ret = storeHelper_.StoreDeviceInfo(deviceInfo_);
    SM_VALIDATE_RETURN(ret == SM_OK, "store device info failed: " << ret, ret);

    auto brokenHandler = [this] { return StartWatchConnectThread(); };
    storeHelper_.RegisterBrokenHandler(brokenHandler);
    StartWatchThread();
    return SM_OK;
}

void SmemTransEntry::UnInitialize()
{
    std::unique_lock<std::mutex> locker{watchMutex_};
    watchRunning_ = false;
    locker.unlock();
    watchCond_.notify_one();
    watchConnectRunning_ = false;

    if (watchThread_.joinable()) {
        try {
            watchThread_.join();
        } catch (const std::system_error &e) {
            SM_LOG_ERROR("watch thread join failed: " << e.what());
        }
    }
    if (watchConnectThread_.joinable()) {
        try {
            watchConnectThread_.join();
        } catch (const std::system_error &e) {
            SM_LOG_ERROR("watch connect thread join failed: " << e.what());
        }
    }
    storeHelper_.Destroy();
}

Result SmemTransEntry::RegisterLocalMemory(const void *address, uint64_t size, uint32_t flags)
{
    if (entity_ == nullptr) {
        SM_LOG_ERROR("not create entity.");
        return SM_ERROR;
    }

    if (address == nullptr || size == 0) {
        SM_LOG_ERROR("input address or size is invalid.");
        return SM_INVALID_PARAM;
    }

    AlignMemory(address, size);
    return RegisterOneMemory(address, size, flags);
}

Result SmemTransEntry::RegisterLocalMemories(const std::vector<std::pair<const void *, size_t>> &regMemories,
                                             uint32_t flags)
{
    if (entity_ == nullptr) {
        SM_LOG_ERROR("not create entity.");
        return SM_ERROR;
    }

    if (regMemories.empty()) {
        return SM_OK;
    }

    for (auto it : regMemories) {
        if (it.first == nullptr || it.second == 0) {
            SM_LOG_ERROR("input address or size is invalid.");
            return SM_INVALID_PARAM;
        }
    }

    auto alignedMemories = regMemories;
    for (auto &it : alignedMemories) {
        AlignMemory(it.first, it.second);
    }
    auto mm = CombineMemories(alignedMemories);
    for (auto &m : mm) {
        auto ret = RegisterOneMemory(m.first, m.second, flags);
        if (ret != 0) {
            return ret;
        }
    }

    return SM_OK;
}

static std::string uniqueToString(const WorkerId &unique)
{
    std::ostringstream oss;
    constexpr int WIDTH = 2;
    for (size_t i = 0; i < unique.size(); ++i) {
        oss << std::hex << std::setw(WIDTH) << std::setfill('0') << static_cast<int>(unique[i]);
        if (i < unique.size() - 1) {
            oss << ":";
        }
    }
    return oss.str();
}

Result SmemTransEntry::SyncTransfer(void *localAddr, const std::string &remoteUniqueId, void *remoteAddr,
                                    size_t dataSize, smem_bm_copy_type opcode, void *stream, uint32_t flags)
{
    return BatchSyncTransfer(&localAddr, remoteUniqueId, &remoteAddr, &dataSize, 1U, opcode, stream, flags);
}

Result SmemTransEntry::BatchSyncTransfer(void *localAddrs[], const std::string &remoteUniqueId, void *remoteAddrs[],
                                         const size_t dataSizes[], uint32_t batchSize, smem_bm_copy_type opcode,
                                         void *stream, uint32_t flags)
{
    SM_VALIDATE_RETURN(localAddrs != nullptr, "invalid localAddrs, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(remoteAddrs != nullptr, "invalid remoteAddrs, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(dataSizes != nullptr, "invalid dataSizes, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(batchSize != 0, "invalid batchSize, which is 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(flags == 0 || flags == COPY_EXTEND_FLAG, "invalid flags", SM_INVALID_PARAM);
    for (auto i = 0U; i < batchSize; i++) {
        SM_VALIDATE_RETURN(localAddrs[i] != nullptr, "localAddrs, which is null", SM_INVALID_PARAM);
        SM_VALIDATE_RETURN(remoteAddrs[i] != nullptr, "remoteAddrs, which is null", SM_INVALID_PARAM);
        SM_VALIDATE_RETURN(dataSizes[i] != 0, "invalid dataSizes, which is 0", SM_INVALID_PARAM);
    }
    WorkerId unique;
    auto ret = ParseNameToUniqueId(remoteUniqueId, unique);
    if (ret != 0) {
        return ret;
    }

    std::vector<void *> mappedAddress(batchSize);

    mf::ReadGuard locker(remoteSliceRwMutex_);
    auto it = remoteSlices_.find(unique);
    if (it == remoteSlices_.end()) {
        SM_LOG_ERROR("session:(" << remoteUniqueId << ")(" << uniqueToString(unique) << ") not found.");
        return SM_INVALID_PARAM;
    }

    for (auto i = 0U; i < batchSize; i++) {
        auto pos = it->second.lower_bound(remoteAddrs[i]);
        if (pos == it->second.end()) {
            SM_LOG_ERROR("remote address[" << i << "] is invalid.");
            return SM_INVALID_PARAM;
        }

        if ((const uint8_t *)remoteAddrs[i] + dataSizes[i] > (const uint8_t *)(pos->first) + pos->second.size) {
            SM_LOG_ERROR("address[" << i << "], size[" << i << "]=" << dataSizes[i] << " out of range.");
            return SM_INVALID_PARAM;
        }

        mappedAddress[i] =
            (uint8_t *)pos->second.address + ((const uint8_t *)remoteAddrs[i] - (const uint8_t *)(pos->first));
    }

    uint32_t flag = flags | ((stream != nullptr) ? ASYNC_COPY_FLAG : 0);
    switch (opcode) {
        case SMEMB_COPY_L2G: {
            hybm_batch_copy_params copyParams = {localAddrs, mappedAddress.data(), dataSizes, batchSize};
            ret = hybm_data_batch_copy(entity_, &copyParams, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, stream, flag);
        } break;
        case SMEMB_COPY_G2L: {
            hybm_batch_copy_params copyParams = {mappedAddress.data(), localAddrs, dataSizes, batchSize};
            ret = hybm_data_batch_copy(entity_, &copyParams, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, stream, flag);
        } break;
        default:
            SM_LOG_ERROR("unexpect copy type[" << opcode << "] is invalid.");
            return SM_INVALID_PARAM;
    }
    if (ret != 0) {
        SM_LOG_ERROR("batch copy data failed:" << ret);
    }
    return ret;
}

bool SmemTransEntry::ParseTransName(const std::string &name, ock::mf::net_addr_t &ip, uint16_t &port)
{
    UrlExtraction extraction;
    int ret = extraction.ExtractIpPortFromUrl(std::string("tcp://").append(name));
    if (ret != 0) {
        SM_LOG_ERROR("parse name failed: " << ret);
        return false;
    }

    struct in6_addr addr6;
    if (inet_pton(AF_INET6, extraction.ip.c_str(), &addr6) == 1) {
        ip.ip.ipv6 = addr6;
        ip.type = ock::mf::IpV6;
    } else {
        struct in_addr addr4;
        if (inet_pton(AF_INET, extraction.ip.c_str(), &addr4) != 1) {
            SM_LOG_ERROR("Invalid IP address format: " << extraction.ip);
            return false;
        }
        ip.ip.ipv4.s_addr = ntohl(addr4.s_addr);
        ip.type = ock::mf::IpV4;
    }
    port = extraction.port;
    return true;
}

Result SmemTransEntry::StartWatchConnectThread()
{
    if (watchConnectThread_.joinable()) {
        try {
            watchConnectThread_.join();
        } catch (const std::system_error &e) {
            SM_LOG_ERROR("watch connect thread join failed: " << e.what());
        }
    }
    watchConnectThread_ = std::thread([this]() {
        while (watchConnectRunning_) {
            if (WatchConnectTaskOneLoop() == SM_OK) {
                SM_LOG_INFO("watch connect success, exit.");
                return 0;
            }
            sleep(1);
        }
        return 0;
    });
    SM_LOG_DEBUG("start background connect thread success");
    return 0;
}

Result SmemTransEntry::WatchConnectTaskOneLoop()
{
    if (!storeHelper_.CheckServerStatus()) {
        auto status = storeHelper_.ReConnect();
        if (status == SM_RECONNECT) {
            storeHelper_.AlterServerStatus(true);
            status = ReInitialize();
            if (status != SM_OK) {
                SM_LOG_ERROR("reconnect success, but rewrite error.");
                return SM_ERROR;
            }
            return SM_OK;
        } else if (status == SM_ERROR) {
            const uint32_t logInternal = 5;
            SM_LOG_LIMIT_WARN(logInternal, "Reconnect failed, wait next connect.");
            return SM_ERROR;
        }
    }
    return SM_OK;
}

Result SmemTransEntry::StartWatchThread()
{
    SM_LOG_DEBUG("start background thread");
    watchThread_ = std::thread([this]() {
        std::unique_lock<std::mutex> locker{watchMutex_};
        const std::chrono::seconds WATCH_INTERVAL(3);
        while (watchRunning_) {
            WatchTaskOneLoop();
            watchCond_.wait_for(locker, WATCH_INTERVAL);
        }
    });
    return 0;
}

void SmemTransEntry::WatchTaskOneLoop()
{
    static int64_t times = 0;
    if (!storeHelper_.CheckServerStatus()) {
        times = 0;
        return;
    }
    WatchTaskFindNewRanks();

    if (times >= 2U) {
        WatchTaskFindNewSlices();
    }
    times++;
}

void SmemTransEntry::WatchTaskFindNewRanks()
{
    auto importNewRanks = [this](const std::vector<hybm_exchange_info> &addInfo) {
        int32_t ret;
        if (!addInfo.empty()) {
            ret = hybm_import(entity_, addInfo.data(), addInfo.size(), nullptr, 0);
            if (ret != 0) {
                SM_LOG_ERROR("import new ranks failed: " << ret);
            }
            return ret;
        }
        return 0;
    };
    storeHelper_.FindNewRemoteRanks(importNewRanks);
}

void SmemTransEntry::CleanupRemoteSlices(const std::vector<StoredSliceInfo> &rmSs)
{
    for (auto i = 0U; i < rmSs.size(); i++) {
        WorkerIdUnion workerId{rmSs[i].session};
        SM_LOG_DEBUG("remove remote slice for : " << uniqueToString(workerId.workerId));
        auto it = remoteSlices_.find(workerId.workerId);
        if (it == remoteSlices_.end()) {
            continue;
        }
        auto sIt = it->second.find(rmSs[i].address);
        if (sIt == it->second.end()) {
            continue;
        }
        it->second.erase(sIt);
        if (it->second.empty()) {
            SM_LOG_INFO("remove workId: " << uniqueToString(workerId.workerId) << " remote slice map.");
            remoteSlices_.erase(it);
        }
    }
}

void SmemTransEntry::RemoveRanks(std::set<uint32_t> &rankSet)
{
    for (auto rankId : rankSet) {
        auto ret = hybm_remove_imported(entity_, rankId, 0);
        if (ret != 0) {
            SM_LOG_ERROR("remove rank:" << rankId << " failed: " << ret);
        }
    }
}

void SmemTransEntry::WatchTaskFindNewSlices()
{
    auto importNewSlices = [this](const std::vector<hybm_exchange_info> &addInfo,
                                  const std::vector<StoredSliceInfo> &addSs, const std::vector<StoredSliceInfo> &rmSs) {
        int32_t ret;
        if (rmSs.size() != 0) {
            SM_LOG_DEBUG("remove slices count=" << rmSs.size());
            ock::mf::WriteGuard locker(remoteSliceRwMutex_);
            CleanupRemoteSlices(rmSs);
            std::set<uint32_t> rankSet;
            for (auto i = 0U; i < rmSs.size(); i++) {
                uint32_t rankId = static_cast<uint32_t>(rmSs[i].rankId);
                rankSet.insert(rankId);
            }
            RemoveRanks(rankSet);
        }
        if (addInfo.size() != 0) {
            std::vector<void *> addresses(addInfo.size());
            ret = hybm_import(entity_, addInfo.data(), addInfo.size(), addresses.data(), 0);
            if (ret != 0) {
                SM_LOG_ERROR("import new slices failed: " << ret);
                return ret;
            }
            SM_LOG_DEBUG("import slices count=" << addInfo.size());

            ock::mf::WriteGuard locker(remoteSliceRwMutex_);
            for (auto i = 0U; i < addSs.size(); i++) {
                WorkerIdUnion workerId{addSs[i].session};
                SM_LOG_DEBUG("add remote slice for : " << uniqueToString(workerId.workerId));
                remoteSlices_[workerId.workerId].emplace(addSs[i].address,
                                                         LocalMapAddress{addresses[i], addSs[i].size});
            }
        }
        return 0;
    };
    storeHelper_.FindNewRemoteSlices(importNewSlices);
}

Result SmemTransEntry::ParseNameToUniqueId(const std::string &name, WorkerId &uniqueId)
{
    WorkerUniqueId workerUniqueId;
    auto it = nameToWorkerId.find(name);
    if (it != nameToWorkerId.end()) {
        /* fast path */
        uniqueId = it->second;
        return SM_OK;
    }
    auto success = ParseTransName(name, workerUniqueId.address, workerUniqueId.port);
    if (!success) {
        SM_LOG_ERROR("parse name failed.");
        return -1;
    }

    WorkerIdUnion workerId{workerUniqueId};
    uniqueId = workerId.workerId;
    nameToWorkerId.emplace(name, workerId.workerId);
    return SM_OK;
}

void SmemTransEntry::AlignMemory(const void *&address, uint64_t &size)
{
    constexpr auto NPU_PAGE_SIZE = 2UL * 1024UL * 1024UL;
    constexpr auto NPU_PAGE_MASK = ~(NPU_PAGE_SIZE - 1UL);

    auto pointer = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
    auto alignPtr = (pointer & NPU_PAGE_MASK);
    auto diff = pointer - alignPtr;
    size += diff;
    size = ((size + NPU_PAGE_SIZE - 1) & NPU_PAGE_MASK);
    address = reinterpret_cast<const void *>(alignPtr);
}

std::vector<std::pair<const void *, size_t>>
SmemTransEntry::CombineMemories(std::vector<std::pair<const void *, size_t>> &input)
{
    std::sort(input.begin(), input.end());
    std::vector<std::pair<const void *, size_t>> result;
    auto current = input[0];
    for (auto i = 1U; i < input.size(); i++) {
        if ((const uint8_t *)current.first + current.second >= (const uint8_t *)input[i].first) {
            ptrdiff_t diff = ((const uint8_t *)input[i].first - (const uint8_t *)current.first);
            if (static_cast<size_t>(diff) > std::numeric_limits<size_t>::max() - input[i].second) {
                result.emplace_back(current);
                current = input[i];
                continue;
            }
            current.second = std::max(current.second, diff + input[i].second);
        } else {
            result.emplace_back(current);
            current = input[i];
        }
    }
    result.emplace_back(current);
    return result;
}

Result SmemTransEntry::RegisterOneMemory(const void *address, uint64_t size, uint32_t flags)
{
    auto slice = hybm_register_local_memory(entity_, address, size, 0);
    if (slice == nullptr) {
        SM_LOG_ERROR("register address with size: " << size << " failed!");
        return SM_ERROR;
    }
    SM_LOG_DEBUG("register memory(address with size=" << size << ") return slice=" << slice);

    hybm_exchange_info info;
    auto ret = hybm_export(entity_, slice, 0, &info);
    if (ret != 0) {
        SM_LOG_ERROR("export slice for register address with size: " << size << " failed:" << ret);
        hybm_free_local_memory(entity_, slice, size, 0);
        return SM_ERROR;
    }

    if (info.descLen != sliceInfoSize_) {
        SM_LOG_ERROR("export slice info size: " << info.descLen << " should be:" << sliceInfoSize_);
        hybm_free_local_memory(entity_, slice, size, 0);
        return SM_ERROR;
    }

    StoredSliceInfo sliceInfo(workerUniqueId_, address, size, rankId_);
    ret = storeHelper_.StoreSliceInfo(info, sliceInfo);
    if (ret != 0) {
        SM_LOG_ERROR("store for slice info failed: " << ret);
        return SM_ERROR;
    }

    return SM_OK;
}

hybm_options SmemTransEntry::GenerateHybmOptions()
{
    hybm_options options;
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.memType = HYBM_MEM_TYPE_DEVICE;
    options.bmScope = HYBM_SCOPE_CROSS_NODE;
    options.rankCount = 512U;
    options.rankId = rankId_;
    options.devId = config_.deviceId;
    options.deviceVASpace = 0;
    options.globalUniqueAddress = false;
    options.role = config_.role == SMEM_TRANS_SENDER ? HYBM_ROLE_SENDER : HYBM_ROLE_RECEIVER;
    options.transUrl[0] = '\0';
    uint16_t port = 11000 + entityId_;
    auto url = "tcp://127.0.0.1:" + std::to_string(port);

    constexpr size_t NIC_SIZE = sizeof(options.transUrl);
    size_t max_chars = std::min(url.length(), NIC_SIZE - 1);
    std::copy_n(url.c_str(), max_chars, options.transUrl);
    options.transUrl[max_chars] = '\0';
    options.tag[0] = '\0';
    options.tagOpInfo[0] = '\0';
    return std::move(options);
}

int32_t SmemTransEntry::ReInitialize()
{
    auto ret = storeHelper_.ReRegisterToServer(rankId_);
    SM_VALIDATE_RETURN(ret == SM_OK, "store helper recover failed: " << ret, ret);
    ret = storeHelper_.ReStoreDeviceInfo();
    SM_VALIDATE_RETURN(ret == SM_OK, "store device info recover failed: " << ret, ret);
    ret = storeHelper_.ReStoreSliceInfo();
    SM_VALIDATE_RETURN(ret == SM_OK, "store slice info recover failed: " << ret, ret);

    return SM_OK;
}
} // namespace smem
} // namespace ock