/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#ifndef SMEM_MMC_META_NET_CLIENT_H
#define SMEM_MMC_META_NET_CLIENT_H
#include "mmc_local_common.h"
#include "mmc_net_engine.h"
#include "mmc_blob_common.h"
namespace ock {
namespace mmc {
#define NET_RETRY_COUNT 3
using ClientRetryHandler = std::function<int32_t(void)>;
using ClientReplicateHandler = std::function<int32_t(const std::string &key, const MmcMemBlobDesc &blobDesc)>;
using ClientReplicateRemoveHandler = std::function<int32_t(const std::string &key)>;
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
     * @param timeoutInSecond [in] timeout in second
     * @return
     */
    template <typename REQ, typename RESP>
    Result SyncCall(const REQ &req, RESP &resp, int32_t timeoutInSecond)
    {
        Result ret = MMC_ERROR;
        for (uint32_t count = 0; count < retryCount_; count++) {
            ret = engine_->Call(rankId_, req.msgId, req, resp, timeoutInSecond);
            if (ret == MMC_OK) {
                return MMC_OK;
            }
        }
        return ret;
    }

    /**
     * @brief Get status of net client
     *
     * @return status
     */
    bool Status();

    void RegisterRetryHandler(const ClientRetryHandler &retryHandler, const ClientReplicateHandler &replicateHandler,
        const ClientReplicateRemoveHandler &replicateRemoveHandler)
    {
        retryHandler_ = retryHandler;
        replicateHandler_ = replicateHandler;
        replicateRemoveHandler_ = replicateRemoveHandler;
    }

private:
    Result HandleMetaReplicate(const NetContextPtr &context);
    Result HandlePing(const NetContextPtr &context);
    Result HandleLinkBroken(const NetLinkPtr &link);

private:
    NetEnginePtr engine_;
    NetLinkPtr link2Index_ = nullptr;
    uint16_t rankId_ = UINT16_MAX;
    std::string ip_ = "";
    uint64_t port_ = 5000U;
    const uint32_t retryCount_ = NET_RETRY_COUNT;
    ClientRetryHandler retryHandler_ = nullptr;
    ClientReplicateHandler replicateHandler_ = nullptr;
    ClientReplicateRemoveHandler replicateRemoveHandler_ = nullptr;
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
