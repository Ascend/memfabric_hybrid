/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef SMEM_MMC_META_NET_CLIENT_H
#define SMEM_MMC_META_NET_CLIENT_H

#include <chrono>
#include <thread>

#include "mmc_local_common.h"
#include "mmc_net_engine.h"
#include "mmc_blob_common.h"

namespace ock {
namespace mmc {
constexpr int NET_RETRY_COUNT = 180;
constexpr int TIMEOUT_60_SECONDS = 60;
constexpr int SYNC_CALL_INTERVAL = 200;
constexpr int FIRST_RETRY = 1;
constexpr int RETRY_LOG_INTERVAL = 10;

using ClientRetryHandler = std::function<int32_t(void)>;
using ClientReplicateHandler = std::function<int32_t(const std::vector<uint32_t> &ops,
                                                     const std::vector<std::string> &keys,
                                                     const std::vector<MmcMemBlobDesc> &blobs)>;
using ClientBlobCopyHandler = std::function<int32_t(const MmcMemBlobDesc &src, const MmcMemBlobDesc &dst)>;
class MetaNetClient : public MmcReferable {
public:
    explicit MetaNetClient(const std::string &serverUrl, const std::string &inputName = "");

    ~MetaNetClient() override;

    /**
     * @brief Start the net client to meta net server
     *
     * @param config       [in] mmc client config
     * @return 0 if successful
     */
    Result Start(const NetEngineOptions &config);

    /**
     * @brief Stop the net client
     */
    void Stop();

    /**
     * @brief Connect to net server
     *
     * @param url          [in] url of net server
     * @return 0 if successful
     */
    Result Connect(const std::string &url);

    /**
     * @brief Do sync call to net server, returned if server response or timeout
     *
     * @tparam REQ         [in] request class type
     * @tparam RESP        [in] response class type
     * @param req          [in] request
     * @param resp         [in/out] response
     * @param timeoutInMilliSecond [in] timeout in millisecond
     * @return
     */
    template <typename REQ, typename RESP>
    Result SyncCall(const REQ &req, RESP &resp, int32_t timeoutInMilliSecond)
    {
        Result ret = MMC_ERROR;

        auto start = std::chrono::high_resolution_clock::now();
        // 预计算超时时间点（开始时间 + 超时毫秒数）
        const auto timeoutTime = start + std::chrono::milliseconds(timeoutInMilliSecond);
        int retryCount = 0;  // 记录重试次数
        do {
            ret = engine_->Call(rankId_, req.msgId, req, resp, TIMEOUT_60_SECONDS);
            if (ret != MMC_LINK_NOT_FOUND && ret != MMC_TIMEOUT) {
                break;
            }

            // 检查是否已超时（避免无效休眠）
            auto now = std::chrono::high_resolution_clock::now();
            if (now >= timeoutTime) {
                break;
            }

            // 计算剩余时间，避免休眠超过超时时间
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(timeoutTime - now).count();
            auto sleepTime = std::min(remaining, (int64_t)SYNC_CALL_INTERVAL);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
            retryCount++;
            if (retryCount == FIRST_RETRY || retryCount % RETRY_LOG_INTERVAL == 0) {
                MMC_LOG_DEBUG("SyncCall retry " << retryCount << ", ret=" << ret);
            }
        } while (true);

        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        MMC_RETURN_ERROR(ret, "SyncCall failed, retry " << retryCount
                         << ", and last for " << duration << " milliseconds");

        MMC_LOG_DEBUG("SyncCall successfully, retry " << retryCount
                      << ", and last for " << duration << " milliseconds");
        return ret;
    }

    /**
     * @brief Get status of net client
     *
     * @return status
     */
    bool Status();

    void RegisterRetryHandler(const ClientRetryHandler &retryHandler, const ClientReplicateHandler &replicateHandler,
        const ClientBlobCopyHandler &blobCopyHandler)
    {
        retryHandler_ = retryHandler;
        replicateHandler_ = replicateHandler;
        blobCopyHandler_ = blobCopyHandler;
    }

private:
    Result HandleMetaReplicate(const NetContextPtr &context);
    Result HandlePing(const NetContextPtr &context);
    Result HandleLinkBroken(const NetLinkPtr &link);
    Result HandleBlobCopy(const NetContextPtr& context);

private:
    NetEnginePtr engine_;
    NetLinkPtr link2Index_ = nullptr;
    uint16_t rankId_ = UINT16_MAX;
    std::string ip_ = "";
    uint64_t port_ = 5000U;
    const uint32_t retryCount_ = NET_RETRY_COUNT;
    ClientRetryHandler retryHandler_ = nullptr;
    ClientReplicateHandler replicateHandler_ = nullptr;
    ClientBlobCopyHandler blobCopyHandler_ = nullptr;
    std::string serverUrl_;

    /* not hot used variables */
    std::mutex mutex_;
    bool started_ = false;
    std::string name_;
};

inline bool MetaNetClient::Status()
{
    std::lock_guard<std::mutex> ld(mutex_);
    return started_;
}

using MetaNetClientPtr = MmcRef<MetaNetClient>;

class MetaNetClientFactory : public MmcReferable {
public:
    static MmcRef<MetaNetClient> GetInstance(const std::string &serverUrl, const std::string inputName = "")
    {
        std::lock_guard<std::mutex> lock(instanceMutex_);
        std::string key = serverUrl + inputName;
        auto it = instances_.find(key);
        if (it == instances_.end()) {
            MmcRef<MetaNetClient> instance = new (std::nothrow) MetaNetClient(serverUrl, inputName);
            if (instance == nullptr) {
                MMC_LOG_ERROR("new MetaNetClient failed, probably out of memory");
                return nullptr;
            }
            instances_[key] = instance;
            return instance;
        }
        return it->second;
    }

private:
    static std::map<std::string, MmcRef<MetaNetClient>> instances_;
    static std::mutex instanceMutex_;
};
}
}
#endif  // SMEM_MMC_META_NET_CLIENT_H
