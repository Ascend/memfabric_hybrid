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

struct ReceiverSliceInfo {
    WorkerSession session;
    const void *address;
    uint64_t size;
    uint8_t info[0];

    ReceiverSliceInfo(WorkerSession ws, const void *a, uint64_t s) noexcept
        : session(std::move(ws)),
          address{a},
          size{s}
    {
    }
};

SmemTransEntryPtr SmemTransEntry::Create(const std::string &name, const std::string &storeUrl,
                                         const smem_trans_config_t &config)
{
    /* create entry and initialize */
    SmemTransEntryPtr transEntry;
    auto result = SmemTransEntryManager::Instance().CreateEntryByName(name, transEntry);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("create trans entry failed, probably out of memory");
        return nullptr;
    }

    /* initialize */
    result = transEntry->Initialize(storeUrl, config);
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

int32_t SmemTransEntry::Initialize(const std::string &storeUrl, const smem_trans_config_t &config)
{
    /* create store */
    UrlExtraction option;
    uint16_t entityId = (16U << 3) + 1U;
    if (option.ExtractIpPortFromUrl(storeUrl) != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("invalid store url. ");
        return SM_INVALID_PARAM;
    }

    if (!ParseTransName(name_, workerSession_.address, workerSession_.port)) {
        return SM_INVALID_PARAM;
    }

    config_ = config;
    auto tmpStore =
        StoreFactory::CreateStore(option.ip, option.port, false, 0, static_cast<int32_t>(config.initTimeout));
    if (tmpStore == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("create store client with url failed.");
        return SM_NEW_OBJECT_FAILED;
    }

    storeUrlExtraction_ = option;
    /* init hybm entity */
    hybm_options options;
    options.bmType = HYBM_TYPE_HBM_HOST_INITIATE;
    options.bmDataOpType = HYBM_DOP_TYPE_SDMA;
    options.bmScope = HYBM_SCOPE_CROSS_NODE;
    options.bmRankType = HYBM_RANK_TYPE_STATIC;
    options.rankCount = 1;
    options.rankId = 0;
    options.devId = config_.deviceId;
    options.singleRankVASpace = 0;
    options.preferredGVA = 0;
    options.globalUniqueAddress = false;
    entity_ = hybm_create_entity(entityId, &options, 0);
    if (entity_ == nullptr) {
        SM_LOG_ERROR("create new entity failed.");
        return SM_ERROR;
    }

    auto ret = hybm_export(entity_, nullptr, 0, &deviceInfo_);
    if (ret != 0) {
        SM_LOG_ERROR("HybmExport device info failed: " << ret);
        return SM_ERROR;
    }

    size_t outputSize;
    ret = hybm_export_slice_size(entity_, &outputSize);
    if (ret != 0) {
        SM_LOG_ERROR("HybmExportSliceSize failed: " << ret);
        return SM_ERROR;
    }

    sliceInfoSize_ = static_cast<uint32_t>(outputSize);
    store_ = StoreFactory::PrefixStore(tmpStore, std::string("/trans/").append(std::to_string(entityId).append("/")));
    SM_ASSERT_RETURN(store_ != nullptr, SM_ERROR);
    ret = StoreDeviceInfo();
    if (ret != SM_OK) {
        return ret;
    }

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
        } catch (const std::system_error& e) {
            SM_LOG_ERROR("watch thread join failed: " << e.what());
        }
    }
    StoreFactory::DestroyStore(storeUrlExtraction_.ip, storeUrlExtraction_.port);
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

    if (config_.role != SMEM_TRANS_RECEIVER && config_.role != SMEM_TRANS_BOTH) {
        SM_LOG_INFO("sender side skip register memory.");
        return SM_OK;
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

    if (config_.role != SMEM_TRANS_RECEIVER && config_.role != SMEM_TRANS_BOTH) {
        SM_LOG_INFO("sender side skip register memory.");
        return SM_OK;
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
    auto ret = ParseNameToSessionId(remoteName, session);
    if (ret != 0) {
        return ret;
    }

    std::vector<void *> mappedAddress(batchSize);

    ReadGuard locker(remoteSliceRwMutex_);
    auto it = remoteSlices_.find(session);
    if (it == remoteSlices_.end()) {
        SM_LOG_ERROR("session not found.");
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

    for (auto i = 0U; i < batchSize; i++) {
        hybm_copy_params copyParams = {srcAddresses[i], mappedAddress[i], dataSizes[i]};
        ret = hybm_data_copy(entity_, &copyParams, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
        if (ret != 0) {
            SM_LOG_ERROR("copy data failed:" << ret);
            return ret;
        }
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
    if (config_.role == SMEM_TRANS_RECEIVER || config_.role == SMEM_TRANS_BOTH) {
        WatchTaskFindNewSenders();
    }

    if (config_.role == SMEM_TRANS_SENDER || config_.role == SMEM_TRANS_BOTH) {
        if (times >= 2) {
            WatchTaskFindNewSlices();
        }
    }
    times++;
}

void SmemTransEntry::WatchTaskFindNewSenders()
{
    int64_t totalValue = 0;
    auto ret = store_->Add(SENDER_COUNT_KEY, 0L, totalValue);
    if (ret != 0) {
        SM_LOG_ERROR("store add(0) sender count failed: " << ret);
        return;
    }

    if (totalValue > sendersLastTime_) {
        SM_LOG_DEBUG("find new sender workers from " << sendersLastTime_ << " to " << totalValue);

        std::vector<uint8_t> values;
        ret = store_->Get(SENDER_DEVICE_INFO_KEY, values);
        if (ret != 0) {
            SM_LOG_ERROR("store get devices info for sender failed: " << ret);
            return;
        }

        auto increment = static_cast<uint32_t>(totalValue - sendersLastTime_);
        std::vector<hybm_exchange_info> info(increment);
        for (auto i = 0U; i < increment; i++) {
            std::copy_n(values.data() + (sendersLastTime_ + i) * deviceInfo_.descLen, deviceInfo_.descLen,
                        info[i].desc);
            info[i].descLen = deviceInfo_.descLen;
        }

        ret = hybm_import(entity_, info.data(), increment, nullptr, 0);
        if (ret != 0) {
            SM_LOG_ERROR("import sender info failed count from " << sendersLastTime_ << " to " << totalValue);
            return;
        }

        sendersLastTime_ = totalValue;
    }
}

void SmemTransEntry::WatchTaskFindNewSlices()
{
    int64_t totalValue = 0;
    auto ret = store_->Add(RECEIVER_TOTAL_SLICE_COUNT_KEY, 0L, totalValue);
    if (ret != 0) {
        SM_LOG_ERROR("store add(0) receivers slices total count failed: " << ret);
        return;
    }

    if (totalValue <= slicesLastTime_) {
        return;
    }
    SM_LOG_DEBUG("find new slices from " << slicesLastTime_ << " to " << totalValue);

    std::vector<uint8_t> values;
    ret = store_->Get(RECEIVER_SLICES_INFO_KEY, values);
    if (ret != 0) {
        SM_LOG_ERROR("store get receivers all slices failed: " << ret);
        return;
    }

    auto increment = static_cast<uint32_t>(totalValue - slicesLastTime_);
    std::vector<hybm_exchange_info> info(increment);
    std::vector<void *> addresses(increment);
    std::vector<const ReceiverSliceInfo *> recvSs(increment);
    auto itemOffsetBytes = (sizeof(ReceiverSliceInfo) + sliceInfoSize_) * static_cast<uint64_t>(slicesLastTime_);
    if (itemOffsetBytes + increment * (sizeof(ReceiverSliceInfo) + sliceInfoSize_) > values.size()) {
        SM_LOG_ERROR("Buffer overflow detected in RECEIVER_SLICES_INFO_KEY");
        return;
    }
    for (auto i = 0U; i < increment; i++) {
        recvSs[i] = (const ReceiverSliceInfo *)(const void *)(values.data() + itemOffsetBytes);
        std::copy_n(values.data() + itemOffsetBytes + sizeof(ReceiverSliceInfo), sliceInfoSize_, info[i].desc);
        info[i].descLen = sliceInfoSize_;
        itemOffsetBytes += (sizeof(ReceiverSliceInfo) + sliceInfoSize_);
    }

    ret = hybm_import(entity_, info.data(), increment, addresses.data(), 0);
    if (ret != 0) {
        SM_LOG_ERROR("import sender info failed count from " << slicesLastTime_ << " to " << totalValue);
        return;
    }
    SM_LOG_DEBUG("import slices count=" << info.size());

    WriteGuard locker(remoteSliceRwMutex_);
    for (auto i = 0U; i < increment; i++) {
        WorkerIdUnion workerId{recvSs[i]->session};
        remoteSlices_[workerId.workerId].emplace(recvSs[i]->address, LocalMapAddress{addresses[i], recvSs[i]->size});
    }

    slicesLastTime_ = totalValue;
}

Result SmemTransEntry::StoreDeviceInfo()
{
    int64_t totalValue = 0;
    uint64_t totalSize = 0;
    std::vector<uint8_t> value(deviceInfo_.desc, deviceInfo_.desc + deviceInfo_.descLen);
    if (config_.role == SMEM_TRANS_SENDER || config_.role == SMEM_TRANS_BOTH) {
        auto ret = store_->Append(SENDER_DEVICE_INFO_KEY, value, totalSize);
        if (ret != 0) {
            SM_LOG_ERROR("store append device info for sender failed: " << ret);
            return SM_ERROR;
        }

        ret = store_->Add(SENDER_COUNT_KEY, 1L, totalValue);
        if (ret != 0) {
            SM_LOG_ERROR("store add sender count failed: " << ret);
            return SM_ERROR;
        }
    }

    if (config_.role == SMEM_TRANS_RECEIVER || config_.role == SMEM_TRANS_BOTH) {
        auto ret = store_->Append(RECEIVER_DEVICE_INFO_KEY, value, totalSize);
        if (ret != 0) {
            SM_LOG_ERROR("store append device info for receiver failed: " << ret);
            return SM_ERROR;
        }

        ret = store_->Add(RECEIVER_COUNT_KEY, 1L, totalValue);
        if (ret != 0) {
            SM_LOG_ERROR("store add sender count failed: " << ret);
            return SM_ERROR;
        }
    }

    return SM_OK;
}

Result SmemTransEntry::ParseNameToSessionId(const std::string &name, uint64_t &session)
{
    WorkerSession workerSession;
    auto success = ParseTransName(name, workerSession.address, workerSession.port);
    if (!success) {
        SM_LOG_ERROR("parse name failed.");
        return -1;
    }

    WorkerIdUnion workerId{workerSession};
    session = workerId.workerId;
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
    address = reinterpret_cast<const void*>(alignPtr);
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

    ReceiverSliceInfo sliceInfo(workerSession_, address, size);
    std::vector<uint8_t> value(sizeof(sliceInfo) + sliceInfoSize_);
    std::copy_n(reinterpret_cast<const uint8_t*>(&sliceInfo), sizeof(sliceInfo), value.data());
    std::copy_n(info.desc, sliceInfoSize_, value.data() + sizeof(sliceInfo));

    uint64_t totalSize = 0;
    SM_LOG_DEBUG("begin append(key=" << RECEIVER_SLICES_INFO_KEY << ", value_size=" << value.size() << ")");
    ret = store_->Append(RECEIVER_SLICES_INFO_KEY, value, totalSize);
    if (ret != 0) {
        SM_LOG_ERROR("store append slice info failed: " << ret);
        hybm_free_local_memory(entity_, slice, size, 0);
        return SM_ERROR;
    }

    SM_LOG_DEBUG("success append(key=" << RECEIVER_SLICES_INFO_KEY << ", value_size=" << value.size()
                                       << "), total_size=" << totalSize);
    int64_t nowCount = 0;
    ret = store_->Add(RECEIVER_TOTAL_SLICE_COUNT_KEY, 1L, nowCount);
    if (ret != 0) {
        SM_LOG_ERROR("store add count for slice info failed: " << ret);
        hybm_free_local_memory(entity_, slice, size, 0);
        return SM_ERROR;
    }

    SM_LOG_DEBUG("now slice total count = " << nowCount);
    return SM_OK;
}

}
}