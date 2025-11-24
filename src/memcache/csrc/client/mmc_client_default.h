/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_CLIENT_DEFAULT_H
#define MEM_FABRIC_MMC_CLIENT_DEFAULT_H

#include "mmc_common_includes.h"
#include "mmc_meta_net_client.h"
#include "mmc_def.h"
#include "mmc_bm_proxy.h"
#include "mmc_thread_pool.h"
#include "mmc_msg_client_meta.h"

namespace ock {
namespace mmc {
class MmcClientDefault : public MmcReferable {
public:
    ~MmcClientDefault() override = default;

    Result Start(const mmc_client_config_t &config);

    void Stop();

    const std::string &Name() const;

    Result Put(const char *key, mmc_buffer *buf, mmc_put_options &options, uint32_t flags);

    Result Get(const char *key, mmc_buffer *buf, uint32_t flags);

    Result Put(const std::string &key, const MmcBufferArray& bufArr, mmc_put_options &options, uint32_t flags);

    Result Get(const std::string &key, const MmcBufferArray& bufArr, uint32_t flags);

    Result BatchPut(const std::vector<std::string>& keys, const std::vector<mmc_buffer>& bufs, mmc_put_options& options,
                    uint32_t flags, std::vector<int>& batchResult);

    Result BatchGet(const std::vector<std::string>& keys, std::vector<mmc_buffer>& bufs, uint32_t flags,
                    std::vector<int>& batchResult);

    Result BatchPut(const std::vector<std::string>& keys, const std::vector<MmcBufferArray>& bufArrs,
                    mmc_put_options& options, uint32_t flags, std::vector<int>& batchResult);

    Result BatchGet(const std::vector<std::string>& keys, const std::vector<MmcBufferArray>& bufArrs,
                    uint32_t flags, std::vector<int>& batchResult);

    Result Remove(const char *key, uint32_t flags) const;

    Result BatchRemove(const std::vector<std::string>& keys, std::vector<Result>& remove_results, uint32_t flags) const;

    Result IsExist(const std::string &key, uint32_t flags) const;

    Result BatchIsExist(const std::vector<std::string> &keys, std::vector<int32_t> &exist_results,
                        uint32_t flags) const;

    Result Query(const std::string &key, mmc_data_info &query_info, uint32_t flags) const;

    Result BatchQuery(const std::vector<std::string> &keys, std::vector<mmc_data_info> &query_infos,
                      uint32_t flags) const;

    Result RegisterBuffer(uint64_t addr, uint64_t size);

    uint32_t RankId() const
    {
        return rankId_;
    }

    static Result RegisterInstance()
    {
        std::lock_guard<std::mutex> lock(gClientHandlerMtx);
        if (gClientHandler != nullptr) {
            MMC_LOG_INFO("client handler already registered");
            return MMC_OK;
        }
        gClientHandler = new (std::nothrow) MmcClientDefault("mmc_client");
        if (gClientHandler == nullptr) {
            MMC_LOG_ERROR("alloc client handler failed");
            return MMC_ERROR;
        }
        return MMC_OK;
    }

    static Result UnregisterInstance()
    {
        std::lock_guard<std::mutex> lock(gClientHandlerMtx);
        if (gClientHandler == nullptr) {
            MMC_LOG_INFO("client handler already unregistered");
            return MMC_OK;
        }
        delete gClientHandler;
        gClientHandler = nullptr;
        return MMC_OK;
    }

    static MmcClientDefault* GetInstance()
    {
        std::lock_guard<std::mutex> lock(gClientHandlerMtx);
        return gClientHandler;
    }

private:
    explicit MmcClientDefault(const std::string& name) : name_(name) {}
    explicit MmcClientDefault(const MmcClientDefault&) = delete;
    MmcClientDefault& operator=(const MmcClientDefault&) = delete;

    inline uint32_t RankId(const affinity_policy &policy);
    Result PrePutHandle(const MmcBufferArray &bufArr, mmc_put_options &options, AllocRequest &request, uint32_t flags);
    Result AllocateAndPutBlobs(const std::vector<std::string>& keys, const std::vector<MmcBufferArray>& bufs,
                               const mmc_put_options& options, uint32_t flags, uint64_t operateId,
                               std::vector<int>& batchResult, BatchAllocResponse& allocResponse);
    void UpdateRegisterMap(uint64_t va, uint64_t size);
    bool QueryInRegisterMap(const mmc_buffer &buf);
    bool QueryInRegisterMap(uint64_t va, uint64_t size);
    int32_t SelectTransportType(const mmc_buffer &buf);

    static std::mutex gClientHandlerMtx;
    static MmcClientDefault *gClientHandler;
    std::mutex mutex_;
    bool started_ = false;
    MetaNetClientPtr metaNetClient_;
    MmcBmProxyPtr bmProxy_;
    std::string name_;
    uint32_t rankId_{UINT32_MAX};
    uint32_t rpcRetryTimeOut_ = 0;
    uint64_t defaultTtlMs_ = MMC_DATA_TTL_MS;
    MmcThreadPoolPtr threadPool_;
    MmcThreadPoolPtr ioThreadPool_;
    std::set<uint64_t> registerSet_; // va << 1 | is_left
};

uint32_t MmcClientDefault::RankId(const affinity_policy &policy)
{
    if (policy == NATIVE_AFFINITY) {
        return rankId_;
    } else {
        MMC_LOG_ERROR("affinity policy " << policy << " not supported");
        return 0;
    }
}
using MmcClientDefaultPtr = MmcRef<MmcClientDefault>;
}
}

#endif  // MEM_FABRIC_MMC_CLIENT_DEFAULT_H