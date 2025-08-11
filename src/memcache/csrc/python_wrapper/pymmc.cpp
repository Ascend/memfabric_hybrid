/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include "pymmc.h"

#include <iostream>

#include "mmc_client.h"
#include "mmc_client_default.h"
#include "mmc.h"
#include "mmc_logger.h"
#include "mmc_types.h"
#include "smem_bm_def.h"

namespace py = pybind11;
using namespace ock::mmc;

constexpr int MAX_LAYER_NUM = 255;
constexpr int MAX_BATCH_SIZE = 512;

// ResourceTracker implementation using singleton pattern
ResourceTracker &ResourceTracker::getInstance() {
    static ResourceTracker instance;
    return instance;
}

ResourceTracker::ResourceTracker() {
    // Set up signal handlers
    struct sigaction sa{};
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Register for common termination signals
    sigaction(SIGINT, &sa, nullptr);   // Ctrl+C
    sigaction(SIGTERM, &sa, nullptr);  // kill command
    sigaction(SIGHUP, &sa, nullptr);   // Terminal closed

    // Register exit handler
    std::atexit(exitHandler);
}

ResourceTracker::~ResourceTracker() {
    // Cleanup is handled by exitHandler
}

void ResourceTracker::registerInstance(DistributedObjectStore *instance) {
    std::lock_guard<std::mutex> lock(mutex_);
    instances_.insert(instance);
}

void ResourceTracker::unregisterInstance(DistributedObjectStore *instance) {
    std::lock_guard<std::mutex> lock(mutex_);
    instances_.erase(instance);
}

void ResourceTracker::cleanupAllResources() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Perform cleanup outside the lock to avoid potential deadlocks
    for (void *instance : instances_) {
        auto *store = static_cast<DistributedObjectStore *>(instance);
        if (store) {
            std::cout << "Cleaning up DistributedObjectStore instance" << std::endl;
            store->tearDownAll();
        }
    }
}

void ResourceTracker::signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", cleaning up resources" << std::endl;
    getInstance().cleanupAllResources();

    // Re-raise the signal with default handler to allow normal termination
    struct sigaction sa{};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, nullptr);
    raise(signal);
}

void ResourceTracker::exitHandler() { getInstance().cleanupAllResources(); }

DistributedObjectStore::DistributedObjectStore() {
    // Register this instance with the global tracker
    // easylog::set_min_severity(easylog::Severity::WARN);
    ResourceTracker::getInstance().registerInstance(this);
}

DistributedObjectStore::~DistributedObjectStore() {
    // Unregister from the tracker before cleanup
    ResourceTracker::getInstance().unregisterInstance(this);
}

int DistributedObjectStore::init() {
    return mmc_init();
}

int DistributedObjectStore::setup(const std::string &local_hostname,
                                  const std::string &metadata_server,
                                  size_t global_segment_size,
                                  size_t local_buffer_size,
                                  const std::string &protocol,
                                  const std::string &rdma_devices,
                                  const std::string &master_server_addr) {
    return 0;
}

int DistributedObjectStore::initAll(const std::string &protocol,
                                    const std::string &device_name,
                                    size_t mount_segment_size) {
    return 0;
}

int DistributedObjectStore::tearDownAll() {
    mmc_uninit();
    return 0;
}

int DistributedObjectStore::put(const std::string &key, mmc_buffer &buffer) {
    mmc_put_options options{};
    options.policy = NATIVE_AFFINITY;
    return mmcc_put(key.c_str(), &buffer, options, 0);
}

int DistributedObjectStore::put_batch(
    const std::vector<std::string> &keys,
    const std::vector<mmc_buffer> &values) {
    return 0;
}

int DistributedObjectStore::put_parts(const std::string &key, std::vector<mmc_buffer> values) {
    return 0;
}

pybind11::bytes DistributedObjectStore::get(const std::string &key) {
    mmc_data_info info;
    auto res = mmcc_query(key.c_str(), &info, 0);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to query key " << key << ", error code: " << res);
        py::gil_scoped_acquire acquire_gil;
        return {"\0", 0};
    }

    const auto data_ptr = std::shared_ptr<char[]>(new char[info.size]);
    if (data_ptr == nullptr) {
        MMC_LOG_ERROR("Failed to allocate dynamic memory. ");
        py::gil_scoped_acquire acquire_gil;
        return {"\0", 0};
    }
    mmc_buffer buffer = {
        .addr = reinterpret_cast<uintptr_t>(data_ptr.get()),
        .type = 0,
        .dimType = 0,
        .oneDim = {
            .offset = 0,
            .len = info.size,
        }
    };

    res = mmcc_get(key.c_str(), &buffer, 0);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to get key " << key << ", error code: " << res);
        py::gil_scoped_acquire acquire_gil;
        return {"\0", 0};
    }

    py::gil_scoped_acquire acquire_gil;
    return {reinterpret_cast<const char *>(buffer.addr), buffer.oneDim.len};
}

std::vector<pybind11::bytes> DistributedObjectStore::get_batch(const std::vector<std::string> &keys) {
    const auto kNullString = pybind11::bytes("\0", 0);
    return {kNullString};
}

int DistributedObjectStore::remove(const std::string &key) {
    return mmcc_remove(key.c_str(), 0);  // 0 - success, other - not success
}

std::vector<int> DistributedObjectStore::removeBatch(const std::vector<std::string> &keys) {
    std::vector<int> results;

    if (keys.empty()) {
        MMC_LOG_ERROR("Empty keys vector provided to batchIsExist");
        return results;  // Return empty vector
    }

    results.resize(keys.size(), -1);
    const char** c_keys = new (std::nothrow) const char*[keys.size()];
    if (c_keys == nullptr) {
        MMC_LOG_ERROR("Cannot malloc memory for keys!");
        return results;  // Return vector filled with error code
    }
    for (size_t i = 0 ; i < keys.size(); ++i) {
        c_keys[i] = keys[i].c_str();
    }

    int32_t res = mmcc_batch_remove(c_keys, keys.size(), results.data(), 0);
    if (res != 0) {
        MMC_LOG_ERROR("remove_batch failed");
        std::fill(results.begin(), results.end(), res);
        delete[] c_keys;
        return results;  // Return vector filled with error code
    }

    delete[] c_keys;
    return results;
}

long DistributedObjectStore::removeAll() {
    return 0;
}

int DistributedObjectStore::isExist(const std::string &key) {
    int32_t res = mmcc_exist(key.c_str(), 0);
    if (res == MMC_OK) {
        // align with mooncake: 1 represents exist
        return 1;
    } else if (res == MMC_UNMATCHED_KEY) {
        // align with mooncake: 0 represents not exist
        return 0;
    }
    return res;
}

std::vector<int> DistributedObjectStore::batchIsExist(const std::vector<std::string> &keys) {
    std::vector<int> results;

    if (keys.empty()) {
        MMC_LOG_ERROR("Empty keys vector provided to batchIsExist");
        return results;  // Return empty vector
    }

    results.resize(keys.size(), -1);
    const char** c_keys = new (std::nothrow) const char*[keys.size()];
    if (c_keys == nullptr) {
        MMC_LOG_ERROR("Cannot malloc memory for keys!");
        return results;  // Return vector filled with error code -1
    }
    for (size_t i = 0 ; i < keys.size(); ++i) {
        c_keys[i] = keys[i].c_str();
    }

    int32_t res = mmcc_batch_exist(c_keys, keys.size(), results.data(), 0);
    if (res != 0) {
        MMC_LOG_ERROR("batch_exist failed");
        std::fill(results.begin(), results.end(), res);
        delete[] c_keys;
        return results;  // Return vector filled with error code
    }

    for (int &result : results) {
        if (result == MMC_OK) {
            // align with mooncake: 1 represents exist
            result = 1;
        } else if (result == MMC_UNMATCHED_KEY) {
            // align with mooncake: 0 represents not exist
            result = 0;
        }
    }

    delete[] c_keys;
    return results;
}

KeyInfo DistributedObjectStore::getKeyInfo(const std::string& key)
{
    mmc_data_info info;
    auto res = mmcc_query(key.c_str(), &info, 0);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to query key " << key << ", error code: " << res);
        return {0, 0};
    }

    if (!info.valid) {
        MMC_LOG_ERROR("Failed to query key " << key << ", info invalid");
        return {0, 0};
    }

    KeyInfo keyInfo{info.size, info.numBlobs};
    for (int i = 0; i < info.numBlobs; i++) {
        keyInfo.AddLoc(info.ranks[i]);
        keyInfo.AddType(info.types[i]);
    }
    return keyInfo;
}

std::vector<KeyInfo> DistributedObjectStore::batchGetKeyInfo(const std::vector<std::string>& keys)
{
    int32_t size = keys.size();
    if (size <= 0) {
        MMC_LOG_ERROR("batch to query keys num is " << size);
        return {};
    }

    const char** ckeys = new (std::nothrow) const char*[size];
    if (ckeys == nullptr) {
        MMC_LOG_ERROR("Cannot malloc memory for keys!");
        return {};
    }
    for (int i = 0 ; i < size; ++i) {
        ckeys[i] = keys[i].c_str();
    }

    mmc_data_info* infoArr = (mmc_data_info*)malloc(size * sizeof(mmc_data_info));
    if (infoArr == nullptr) {
        delete[] ckeys;
        MMC_LOG_ERROR("Cannot malloc memory for infos!");
        return {};
    }

    auto ret = mmcc_batch_query(ckeys, size, infoArr, 0);
    if (ret != MMC_OK) {
        delete[] ckeys;
        free(infoArr);
        MMC_LOG_ERROR("batch query failed! ret:" << ret);
        return {};
    }

    std::vector<KeyInfo> infoList{};
    for (int i = 0; i < size; ++i) {
        mmc_data_info& info = infoArr[i];
        if (!info.valid) {
            infoList.emplace_back(KeyInfo{0, 0});
            continue;
        }

        KeyInfo keyInfo{info.size, info.numBlobs};
        for (int j = 0; j < info.numBlobs; j++) {
            keyInfo.AddLoc(info.ranks[j]);
            keyInfo.AddType(info.types[j]);
        }
        infoList.emplace_back(keyInfo);
    }

    delete[] ckeys;
    free(infoArr);
    return infoList;
}

int64_t DistributedObjectStore::getSize(const std::string &key) {
    return 0;
}

// SliceBuffer implementation
SliceBuffer::SliceBuffer(DistributedObjectStore &store, void *buffer,
                         uint64_t size, bool use_allocator_free)
    : store_(store),
      buffer_(buffer),
      size_(size),
      use_allocator_free_(use_allocator_free) {}

SliceBuffer::~SliceBuffer() {
}

void *SliceBuffer::ptr() const { return buffer_; }

uint64_t SliceBuffer::size() const { return size_; }

// Implementation of get_buffer method
std::shared_ptr<SliceBuffer> DistributedObjectStore::get_buffer(const std::string &key) {
    return nullptr;
}

int DistributedObjectStore::register_buffer(void *buffer, size_t size) {
    return 0;
}

int DistributedObjectStore::get_into(const std::string &key, mmc_buffer &buffer) {
    mmc_data_info info;
    auto res = mmcc_query(key.c_str(), &info, 0);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to query key " << key << ", error code: " << res);
        py::gil_scoped_acquire acquire_gil;
        return res;
    }

    res = mmcc_get(key.c_str(), &buffer, 0);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to get key " << key << ", error code: " << res);
        py::gil_scoped_acquire acquire_gil;
        return res;
    }
    py::gil_scoped_acquire acquire_gil;
    return 0;
}

std::vector<int> DistributedObjectStore::batch_put_from(const std::vector<std::string> &keys,
                                                        const std::vector<void *> &buffers,
                                                        const std::vector<size_t> &sizes, const int32_t &direct)
{
    size_t count = keys.size();
    std::vector<int> results(count, -1);
    if (buffers.size() != count || sizes.size() != count) {
        MMC_LOG_ERROR("Input vector sizes mismatch: keys=" << keys.size()
                      << ", buffers=" << buffers.size()
                      << ", sizes=" << sizes.size());
        return results;
    }
    uint32_t type = 0;
    switch (direct) {
        case SMEMB_COPY_L2G:
            type = 1;
            break;
        case SMEMB_COPY_H2G:
            type = 0;
            break;
        default:
            throw std::invalid_argument("direct is invalid");
    }

    const char *keyArray[count];
    mmc_buffer bufferArray[count];
    for (size_t i = 0; i < count; ++i) {
        keyArray[i] = keys[i].c_str();
        bufferArray[i] = {
            .addr=reinterpret_cast<uint64_t>(buffers[i]),
            .type=type,
            .dimType=0,
            .oneDim={.offset=0, .len=static_cast<uint64_t>(sizes[i])}
        };
    }

    mmc_put_options options{};
    options.policy = NATIVE_AFFINITY;
    mmcc_batch_put(keyArray, count, bufferArray, options, 0, results.data());
    return results;
}

std::vector<int> DistributedObjectStore::batch_get_into(
    const std::vector<std::string> &keys, const std::vector<void *> &buffers,
    const std::vector<size_t> &sizes, const int32_t &direct) {
    size_t count = keys.size();
    std::vector<int> results(count, -1);
    if (buffers.size() != count || sizes.size() != count) {
        MMC_LOG_ERROR("Input vector sizes mismatch: keys=" << keys.size()
                      << ", buffers=" << buffers.size()
                      << ", sizes=" << sizes.size());
        return results;
    }
    uint32_t type = 0;
    switch (direct) {
        case SMEMB_COPY_G2L:
            type = 1;
            break;
        case SMEMB_COPY_G2H:
            type = 0;
            break;
        default:
            throw std::invalid_argument("direct is invalid");
    }

    const char *keyArray[count];
    mmc_buffer bufferArray[count];
    for (size_t i = 0; i < count; ++i) {
        keyArray[i] = keys[i].c_str();
        bufferArray[i] = {
            .addr=reinterpret_cast<uint64_t>(buffers[i]),
            .type=type,
            .dimType=0,
            .oneDim={.offset=0, .len=static_cast<uint64_t>(sizes[i])}
        };
    }

    mmcc_batch_get(keyArray, count, bufferArray, 0, results.data());
    return results;
}

int DistributedObjectStore::put_from(const std::string &key, mmc_buffer &buffer) {
    return put(key, buffer);
}

int DistributedObjectStore::put_from_layers(const std::string& key, const std::vector<uint8_t*>& buffers,
                                            const std::vector<size_t>& sizes, const int32_t& direct)
{
    uint32_t type;
    if (direct == SMEMB_COPY_L2G) {
        type = 1;
    } else if (direct == SMEMB_COPY_H2G) {
        type = 0;
    } else {
        MMC_LOG_ERROR("Invalid direct(" << direct << "), only 0 (SMEMB_COPY_L2G) and 3 (SMEMB_COPY_H2G) is supported");
        return MMC_INVALID_PARAM;
    }

    auto layerNum = buffers.size();
    if (layerNum == 0 || layerNum > MAX_LAYER_NUM) {
        MMC_LOG_ERROR("Layer number is 0 or exceeds the limit of " << MAX_LAYER_NUM);
        return MMC_INVALID_PARAM;
    }

    if (sizes.size() != layerNum) {
        MMC_LOG_ERROR("Unmatched number of layers and sizes");
        return MMC_INVALID_PARAM;
    }

    mmc_put_options options{};
    options.policy = NATIVE_AFFINITY;
    if (is2D(buffers, sizes)) {
        mmc_buffer buffer = {
            .addr=reinterpret_cast<uint64_t>(buffers[0]),
            .type=type,
            .dimType=1,
            .twoDim = {
                .dpitch = static_cast<uint64_t>(buffers[1] - buffers[0]),
                .layerOffset = 0,
                .width = static_cast<uint32_t>(sizes[0]),
                .layerNum = static_cast<uint16_t>(layerNum),
                .layerCount = static_cast<uint16_t>(layerNum)
            }
        };
        return mmcc_put(key.c_str(), &buffer, options, 0);
    } else {
        std::vector<mmc_buffer> mmc_buffers;
        for (size_t i = 0; i < layerNum; i += 1) {
            mmc_buffers.push_back({
                .addr=reinterpret_cast<uint64_t>(buffers[i]),
                .type=type,
                .dimType=0,
                .oneDim = {.offset=0, .len=static_cast<uint64_t>(sizes[i])}
            });
        }
        MmcBufferArray bufArr(mmc_buffers, type);
        return MmcClientDefault::GetInstance()->Put(key, bufArr, options, 0);
    }
}

std::vector<int> DistributedObjectStore::batch_put_from_layers(const std::vector<std::string>& keys,
                                                               const std::vector<std::vector<uint8_t*>>& buffers,
                                                               const std::vector<std::vector<size_t>>& sizes,
                                                               const int32_t& direct)
{
    const size_t batchSize = keys.size();
    std::vector<int> results(batchSize, MMC_INVALID_PARAM);

    uint32_t type;
    if (direct == SMEMB_COPY_L2G) {
        type = 1;
    } else if (direct == SMEMB_COPY_H2G) {
        type = 0;
    } else {
        MMC_LOG_ERROR("Invalid direct(" << direct << "), only 0 (SMEMB_COPY_L2G) and 3 (SMEMB_COPY_H2G) is supported");
        return results;
    }

    if (batchSize != buffers.size() || batchSize != sizes.size()) {
        MMC_LOG_ERROR("Input vector sizes mismatch: keys=" << keys.size()
                      << ", buffers=" << buffers.size()
                      << ", sizes=" << sizes.size());
        return results;
    }

    if (batchSize == 0 || batchSize > MAX_BATCH_SIZE) {
        MMC_LOG_ERROR("Batch size is 0 or exceeds the limit of " << MAX_BATCH_SIZE);
        return results;
    }

    bool all2D;
    auto res = CheckInputAndIsAll2D(batchSize, buffers, sizes, all2D);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to check if all layers are 2D");
        return results;
    }

    mmc_put_options options{};
    options.policy = NATIVE_AFFINITY;
    if (all2D) {
        std::vector<mmc_buffer> buffersIn2D;
        getBuffersIn2D(batchSize, type, buffers, sizes, buffersIn2D);
        MmcClientDefault::GetInstance()->BatchPut(keys, buffersIn2D, options, 0, results);
    } else {
        std::vector<MmcBufferArray> bufferArrays;
        getBufferArrays(batchSize, type, buffers, sizes, bufferArrays);
        MmcClientDefault::GetInstance()->BatchPut(keys, bufferArrays, options, 0, results);
    }

    return results;
}

int DistributedObjectStore::get_into_layers(const std::string& key, const std::vector<uint8_t*>& buffers,
                                            const std::vector<size_t>& sizes, const int32_t& direct)
{
    uint32_t type;
    if (direct == SMEMB_COPY_G2L) {
        type = 1;
    } else if (direct == SMEMB_COPY_G2H) {
        type = 0;
    } else {
        MMC_LOG_ERROR("Invalid direct(" << direct << "), only 1 (SMEMB_COPY_G2L) and 2 (SMEMB_COPY_G2H) is supported");
        return MMC_INVALID_PARAM;
    }

    auto layerNum = buffers.size();
    if (layerNum == 0 || layerNum > MAX_LAYER_NUM) {
        MMC_LOG_ERROR("Layer number is 0 or exceeds the limit of " << MAX_LAYER_NUM);
        return MMC_INVALID_PARAM;
    }

    if (sizes.size() != layerNum) {
        MMC_LOG_ERROR("Unmatched number of layers and sizes");
        return MMC_INVALID_PARAM;
    }

    if (is2D(buffers, sizes)) {
        mmc_buffer buffer = {
            .addr=reinterpret_cast<uint64_t>(buffers[0]),
            .type=type,
            .dimType=1,
            .twoDim = {
                .dpitch = static_cast<uint64_t>(buffers[1] - buffers[0]),
                .layerOffset = 0,
                .width = static_cast<uint32_t>(sizes[0]),
                .layerNum = static_cast<uint16_t>(layerNum),
                .layerCount = static_cast<uint16_t>(layerNum)
            }
        };
        return mmcc_get(key.c_str(), &buffer, 0);
    } else {
        std::vector<mmc_buffer> mmc_buffers;
        for (size_t i = 0; i < layerNum; i += 1) {
            mmc_buffers.push_back({
                .addr=reinterpret_cast<uint64_t>(buffers[i]),
                .type=type,
                .dimType=0,
                .oneDim = {.offset=0, .len=static_cast<uint64_t>(sizes[i])}
            });
        }
        MmcBufferArray bufArr(mmc_buffers, type);
        return MmcClientDefault::GetInstance()->Get(key, bufArr, 0);
    }
}

std::vector<int> DistributedObjectStore::batch_get_into_layers(const std::vector<std::string>& keys,
                                                               const std::vector<std::vector<uint8_t*>>& buffers,
                                                               const std::vector<std::vector<size_t>>& sizes,
                                                               const int32_t& direct)
{
    const size_t batchSize = keys.size();
    std::vector<int> results(batchSize, MMC_INVALID_PARAM);

    uint32_t type;
    if (direct == SMEMB_COPY_G2L) {
        type = 1;
    } else if (direct == SMEMB_COPY_G2H) {
        type = 0;
    } else {
        MMC_LOG_ERROR("Invalid direct(" << direct << "), only 1 (SMEMB_COPY_G2L) and 2 (SMEMB_COPY_G2H) is supported");
        return results;
    }

    if (batchSize != buffers.size() || batchSize != sizes.size()) {
        MMC_LOG_ERROR("Input vector sizes mismatch: keys=" << keys.size()
                      << ", buffers=" << buffers.size()
                      << ", sizes=" << sizes.size());
        return results;
    }

    if (batchSize == 0 || batchSize > MAX_BATCH_SIZE) {
        MMC_LOG_ERROR("Batch size (" << batchSize << ") is 0 or exceeds the limit of " << MAX_BATCH_SIZE);
        return results;
    }

    bool isAll2D;
    auto res = CheckInputAndIsAll2D(batchSize, buffers, sizes, isAll2D);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to check if all layers are 2D");
        return results;
    }

    if (isAll2D) {
        std::vector<mmc_buffer> buffersIn2D;
        getBuffersIn2D(batchSize, type, buffers, sizes, buffersIn2D);
        MmcClientDefault::GetInstance()->BatchGet(keys, buffersIn2D, 0, results);
    } else {
        std::vector<MmcBufferArray> bufferArrays;
        getBufferArrays(batchSize, type, buffers, sizes, bufferArrays);
        MmcClientDefault::GetInstance()->BatchGet(keys, bufferArrays, 0, results);
    }

    return results;
}

bool DistributedObjectStore::is2D(const std::vector<uint8_t*>& buffers, const std::vector<size_t>& sizes)
{
    const auto layerNum = buffers.size();
    if (layerNum < 2) {
        return false;
    }

    const auto interval = buffers[1] - buffers[0];
    for (size_t i = 2; i < layerNum; i+=1) {
        if (buffers[i] - buffers[i - 1] != interval) {
            return false;
        }
    }

    const auto size = sizes[0];
    for (size_t i = 1; i < layerNum; i+=1) {
        if (sizes[i] != size) {
            return false;
        }
    }

    return true;
}

int DistributedObjectStore::CheckInputAndIsAll2D(const size_t batchSize,
                                                 const std::vector<std::vector<uint8_t*>>& buffers,
                                                 const std::vector<std::vector<size_t>>& sizes,
                                                 bool& result)
{
    for (size_t i = 0; i < batchSize; i += 1) {
        const auto layerNum = buffers[i].size();
        if (layerNum == 0 || layerNum > MAX_LAYER_NUM) {
            MMC_LOG_ERROR("Layer number is 0 or exceeds the limit of " << MAX_LAYER_NUM);
            return MMC_INVALID_PARAM;
        }
        if (sizes[i].size() != layerNum) {
            MMC_LOG_ERROR("Unmatched number of layers and sizes");
            return MMC_INVALID_PARAM;
        }
    }

    result = true;
    for (size_t i = 0; i < batchSize; i += 1) {
        if (!is2D(buffers[i], sizes[i])) {
            result = false;
            break;
        }
    }
    return MMC_OK;
}

void DistributedObjectStore::getBuffersIn2D(const size_t batchSize, const uint32_t type,
                                            const std::vector<std::vector<uint8_t*>>& bufferLists,
                                            const std::vector<std::vector<size_t>>& sizeLists,
                                            std::vector<mmc_buffer>& buffersIn2D)
{
    for (size_t i = 0; i < batchSize; i += 1) {
        const auto& buffers = bufferLists[i];
        const auto& sizes = sizeLists[i];
        const auto layerNum = buffers.size();
        buffersIn2D.push_back({
            .addr = reinterpret_cast<uint64_t>(buffers[0]),
            .type = type,
            .dimType = 1,
            .twoDim = {
                .dpitch = static_cast<uint64_t>(buffers[1] - buffers[0]),
                .layerOffset = 0,
                .width = static_cast<uint32_t>(sizes[0]),
                .layerNum = static_cast<uint16_t>(layerNum),
                .layerCount = static_cast<uint16_t>(layerNum)
            }
        });
    }
}

void DistributedObjectStore::getBufferArrays(const size_t batchSize, const uint32_t type,
                                             const std::vector<std::vector<uint8_t*>>& bufferLists,
                                             const std::vector<std::vector<size_t>>& sizeLists,
                                             std::vector<MmcBufferArray>& bufferArrays)
{
    for (size_t i = 0; i < batchSize; i += 1) {
        const auto& buffers = bufferLists[i];
        const auto& sizes = sizeLists[i];
        const auto layerNum = buffers.size();

        std::vector<mmc_buffer> mmc_buffers;
        for (size_t l = 0; l < layerNum; l+=1) {
            mmc_buffers.push_back({
                .addr=reinterpret_cast<uint64_t>(buffers[l]),
                .type=type,
                .dimType=0,
                .oneDim = {.offset=0, .len=sizes[l]}
            });
        }
        MmcBufferArray bufArr(mmc_buffers, type);
        bufferArrays.push_back(bufArr);
    }
}

void DefineMmcStructModule(py::module_& m)
{
    py::enum_<smem_bm_copy_type>(m, "MmcCopyDirect")
            .value("SMEMB_COPY_L2G", SMEMB_COPY_L2G)
            .value("SMEMB_COPY_G2L", SMEMB_COPY_G2L)
            .value("SMEMB_COPY_G2H", SMEMB_COPY_G2H)
            .value("SMEMB_COPY_H2G", SMEMB_COPY_H2G);

    // Define the SliceBuffer class
    py::class_<KeyInfo, std::shared_ptr<KeyInfo>>(m, "KeyInfo", py::buffer_protocol())
        .def("size", &KeyInfo::Size)
        .def("loc_list", &KeyInfo::GetLocs)
        .def("type_list", &KeyInfo::GetTypes)
        .def("__str__", &KeyInfo::ToString)
        .def("__repr__", &KeyInfo::ToString);
}

PYBIND11_MODULE(_pymmc, m) {
    DefineMmcStructModule(m);
    // Define the SliceBuffer class
    py::class_<SliceBuffer, std::shared_ptr<SliceBuffer>>(m, "SliceBuffer",
                                                          py::buffer_protocol())
        .def("ptr",
             [](const SliceBuffer &self) {
                 // Return the pointer as an integer for Python
                 return reinterpret_cast<uintptr_t>(self.ptr());
             })
        .def("size", &SliceBuffer::size)
        .def("__len__", &SliceBuffer::size)
        .def_buffer([](SliceBuffer &self) -> py::buffer_info {
            // SliceBuffer now always contains contiguous memory
            if (self.size() > 0) {
                return py::buffer_info(
                    self.ptr(),   /* Pointer to buffer */
                    sizeof(char), /* Size of one scalar */
                    py::format_descriptor<
                        char>::format(),   /* Python struct-style
                                              format descriptor */
                    1,                     /* Number of dimensions */
                    {(size_t)self.size()}, /* Buffer dimensions */
                    {sizeof(char)} /* Strides (in bytes) for each index */
                );
            } else {
                // Empty buffer
                return py::buffer_info(
                    nullptr,      /* Pointer to buffer */
                    sizeof(char), /* Size of one scalar */
                    py::format_descriptor<
                        char>::format(), /* Python struct-style
                                            format descriptor */
                    1,                   /* Number of dimensions */
                    {0},                 /* Buffer dimensions */
                    {sizeof(char)}       /* Strides (in bytes) for each index */
                );
            }
        });

    // Define the DistributedObjectStore class
    py::class_<DistributedObjectStore>(m, "DistributedObjectStore")
        .def(py::init<>())
        .def("init", &DistributedObjectStore::init)
        .def("setup", &DistributedObjectStore::setup)
        .def("init_all", &DistributedObjectStore::initAll)
        .def("get", &DistributedObjectStore::get)
        .def("get_batch", &DistributedObjectStore::get_batch)
        .def("get_buffer", &DistributedObjectStore::get_buffer,
             py::call_guard<py::gil_scoped_release>(),
             py::return_value_policy::take_ownership)
        .def("remove", &DistributedObjectStore::remove,
             py::call_guard<py::gil_scoped_release>())
        .def("remove_batch", &DistributedObjectStore::removeBatch,
             py::call_guard<py::gil_scoped_release>())
        .def("remove_all", &DistributedObjectStore::removeAll,
             py::call_guard<py::gil_scoped_release>())
        .def("is_exist", &DistributedObjectStore::isExist,
             py::call_guard<py::gil_scoped_release>())
        .def("batch_is_exist", &DistributedObjectStore::batchIsExist,
             py::call_guard<py::gil_scoped_release>(), py::arg("keys"),
             "Check if multiple objects exist. Returns list of results: 1 if "
             "exists, 0 if not exists, -1 if error")
        .def("get_key_info", &DistributedObjectStore::getKeyInfo,
             py::call_guard<py::gil_scoped_release>())
        .def("batch_get_key_info", &DistributedObjectStore::batchGetKeyInfo,
             py::call_guard<py::gil_scoped_release>(), py::arg("keys"))
        .def("close", &DistributedObjectStore::tearDownAll)
        .def("get_size", &DistributedObjectStore::getSize,
             py::call_guard<py::gil_scoped_release>())
        .def(
            "register_buffer",
            [](DistributedObjectStore &self, uintptr_t buffer_ptr,
               size_t size) {
                // Register memory buffer for RDMA operations
                void *buffer = reinterpret_cast<void *>(buffer_ptr);
                py::gil_scoped_release release;
                return self.register_buffer(buffer, size);
            },
            py::arg("buffer_ptr"), py::arg("size"),
            "Register a memory buffer for direct access operations")
        .def(
            "get_into",
            [](DistributedObjectStore &self, const std::string &key,
                uintptr_t buffer_ptr, size_t size, const int32_t &direct) {
                uint32_t type = 0;
                switch (direct) {
                    case SMEMB_COPY_G2L:
                        type = 1;
                        break;
                    case SMEMB_COPY_G2H:
                        type = 0;
                        break;
                    default:
                        throw std::invalid_argument("direct is invalid");
                }
                mmc_buffer buffer = {
                    .addr=buffer_ptr, \
                    .type=type,
                    .dimType=0,
                    .oneDim={.offset=0, .len=size}
                };
                return self.get_into(key, buffer);
            },
            py::arg("key"), py::arg("buffer_ptr"), py::arg("size"), py::arg("direct") = SMEMB_COPY_G2H,
            "Get object data directly into a pre-allocated buffer")
        .def(
            "batch_get_into",
            [](DistributedObjectStore &self,
               const std::vector<std::string> &keys,
               const std::vector<uintptr_t> &buffer_ptrs,
               const std::vector<size_t> &sizes, const int32_t &direct) {
                std::vector<void *> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (uintptr_t ptr : buffer_ptrs) {
                    buffers.push_back(reinterpret_cast<void *>(ptr));
                }
                py::gil_scoped_release release;
                return self.batch_get_into(keys, buffers, sizes, direct);
            },
            py::arg("keys"), py::arg("buffer_ptrs"), py::arg("sizes"), py::arg("direct") = SMEMB_COPY_G2H,
            "Get object data directly into pre-allocated buffers for multiple "
            "keys")
        .def(
            "get_into_layers",
            [](DistributedObjectStore &self,
               const std::string &key,
               const std::vector<uintptr_t> &buffer_ptrs,
               const std::vector<size_t> &sizes, const int32_t &direct) {
                std::vector<uint8_t*> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (uintptr_t ptr : buffer_ptrs) {
                    buffers.push_back(reinterpret_cast<uint8_t*>(ptr));
                }
                py::gil_scoped_release release;
                return self.get_into_layers(key, buffers, sizes, direct);
            },
            py::arg("key"), py::arg("buffer_ptrs"), py::arg("sizes"), py::arg("direct") = SMEMB_COPY_G2H)
        .def(
            "batch_get_into_layers",
            [](DistributedObjectStore &self,
               const std::vector<std::string> &keys,
               const std::vector<std::vector<uintptr_t>> &buffer_ptrs,
               const std::vector<std::vector<size_t>> &sizes, const int32_t &direct) {
                std::vector<std::vector<uint8_t*>> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (auto vec : buffer_ptrs) {
                    std::vector<uint8_t*> tmp;
                    for (uintptr_t ptr : vec) {
                        tmp.push_back(reinterpret_cast<uint8_t*>(ptr));
                    }
                    buffers.push_back(tmp);
                }
                py::gil_scoped_release release;
                return self.batch_get_into_layers(keys, buffers, sizes, direct);
            },
            py::arg("keys"), py::arg("buffer_ptrs"), py::arg("sizes"), py::arg("direct") = SMEMB_COPY_G2H)
        .def(
            "put_from",
            [](DistributedObjectStore &self, const std::string &key,
               uintptr_t buffer_ptr, size_t size, const int32_t &direct) {
                uint32_t type = 0;
                switch (direct) {
                    case SMEMB_COPY_L2G:
                        type = 1;
                        break;
                    case SMEMB_COPY_H2G:
                        type = 0;
                        break;
                    default:
                        throw std::invalid_argument("direct is invalid");
                }
                mmc_buffer buffer = {
                    .addr=buffer_ptr, \
                    .type=type,
                    .dimType=0,
                    .oneDim={.offset=0, .len=size}
                };
                py::gil_scoped_release release;
                return self.put_from(key, buffer);
            },
            py::arg("key"), py::arg("buffer_ptr"), py::arg("size"), py::arg("direct") = SMEMB_COPY_H2G,
            "Put object data directly from a pre-allocated buffer")
        .def(
            "batch_put_from",
            [](DistributedObjectStore &self,
               const std::vector<std::string> &keys,
               const std::vector<uintptr_t> &buffer_ptrs,
               const std::vector<size_t> &sizes, const int32_t &direct) {
                std::vector<void *> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (uintptr_t ptr : buffer_ptrs) {
                    buffers.push_back(reinterpret_cast<void *>(ptr));
                }
                py::gil_scoped_release release;
                return self.batch_put_from(keys, buffers, sizes, direct);
            },
            py::arg("keys"), py::arg("buffer_ptrs"), py::arg("sizes"), py::arg("direct") = SMEMB_COPY_H2G,
            "Put object data directly from pre-allocated buffers for multiple "
            "keys")
        .def(
            "put_from_layers",
            [](DistributedObjectStore &self,
               const std::string &key,
               const std::vector<uintptr_t> &buffer_ptrs,
               const std::vector<size_t> &sizes, const int32_t &direct) {
                std::vector<uint8_t*> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (uintptr_t ptr : buffer_ptrs) {
                    buffers.push_back(reinterpret_cast<uint8_t*>(ptr));
                }
                py::gil_scoped_release release;
                return self.put_from_layers(key, buffers, sizes, direct);
            },
            py::arg("key"), py::arg("buffer_ptrs"), py::arg("sizes"), py::arg("direct") = SMEMB_COPY_H2G)
        .def(
            "batch_put_from_layers",
            [](DistributedObjectStore &self,
               const std::vector<std::string> &keys,
               const std::vector<std::vector<uintptr_t>> &buffer_ptrs,
               const std::vector<std::vector<size_t>> &sizes, const int32_t &direct) {
                std::vector<std::vector<uint8_t*>> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (auto vec : buffer_ptrs) {
                    std::vector<uint8_t*> tmp;
                    for (uintptr_t ptr : vec) {
                        tmp.push_back(reinterpret_cast<uint8_t*>(ptr));
                    }
                    buffers.push_back(tmp);
                }
                py::gil_scoped_release release;
                return self.batch_put_from_layers(keys, buffers, sizes, direct);
            },
            py::arg("keys"), py::arg("buffer_ptrs"), py::arg("sizes"), py::arg("direct") = SMEMB_COPY_H2G)
        .def("put",
            [](DistributedObjectStore &self, const std::string &key, const py::buffer &buf) {
                py::buffer_info info = buf.request(/*writable=*/false);
                mmc_buffer buffer = {
                    .addr=reinterpret_cast<uint64_t>(info.ptr), \
                    .type=0,
                    .dimType=0,
                    .oneDim={.offset=0, .len=static_cast<uint64_t>(info.size)}
                };
                py::gil_scoped_release release;
                return self.put(key, buffer);
            })
        .def("put_parts",
            [](DistributedObjectStore &self, const std::string &key, const py::args& parts) {
                // 1) Python buffer → mmc_buffer
                std::vector<py::buffer_info> infos;
                std::vector<mmc_buffer> buffers;
                infos.reserve(parts.size());
                buffers.reserve(parts.size());

                for (auto &obj : parts) {
                    py::buffer buf = py::reinterpret_borrow<py::buffer>(obj);
                    infos.emplace_back(buf.request(false));
                    const auto &info = infos.back();
                    if (info.ndim != 1 || info.itemsize != 1)
                        throw std::runtime_error("parts must be 1-D bytes-like");

                    buffers.emplace_back(mmc_buffer{
                        .addr=reinterpret_cast<uint64_t>(info.ptr),
                        .type=0,
                        .dimType=0,
                        .oneDim={.offset=0, .len=static_cast<uint64_t>(info.size)}});
                }

                // 2) Call C++ function
                py::gil_scoped_release unlock;
                return self.put_parts(key, buffers);
            })
        .def(
            "put_batch",
            [](DistributedObjectStore &self, const std::vector<std::string> &keys, const std::vector<py::bytes> &py_values) {
                std::vector<std::string> temp_values;
                temp_values.reserve(py_values.size());
                for (const auto &value : py_values) {
                    temp_values.emplace_back(value.cast<std::string>());
                }

                std::vector<mmc_buffer> buffers;
                buffers.reserve(temp_values.size());
                for (const auto &s : temp_values) {
                    buffers.emplace_back(mmc_buffer{
                        .addr=reinterpret_cast<uintptr_t>(s.data()),
                        .type=0,
                        .dimType=0,
                        .oneDim={.offset=0, .len=s.size()}});
                }

                return self.put_batch(keys, buffers);
            },
            py::arg("keys"), py::arg("values"));
}
