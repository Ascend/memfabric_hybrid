/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <iostream>

#include "mmc_client.h"
#include "mmc_client_default.h"
#include "mmc.h"
#include "mmc_logger.h"
#include "mmc_types.h"
#include "mmc_ptracer.h"
#include "mmc_meta_service_process.h"
#include "smem_bm_def.h"
#include "mmcache_store.h"

namespace ock {
namespace mmc {

constexpr int MAX_LAYER_NUM = 255;
constexpr int MAX_BATCH_SIZE = 512;
constexpr int MAX_KEY_LEN = 256;

// ResourceTracker implementation using singleton pattern
ResourceTracker &ResourceTracker::getInstance()
{
    static ResourceTracker instance;
    return instance;
}

ResourceTracker::ResourceTracker()
{
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

ResourceTracker::~ResourceTracker()
{
    // Cleanup is handled by exitHandler
}

void ResourceTracker::registerInstance(MmcacheStore *instance)
{
    std::lock_guard<std::mutex> lock(mutex_);
    instances_.insert(instance);
}

void ResourceTracker::unregisterInstance(MmcacheStore *instance)
{
    std::lock_guard<std::mutex> lock(mutex_);
    instances_.erase(instance);
}

void ResourceTracker::cleanupAllResources()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Perform cleanup outside the lock to avoid potential deadlocks
    for (void *instance : instances_) {
        auto *store = static_cast<MmcacheStore *>(instance);
        if (store) {
            std::cout << "Cleaning up MmcacheStore instance" << std::endl;
            store->TearDown();
        }
    }
}

void ResourceTracker::signalHandler(int signal)
{
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

void ResourceTracker::exitHandler()
{
    getInstance().cleanupAllResources();
}

MmcacheStore::MmcacheStore()
{
    // Register this instance with the global tracker
    ResourceTracker::getInstance().registerInstance(this);
}

MmcacheStore::~MmcacheStore()
{
    // Unregister from the tracker before cleanup
    ResourceTracker::getInstance().unregisterInstance(this);
}

ObjectStore::~ObjectStore() {}

std::shared_ptr<ObjectStore> ObjectStore::CreateObjectStore()
{
    return std::make_shared<MmcacheStore>();
}

int MmcacheStore::Init(const uint32_t deviceId)
{
    mmc_init_config config{deviceId};
    return mmc_init(config);
}

int MmcacheStore::TearDown()
{
    mmc_uninit();
    return 0;
}

int MmcacheStore::RegisterBuffer(void *buffer, size_t size)
{
    return mmcc_register_buffer(reinterpret_cast<uint64_t>(buffer), size);
}

int MmcacheStore::GetInto(const std::string &key, void *buffer, size_t size, const int32_t direct)
{
    uint32_t type = 0;
    switch (direct) {
        case SMEMB_COPY_G2L: type = 1; break;
        case SMEMB_COPY_G2H: type = 0; break;
        default: MMC_LOG_ERROR("Failed to get by type " << direct << " for key " << key); return -1;
    }
    mmc_buffer mmcBuffer = {
        .addr = reinterpret_cast<uint64_t>(buffer), .type = type, .dimType = 0, .oneDim = {.offset = 0, .len = size}};
    TP_TRACE_BEGIN(TP_MMC_PY_GET);
    auto res = mmcc_get(key.c_str(), &mmcBuffer, 0);
    TP_TRACE_END(TP_MMC_PY_GET, res);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to get key " << key << ", error code: " << res);
    }
    return res;
}

int MmcacheStore::PutFrom(const std::string &key, void *buffer, size_t size, const int32_t direct)
{
    uint32_t type = 0;
    switch (direct) {
        case SMEMB_COPY_G2L: type = 1; break;
        case SMEMB_COPY_G2H: type = 0; break;
        default: MMC_LOG_ERROR("Failed to put by type " << direct << " for key " << key); return -1;
    }
    mmc_buffer mmcBuffer = {
        .addr = reinterpret_cast<uint64_t>(buffer), .type = type, .dimType = 0, .oneDim = {.offset = 0, .len = size}};

    mmc_put_options options{};
    options.mediaType = 0;  // will set by client proxy
    options.policy = NATIVE_AFFINITY;
    TP_TRACE_BEGIN(TP_MMC_PY_PUT);
    const auto res = mmcc_put(key.c_str(), &mmcBuffer, options, 0);
    auto ret = ReturnWrapper(res, key);
    TP_TRACE_END(TP_MMC_PY_PUT, ret);
    return ret;
}

int MmcacheStore::Remove(const std::string &key)
{
    TP_TRACE_BEGIN(TP_MMC_PY_REMOVE);
    auto ret = mmcc_remove(key.c_str(), 0);  // 0 - success, other - not success
    TP_TRACE_END(TP_MMC_PY_REMOVE, ret);
    return ret;
}

std::vector<int> MmcacheStore::BatchRemove(const std::vector<std::string> &keys)
{
    std::vector<int> results;

    if (keys.empty()) {
        MMC_LOG_ERROR("Empty keys vector provided to batchIsExist");
        return results;  // Return empty vector
    }

    results.resize(keys.size(), -1);
    const char **c_keys = new (std::nothrow) const char *[keys.size()];
    if (c_keys == nullptr) {
        MMC_LOG_ERROR("Cannot malloc memory for keys!");
        return results;  // Return vector filled with error code
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        c_keys[i] = keys[i].c_str();
    }
    TP_TRACE_BEGIN(TP_MMC_PY_BATCH_REMOVE);
    int32_t res = mmcc_batch_remove(c_keys, keys.size(), results.data(), 0);
    TP_TRACE_END(TP_MMC_PY_BATCH_REMOVE, res);
    if (res != 0) {
        MMC_LOG_ERROR("remove_batch failed");
        std::fill(results.begin(), results.end(), res);
        delete[] c_keys;
        return results;  // Return vector filled with error code
    }

    delete[] c_keys;
    return results;
}

int MmcacheStore::IsExist(const std::string &key)
{
    TP_TRACE_BEGIN(TP_MMC_PY_EXIST);
    int32_t res = mmcc_exist(key.c_str(), 0);
    TP_TRACE_END(TP_MMC_PY_EXIST, res);
    if (res == MMC_OK) {
        // align with mooncake: 1 represents exist
        return 1;
    } else if (res == MMC_UNMATCHED_KEY) {
        // align with mooncake: 0 represents not exist
        return 0;
    }
    return res;
}

std::vector<int> MmcacheStore::BatchIsExist(const std::vector<std::string> &keys)
{
    std::vector<int> results;

    if (keys.empty()) {
        MMC_LOG_ERROR("Empty keys vector provided to batchIsExist");
        return results;  // Return empty vector
    }

    results.resize(keys.size(), -1);
    const char **c_keys = new (std::nothrow) const char *[keys.size()];
    if (c_keys == nullptr) {
        MMC_LOG_ERROR("Cannot malloc memory for keys!");
        return results;  // Return vector filled with error code -1
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        c_keys[i] = keys[i].c_str();
    }
    TP_TRACE_BEGIN(TP_MMC_PY_BATCH_EXIST);
    int32_t res = mmcc_batch_exist(c_keys, keys.size(), results.data(), 0);
    TP_TRACE_END(TP_MMC_PY_BATCH_EXIST, res);
    if (res != 0) {
        MMC_LOG_ERROR("batch_exist failed:" << res);
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

KeyInfo MmcacheStore::GetKeyInfo(const std::string &key)
{
    mmc_data_info info;
    TP_TRACE_BEGIN(TP_MMC_PY_QUERY);
    auto res = mmcc_query(key.c_str(), &info, 0);
    TP_TRACE_END(TP_MMC_PY_QUERY, res);
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

std::vector<KeyInfo> MmcacheStore::BatchGetKeyInfo(const std::vector<std::string> &keys)
{
    int32_t size = keys.size();
    if (size <= 0) {
        MMC_LOG_ERROR("batch to query keys num is " << size);
        return {};
    }

    const char **ckeys = new (std::nothrow) const char *[size];
    if (ckeys == nullptr) {
        MMC_LOG_ERROR("Cannot malloc memory for keys!");
        return {};
    }
    for (int i = 0; i < size; ++i) {
        ckeys[i] = keys[i].c_str();
    }

    mmc_data_info *infoArr = (mmc_data_info *)malloc(size * sizeof(mmc_data_info));
    if (infoArr == nullptr) {
        delete[] ckeys;
        MMC_LOG_ERROR("Cannot malloc memory for infos!");
        return {};
    }
    TP_TRACE_BEGIN(TP_MMC_PY_BATCH_QUERY);
    auto ret = mmcc_batch_query(ckeys, size, infoArr, 0);
    TP_TRACE_END(TP_MMC_PY_BATCH_QUERY, ret);
    if (ret != MMC_OK) {
        delete[] ckeys;
        free(infoArr);
        MMC_LOG_ERROR("batch query failed! ret:" << ret);
        return {};
    }

    std::vector<KeyInfo> infoList{};
    for (int i = 0; i < size; ++i) {
        mmc_data_info &info = infoArr[i];
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

std::vector<int> MmcacheStore::BatchPutFrom(const std::vector<std::string> &keys, const std::vector<void *> &buffers,
                                            const std::vector<size_t> &sizes, const int32_t direct)
{
    const size_t count = keys.size();
    std::vector<int> results(count, -1);
    if (buffers.size() != count || sizes.size() != count) {
        MMC_LOG_ERROR("Input vector sizes mismatch: keys=" << keys.size() << ", buffers=" << buffers.size()
                                                           << ", sizes=" << sizes.size());
        return results;
    }
    uint32_t type = 0;
    switch (direct) {
        case SMEMB_COPY_L2G: type = 1; break;
        case SMEMB_COPY_H2G: type = 0; break;
        default: MMC_LOG_ERROR("Failed to batch put by type " << direct); return results;
    }

    const char *keyArray[count];
    mmc_buffer bufferArray[count];
    for (size_t i = 0; i < count; ++i) {
        keyArray[i] = keys[i].c_str();
        bufferArray[i] = {.addr = reinterpret_cast<uint64_t>(buffers[i]),
                          .type = type,
                          .dimType = 0,
                          .oneDim = {.offset = 0, .len = static_cast<uint64_t>(sizes[i])}};
    }

    mmc_put_options options{};
    options.mediaType = 0;
    options.policy = NATIVE_AFFINITY;
    TP_TRACE_BEGIN(TP_MMC_PY_BATCH_PUT);
    mmcc_batch_put(keyArray, count, bufferArray, options, 0, results.data());
    TP_TRACE_END(TP_MMC_PY_BATCH_PUT, 0);
    for (size_t i = 0; i < count; i++) {
        results[i] = ReturnWrapper(results[i], keys[i]);
    }
    return results;
}

std::vector<int> MmcacheStore::BatchGetInto(const std::vector<std::string> &keys, const std::vector<void *> &buffers,
                                            const std::vector<size_t> &sizes, const int32_t direct)
{
    size_t count = keys.size();
    std::vector<int> results(count, -1);
    if (buffers.size() != count || sizes.size() != count) {
        MMC_LOG_ERROR("Input vector sizes mismatch: keys=" << keys.size() << ", buffers=" << buffers.size()
                                                           << ", sizes=" << sizes.size());
        return results;
    }
    uint32_t type = 0;
    switch (direct) {
        case SMEMB_COPY_G2L: type = 1; break;
        case SMEMB_COPY_G2H: type = 0; break;
        default: MMC_LOG_ERROR("Failed to batch get by type " << direct); return results;
    }

    const char *keyArray[count];
    mmc_buffer bufferArray[count];
    for (size_t i = 0; i < count; ++i) {
        keyArray[i] = keys[i].c_str();
        bufferArray[i] = {.addr = reinterpret_cast<uint64_t>(buffers[i]),
                          .type = type,
                          .dimType = 0,
                          .oneDim = {.offset = 0, .len = static_cast<uint64_t>(sizes[i])}};
    }
    TP_TRACE_BEGIN(TP_MMC_PY_BATCH_GET);
    auto ret = mmcc_batch_get(keyArray, count, bufferArray, 0, results.data());
    TP_TRACE_END(TP_MMC_PY_BATCH_GET, ret);
    return results;
}

int MmcacheStore::PutFromLayers(const std::string &key, const std::vector<void *> &buffers,
                                const std::vector<size_t> &sizes, const int32_t direct)
{
    if (direct != SMEMB_COPY_L2G && direct != SMEMB_COPY_H2G) {
        MMC_LOG_ERROR("Invalid direct(" << direct << "), only 0 (SMEMB_COPY_L2G) and 3 (SMEMB_COPY_H2G) is supported");
        return MMC_INVALID_PARAM;
    }
    uint32_t type = (direct == SMEMB_COPY_L2G ? 1 : 0);

    if (key.length() == 0 || key.length() > MAX_KEY_LEN) {
        MMC_LOG_ERROR("Invalid param, key's len (" << key.length() << ") is not between 1 and " << MAX_KEY_LEN);
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
    Result res;
    if (Is2D(buffers, sizes)) {
        mmc_buffer buffer = {.addr = reinterpret_cast<uint64_t>(buffers[0]),
                             .type = type,
                             .dimType = 1,
                             .twoDim = {.dpitch = reinterpret_cast<uint64_t>(buffers[1]) - reinterpret_cast<uint64_t>(buffers[0]),
                                        .layerOffset = 0,
                                        .width = static_cast<uint32_t>(sizes[0]),
                                        .layerNum = static_cast<uint16_t>(layerNum),
                                        .layerCount = static_cast<uint16_t>(layerNum)}};
        TP_TRACE_BEGIN(TP_MMC_PY_PUT_LAYERS_2D);
        res = mmcc_put(key.c_str(), &buffer, options, 0);
        TP_TRACE_END(TP_MMC_PY_PUT_LAYERS_2D, res);
    } else {
        MmcBufferArray bufArr;
        for (size_t i = 0; i < layerNum; i += 1) {
            bufArr.AddBuffer({.addr = reinterpret_cast<uint64_t>(buffers[i]),
                              .type = type,
                              .dimType = 0,
                              .oneDim = {.offset = 0, .len = static_cast<uint64_t>(sizes[i])}});
        }
        TP_TRACE_BEGIN(TP_MMC_PY_PUT_LAYERS);
        res = MmcClientDefault::GetInstance()->Put(key, bufArr, options, 0);
        TP_TRACE_END(TP_MMC_PY_PUT_LAYERS, res);
    }

    return ReturnWrapper(res, key);
}

std::vector<int> MmcacheStore::BatchPutFromLayers(const std::vector<std::string> &keys,
                                                  const std::vector<std::vector<void *>> &buffers,
                                                  const std::vector<std::vector<size_t>> &sizes, const int32_t direct)
{
    const size_t batchSize = keys.size();
    std::vector<int> results(batchSize, MMC_INVALID_PARAM);

    if (direct != SMEMB_COPY_L2G && direct != SMEMB_COPY_H2G) {
        MMC_LOG_ERROR("Invalid direct(" << direct << "), only 0 (SMEMB_COPY_L2G) and 3 (SMEMB_COPY_H2G) is supported");
        return results;
    }
    uint32_t type = (direct == SMEMB_COPY_L2G ? 1 : 0);

    if (batchSize != buffers.size() || batchSize != sizes.size()) {
        MMC_LOG_ERROR("Input vector sizes mismatch: keys=" << keys.size() << ", buffers=" << buffers.size()
                                                           << ", sizes=" << sizes.size());
        return results;
    }

    if (batchSize == 0 || batchSize > MAX_BATCH_SIZE) {
        MMC_LOG_ERROR("Batch size is 0 or exceeds the limit of " << MAX_BATCH_SIZE);
        return results;
    }

    for (const std::string &key : keys) {
        if (key.length() == 0 || key.length() > MAX_KEY_LEN) {
            MMC_LOG_ERROR("Invalid param, key's len (" << key.length() << ") is not between 1 and " << MAX_KEY_LEN);
            return results;
        }
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
        TP_TRACE_BEGIN(TP_MMC_PY_BATCH_PUT_LAYERS_2D);
        std::vector<mmc_buffer> buffersIn2D;
        GetBuffersIn2D(batchSize, type, buffers, sizes, buffersIn2D);
        auto ret = MmcClientDefault::GetInstance()->BatchPut(keys, buffersIn2D, options, 0, results);
        TP_TRACE_END(TP_MMC_PY_BATCH_PUT_LAYERS_2D, ret);
    } else {
        TP_TRACE_BEGIN(TP_MMC_PY_BATCH_PUT_LAYERS);
        std::vector<MmcBufferArray> bufferArrays;
        GetBufferArrays(batchSize, type, buffers, sizes, bufferArrays);
        auto ret = MmcClientDefault::GetInstance()->BatchPut(keys, bufferArrays, options, 0, results);
        TP_TRACE_END(TP_MMC_PY_BATCH_PUT_LAYERS, ret);
    }

    for (size_t i = 0; i < batchSize; i++) {
        results[i] = ReturnWrapper(results[i], keys[i]);
    }

    return results;
}

int MmcacheStore::GetIntoLayers(const std::string &key, const std::vector<void *> &buffers,
                                const std::vector<size_t> &sizes, const int32_t direct)
{
    if (direct != SMEMB_COPY_G2L && direct != SMEMB_COPY_G2H) {
        MMC_LOG_ERROR("Invalid direct(" << direct << "), only 1 (SMEMB_COPY_G2L) and 2 (SMEMB_COPY_G2H) is supported");
        return MMC_INVALID_PARAM;
    }
    uint32_t type = (direct == SMEMB_COPY_G2L ? 1 : 0);

    if (key.length() == 0 || key.length() > MAX_KEY_LEN) {
        MMC_LOG_ERROR("Invalid param, key's len (" << key.length() << ") is not between 1 and " << MAX_KEY_LEN);
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

    if (Is2D(buffers, sizes)) {
        mmc_buffer buffer = {.addr = reinterpret_cast<uint64_t>(buffers[0]),
                             .type = type,
                             .dimType = 1,
                             .twoDim = {.dpitch = reinterpret_cast<uint64_t>(buffers[1]) - reinterpret_cast<uint64_t>(buffers[0]),
                                        .layerOffset = 0,
                                        .width = static_cast<uint32_t>(sizes[0]),
                                        .layerNum = static_cast<uint16_t>(layerNum),
                                        .layerCount = static_cast<uint16_t>(layerNum)}};
        TP_TRACE_BEGIN(TP_MMC_PY_GET_LAYERS_2D);
        auto ret = mmcc_get(key.c_str(), &buffer, 0);
        TP_TRACE_END(TP_MMC_PY_GET_LAYERS_2D, ret);
        return ret;
    } else {
        std::vector<mmc_buffer> mmc_buffers;
        for (size_t i = 0; i < layerNum; i += 1) {
            mmc_buffers.push_back({.addr = reinterpret_cast<uint64_t>(buffers[i]),
                                   .type = type,
                                   .dimType = 0,
                                   .oneDim = {.offset = 0, .len = static_cast<uint64_t>(sizes[i])}});
        }
        MmcBufferArray bufArr(mmc_buffers);
        TP_TRACE_BEGIN(TP_MMC_PY_GET_LAYERS);
        auto ret = MmcClientDefault::GetInstance()->Get(key, bufArr, 0);
        TP_TRACE_END(TP_MMC_PY_GET_LAYERS, ret);
        return ret;
    }
}

std::vector<int> MmcacheStore::BatchGetIntoLayers(const std::vector<std::string> &keys,
                                                  const std::vector<std::vector<void *>> &buffers,
                                                  const std::vector<std::vector<size_t>> &sizes, const int32_t direct)
{
    const size_t batchSize = keys.size();
    std::vector<int> results(batchSize, MMC_INVALID_PARAM);

    if (direct != SMEMB_COPY_G2L && direct != SMEMB_COPY_G2H) {
        MMC_LOG_ERROR("Invalid direct(" << direct << "), only 1 (SMEMB_COPY_G2L) and 2 (SMEMB_COPY_G2H) is supported");
        return results;
    }
    uint32_t type = (direct == SMEMB_COPY_G2L ? 1 : 0);

    if (batchSize != buffers.size() || batchSize != sizes.size()) {
        MMC_LOG_ERROR("Input vector sizes mismatch: keys=" << keys.size() << ", buffers=" << buffers.size()
                                                           << ", sizes=" << sizes.size());
        return results;
    }

    if (batchSize == 0 || batchSize > MAX_BATCH_SIZE) {
        MMC_LOG_ERROR("Batch size (" << batchSize << ") is 0 or exceeds the limit of " << MAX_BATCH_SIZE);
        return results;
    }

    for (const std::string &key : keys) {
        if (key.length() == 0 || key.length() > MAX_KEY_LEN) {
            MMC_LOG_ERROR("Invalid param, key's len (" << key.length() << ") is not between 1 and " << MAX_KEY_LEN);
            return results;
        }
    }

    bool isAll2D;
    auto res = CheckInputAndIsAll2D(batchSize, buffers, sizes, isAll2D);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to check if all layers are 2D");
        return results;
    }

    if (isAll2D) {
        TP_TRACE_BEGIN(TP_MMC_PY_BATCH_GET_LAYERS_2D);
        std::vector<mmc_buffer> buffersIn2D;
        GetBuffersIn2D(batchSize, type, buffers, sizes, buffersIn2D);
        auto ret = MmcClientDefault::GetInstance()->BatchGet(keys, buffersIn2D, 0, results);
        TP_TRACE_END(TP_MMC_PY_BATCH_GET_LAYERS_2D, ret);
    } else {
        TP_TRACE_BEGIN(TP_MMC_PY_BATCH_GET_LAYERS);
        std::vector<MmcBufferArray> bufferArrays;
        GetBufferArrays(batchSize, type, buffers, sizes, bufferArrays);
        auto ret = MmcClientDefault::GetInstance()->BatchGet(keys, bufferArrays, 0, results);
        TP_TRACE_END(TP_MMC_PY_BATCH_GET_LAYERS, ret);
    }

    return results;
}

bool MmcacheStore::Is2D(const std::vector<void *> &buffers, const std::vector<size_t> &sizes)
{
    const auto layerNum = buffers.size();
    if (layerNum < 2) {
        return false;
    }

    const auto interval = reinterpret_cast<uint64_t>(buffers[1]) - reinterpret_cast<uint64_t>(buffers[0]);
    for (size_t i = 2; i < layerNum; i += 1) {
        if (reinterpret_cast<uint64_t>(buffers[i]) - reinterpret_cast<uint64_t>(buffers[i - 1]) != interval) {
            return false;
        }
    }

    const auto size = sizes[0];
    for (size_t i = 1; i < layerNum; i += 1) {
        if (sizes[i] != size) {
            return false;
        }
    }

    return true;
}

int MmcacheStore::CheckInputAndIsAll2D(const size_t batchSize, const std::vector<std::vector<void *>> &buffers,
                                       const std::vector<std::vector<size_t>> &sizes, bool &result)
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
        if (!Is2D(buffers[i], sizes[i])) {
            result = false;
            break;
        }
    }
    return MMC_OK;
}

void MmcacheStore::GetBuffersIn2D(const size_t batchSize, const uint32_t type,
                                  const std::vector<std::vector<void *>> &bufferLists,
                                  const std::vector<std::vector<size_t>> &sizeLists,
                                  std::vector<mmc_buffer> &buffersIn2D)
{
    for (size_t i = 0; i < batchSize; i += 1) {
        const auto &buffers = bufferLists[i];
        const auto &sizes = sizeLists[i];
        const auto layerNum = buffers.size();
        buffersIn2D.push_back({.addr = reinterpret_cast<uint64_t>(buffers[0]),
                               .type = type,
                               .dimType = 1,
                               .twoDim = {.dpitch = reinterpret_cast<uint64_t>(buffers[1]) - reinterpret_cast<uint64_t>(buffers[0]),
                                          .layerOffset = 0,
                                          .width = static_cast<uint32_t>(sizes[0]),
                                          .layerNum = static_cast<uint16_t>(layerNum),
                                          .layerCount = static_cast<uint16_t>(layerNum)}});
    }
}

void MmcacheStore::GetBufferArrays(const size_t batchSize, const uint32_t type,
                                   const std::vector<std::vector<void *>> &bufferLists,
                                   const std::vector<std::vector<size_t>> &sizeLists,
                                   std::vector<MmcBufferArray> &bufferArrays)
{
    for (size_t i = 0; i < batchSize; i += 1) {
        const auto &buffers = bufferLists[i];
        const auto &sizes = sizeLists[i];
        const auto layerNum = buffers.size();

        std::vector<mmc_buffer> mmc_buffers;
        for (size_t l = 0; l < layerNum; l += 1) {
            mmc_buffers.push_back({.addr = reinterpret_cast<uint64_t>(buffers[l]),
                                   .type = type,
                                   .dimType = 0,
                                   .oneDim = {.offset = 0, .len = sizes[l]}});
        }
        MmcBufferArray bufArr(mmc_buffers);
        bufferArrays.push_back(bufArr);
    }
}

int MmcacheStore::ReturnWrapper(const int result, const std::string &key)
{
    if (result != MMC_OK) {
        if (result == MMC_DUPLICATED_OBJECT) {
            MMC_LOG_DEBUG("Duplicated key " << key << ", put operation skipped");
            return MMC_OK;
        } else {
            MMC_LOG_ERROR("Failed to put key " << key << ", error code=" << result);
            return result;
        }
    }
    return MMC_OK;
}

int MmcacheStore::Put(const std::string &key, mmc_buffer &buffer)
{
    mmc_put_options options{};
    options.mediaType = 0;  // will set by client proxy
    options.policy = NATIVE_AFFINITY;
    TP_TRACE_BEGIN(TP_MMC_PY_PUT);
    const auto res = mmcc_put(key.c_str(), &buffer, options, 0);
    auto ret = ReturnWrapper(res, key);
    TP_TRACE_END(TP_MMC_PY_PUT, ret);
    return ret;
}

mmc_buffer MmcacheStore::Get(const std::string &key)
{
    mmc_data_info info;
    auto res = mmcc_query(key.c_str(), &info, 0);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to query key " << key << ", error code: " << res);
        return {};
    }

    const auto dataPtr = new char[info.size];
    if (dataPtr == nullptr) {
        MMC_LOG_ERROR("Failed to allocate dynamic memory. ");
        return {};
    }
    mmc_buffer buffer = {.addr = reinterpret_cast<uintptr_t>(dataPtr),
                         .type = 0,
                         .dimType = 0,
                         .oneDim = {
                             .offset = 0,
                             .len = info.size,
                         }};

    res = mmcc_get(key.c_str(), &buffer, 0);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to get key " << key << ", error code: " << res);
        delete[] dataPtr;
        return {};
    }

    return buffer;
}

}  // namespace mmc
}  // namespace ock