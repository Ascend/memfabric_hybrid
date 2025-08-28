/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
#include "smem_trans_entry.h"
#include "smem_trans_entry_manager.h"

namespace ock {
namespace smem {
static const std::string SENDER_COUNT_KEY = "count_for_senders";
static const std::string SENDER_DEVICE_INFO_KEY = "devices_info_for_senders";
static const std::string RECEIVER_COUNT_KEY = "receiver_for_senders";
static const std::string RECEIVER_DEVICE_INFO_KEY = "devices_info_for_receivers";
static const std::string RECEIVER_TOTAL_SLICE_COUNT_KEY = "receivers_total_slices_count";
static const std::string RECEIVER_SLICES_INFO_KEY = "receivers_all_slices_info";

static const std::string SENDER_TOTAL_SLICE_COUNT_KEY = "senders_total_slices_count";
static const std::string SENDER_SLICES_INFO_KEY = "senders_all_slices_info";

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
    entityId_ = (16U << 3) + 1U;
    if (!ParseTransName(name_, workerSession_.address, workerSession_.port)) {
        return SM_INVALID_PARAM;
    }

    auto ret = storeHelper_.Initialize(entityId_, static_cast<int32_t>(config.initTimeout));
    SM_VALIDATE_RETURN(ret == SM_OK, "store helper initialize failed: " << ret, ret);

    ret = storeHelper_.GenerateRankId(config, rankId_);
    SM_VALIDATE_RETURN(ret == SM_OK, "store helper generate rankId failed: " << ret, ret);

    config_ = config;
    auto options = GenerateHybmOptions();
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

    StartWatchThread();
    return SM_OK;
}

void SmemTransEntry::UnInitialize()
{
    std::unique_lock<std::mutex> locker{watchMutex_};
    watchRunning_ = false;
    locker.unlock();
    watchCond_.notify_one();

    if (watchThread_.joinable()) {
        try {
            watchThread_.join();
        } catch (const std::system_error &e) {
            SM_LOG_ERROR("watch thread join failed: " << e.what());
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

Result SmemTransEntry::SyncWrite(const void *srcAddress, const std::string &remoteName, void *destAddress,
                                 size_t dataSize)
{
    return SyncWrite(&srcAddress, remoteName, &destAddress, &dataSize, 1U);
}

Result SmemTransEntry::SyncWrite(const void *srcAddresses[], const std::string &remoteName, void *destAddresses[],
                                 const size_t dataSizes[], uint32_t batchSize)
{
    uint64_t session;
    auto ret = ParseNameToUniqueId(remoteName, session);
    if (ret != 0) {
        return ret;
    }

    std::vector<void *> mappedAddress(batchSize);

    ReadGuard locker(remoteSliceRwMutex_);
    auto it = remoteSlices_.find(session);
    if (it == remoteSlices_.end()) {
        SM_LOG_ERROR("session:(" << remoteName << ")(" << session << ") not found.");
        return SM_INVALID_PARAM;
    }

    for (auto i = 0U; i < batchSize; i++) {
        auto pos = it->second.lower_bound(destAddresses[i]);
        if (pos == it->second.end()) {
            SM_LOG_ERROR("address[" << i << "] is invalid.");
            return SM_INVALID_PARAM;
        }

        if ((const uint8_t *)destAddresses[i] + dataSizes[i] > (const uint8_t *)(pos->first) + pos->second.size) {
            SM_LOG_ERROR("address[" << i << "], size[" << i << "]=" << dataSizes[i] << " out of range.");
            return SM_INVALID_PARAM;
        }

        mappedAddress[i] =
            (uint8_t *)pos->second.address + ((const uint8_t *)destAddresses[i] - (const uint8_t *)(pos->first));
    }

    hybm_batch_copy_params copyParams = {srcAddresses, mappedAddress.data(), dataSizes, batchSize};
    ret = hybm_data_batch_copy(entity_, &copyParams,
                               HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    if (ret != 0) {
        SM_LOG_ERROR("batch copy data failed:" << ret);
        return ret;
    }

    return SM_OK;
}

bool SmemTransEntry::ParseTransName(const std::string &name, uint32_t &ip, uint16_t &port)
{
    UrlExtraction extraction;
    auto ret = extraction.ExtractIpPortFromUrl(std::string("tcp://").append(name));
    if (ret != 0) {
        SM_LOG_ERROR("parse name failed: " << ret);
        return false;
    }

    in_addr addr;
    ret = inet_aton(extraction.ip.c_str(), &addr);
    if (ret != 1) {
        SM_LOG_ERROR("parse name failed: " << ret);
        return false;
    }

    ip = ntohl(addr.s_addr);
    port = extraction.port;
    return true;
}

Result SmemTransEntry::StartWatchThread()
{
    SM_LOG_DEBUG("start background thread");
    watchThread_ = std::thread([this]() {
        std::unique_lock<std::mutex> locker{watchMutex_};
        while (watchRunning_) {
            WatchTaskOneLoop();
            watchCond_.wait_for(locker, std::chrono::seconds(3));
        }
    });
    return 0;
}

void SmemTransEntry::WatchTaskOneLoop()
{
    static int64_t times = 0;
    WatchTaskFindNewRanks();

    if (times >= 2) {
        WatchTaskFindNewSlices();
    }
    times++;
}

void SmemTransEntry::WatchTaskFindNewRanks()
{
    auto importNewRanks = [this](const std::vector<hybm_exchange_info> &info) {
        auto ret = hybm_import(entity_, info.data(), info.size(), nullptr, 0);
        if (ret != 0) {
            SM_LOG_ERROR("import new ranks failed: " << ret);
        }
        return ret;
    };
    storeHelper_.FindNewRemoteRanks(importNewRanks);
}

void SmemTransEntry::WatchTaskFindNewSlices()
{
    auto importNewSlices = [this](const std::vector<hybm_exchange_info> &info,
                                  const std::vector<const StoredSliceInfo *> &ss) {
        std::vector<void *> addresses(info.size());
        auto ret = hybm_import(entity_, info.data(), info.size(), addresses.data(), 0);
        if (ret != 0) {
            SM_LOG_ERROR("import new slices failed: " << ret);
            return ret;
        }
        SM_LOG_DEBUG("import slices count=" << info.size());

        WriteGuard locker(remoteSliceRwMutex_);
        for (auto i = 0U; i < info.size(); i++) {
            WorkerIdUnion workerId{ss[i]->session};
            SM_LOG_DEBUG("add remote slice for : " << workerId.workerId);
            remoteSlices_[workerId.workerId].emplace(ss[i]->address, LocalMapAddress{addresses[i], ss[i]->size});
        }
        return 0;
    };
    storeHelper_.FindNewRemoteSlices(importNewSlices);
}

Result SmemTransEntry::ParseNameToUniqueId(const std::string &name, uint64_t &uniqueId)
{
    WorkerSession workerSession;
    auto it = nameToWorkerId.find(name);
    if (it != nameToWorkerId.end()) {
        /* fast path */
        uniqueId = it->second;
        return SM_OK;
    }
    auto success = ParseTransName(name, workerSession.address, workerSession.port);
    if (!success) {
        SM_LOG_ERROR("parse name failed.");
        return -1;
    }

    WorkerIdUnion workerId{workerSession};
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

std::vector<std::pair<const void *, size_t>> SmemTransEntry::CombineMemories(
    std::vector<std::pair<const void *, size_t>> &input)
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
    auto slice = hybm_register_local_memory(entity_, HYBM_MEM_TYPE_DEVICE, address, size, 0);
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
    SM_LOG_DEBUG("export slice=" << slice << " success");

    if (info.descLen != sliceInfoSize_) {
        SM_LOG_ERROR("export slice info size: " << info.descLen << " should be:" << sliceInfoSize_);
        hybm_free_local_memory(entity_, slice, size, 0);
        return SM_ERROR;
    }

    StoredSliceInfo sliceInfo(workerSession_, address, size);
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
    options.bmType = HYBM_TYPE_HBM_HOST_INITIATE;
    options.bmDataOpType = HYBM_DOP_TYPE_DEVICE_RDMA;
    options.bmScope = HYBM_SCOPE_CROSS_NODE;
    options.bmRankType = HYBM_RANK_TYPE_STATIC;
    options.rankCount = 512U;
    options.rankId = rankId_;
    options.devId = config_.deviceId;
    options.singleRankVASpace = 0;
    options.preferredGVA = 0;
    options.globalUniqueAddress = false;
    options.role = config_.role == SMEM_TRANS_SENDER ? HYBM_ROLE_SENDER : HYBM_ROLE_RECEIVER;
    options.nic[0] = '\0';
    uint16_t port = 11000 + entityId_;
    auto url = std::string("tcp://0.0.0.0/0:").append(std::to_string(port));
    strcpy(options.nic, url.c_str());
    return std::move(options);
}

}
}