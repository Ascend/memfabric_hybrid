/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_BM_PROXY_H
#define MEM_FABRIC_MMC_BM_PROXY_H

#include <mutex>
#include <map>
#include <vector>
#include "smem.h"
#include "smem_bm.h"
#include "mmc_def.h"
#include "mmc_mem_blob.h"
#include "mmc_types.h"
#include "mmc_ref.h"

typedef struct {
    uint32_t deviceId;
    uint32_t worldSize;
    std::string ipPort;
    std::string hcomUrl;
    int32_t logLevel;
    ExternalLog logFunc;
    uint32_t flags;
    mmc_tls_config hcomTlsConfig;
    mmc_tls_config storeTlsConfig;
} mmc_bm_init_config_t;

typedef struct {
    uint32_t id;
    uint32_t memberSize;
    std::string dataOpType;
    uint64_t localDRAMSize;
    uint64_t localHBMSize;
    uint32_t flags;
} mmc_bm_create_config_t;

namespace ock {
namespace mmc {

class MmcBmProxy : public MmcReferable {
public:
    explicit MmcBmProxy(const std::string &name) : name_(name), spaces_{0}, bmRankId_{0} {}
    ~MmcBmProxy() override = default;

    // 删除拷贝构造函数和赋值运算符
    MmcBmProxy(const MmcBmProxy&) = delete;
    MmcBmProxy& operator=(const MmcBmProxy&) = delete;

    Result InitBm(const mmc_bm_init_config_t &initConfig, const mmc_bm_create_config_t &createConfig);
    void DestroyBm();
    Result Put(uint64_t srcBmAddr, uint64_t dstBmAddr, uint64_t size, smem_bm_copy_type type);
    Result Put(const mmc_buffer* buf, uint64_t bmAddr, uint64_t size);
    Result Get(const mmc_buffer* buf, uint64_t bmAddr, uint64_t size);
    Result Put(const MmcBufferArray& bufArr, const MmcMemBlobDesc& blob);
    Result Get(const MmcBufferArray& bufArr, const MmcMemBlobDesc& blob);
    Result BatchPut(const MmcBufferArray& bufArr, const MmcMemBlobDesc& blob);
    Result BatchGet(const MmcBufferArray& bufArr, const MmcMemBlobDesc& blob);
    Result BatchDataPut(std::vector<void *> &sources, std::vector<void *> &destinations,
                        const std::vector<uint64_t> &sizes, MediaType localMedia);
    Result BatchDataGet(std::vector<void *> &sources, std::vector<void *> &destinations,
                        const std::vector<uint64_t> &sizes, MediaType localMedia);
    Result RegisterBuffer(uint64_t addr, uint64_t size);
    Result RegisterLayerBuffer(const uint64_t *addrs, const uint64_t *sizes, uint64_t layer, uint64_t num);

    Result CopyWait();

    uint64_t GetGva(MediaType type) const
    {
        if (type == MEDIA_NONE) {
            return 0;
        }
        return reinterpret_cast<uint64_t>(gvas_[type]);
    }

    uint64_t GetCapacity(MediaType type) const
    {
        if (type == MEDIA_NONE) {
            return 0;
        }
        return spaces_[type];
    }

    MediaType GetMediaType();
    std::string GetDataOpType() const;
    inline uint32_t RankId() const;

private:
    Result InternalCreateBm(const mmc_bm_create_config_t &createConfig);

    void* gvas_[MEDIA_NONE]{};
    uint64_t spaces_[MEDIA_NONE];
    smem_bm_t handle_ = nullptr;
    std::string name_;
    bool started_ = false;
    std::mutex mutex_;
    uint32_t bmRankId_;
    MediaType mediaType_ {MEDIA_NONE};
    mmc_bm_create_config_t createConfig_{};
};

uint32_t MmcBmProxy::RankId() const
{
    return bmRankId_;
}

using MmcBmProxyPtr = MmcRef<MmcBmProxy>;

class MmcBmProxyFactory : public MmcReferable {
public:
    static MmcBmProxyPtr GetInstance(const std::string& key = "")
    {
        std::lock_guard<std::mutex> lock(instanceMutex_);
        const auto it = instances_.find(key);
        if (it == instances_.end()) {
            MmcRef<MmcBmProxy> instance = new (std::nothrow)MmcBmProxy("bmProxy");
            if (instance == nullptr) {
                MMC_LOG_ERROR("new object failed, probably out of memory");
                return nullptr;
            }
            instances_[key] = instance;
            return instance;
        }
        return it->second;
    }

private:
    static std::map<std::string, MmcRef<MmcBmProxy>> instances_;
    static std::mutex instanceMutex_;
};
}
}

#endif  // MEM_FABRIC_MMC_BM_PROXY_H
