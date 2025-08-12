/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_BM_PROXY_H
#define MEM_FABRIC_MMC_BM_PROXY_H

#include <mutex>
#include <map>
#include "smem.h"
#include "smem_bm.h"
#include "mmc_def.h"
#include "mmc_mem_blob.h"
#include "mmc_types.h"
#include "mmc_ref.h"

typedef struct {
    uint32_t deviceId;
    uint32_t rankId;
    uint32_t worldSize;
    std::string ipPort;
    std::string hcomUrl;
    int32_t autoRanking;
    int32_t logLevel;
    ExternalLog logFunc;
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
class MmcBmProxy : public MmcReferable{
public:
    explicit MmcBmProxy(const std::string &name) : name_(name) {}
    ~MmcBmProxy() override = default;

    // 删除拷贝构造函数和赋值运算符
    MmcBmProxy(const MmcBmProxy&) = delete;
    MmcBmProxy& operator=(const MmcBmProxy&) = delete;

    Result InitBm(const mmc_bm_init_config_t &initConfig, const mmc_bm_create_config_t &createConfig);
    void DestroyBm();
    Result Put(const mmc_buffer* buf, uint64_t bmAddr, uint64_t size);
    Result Get(const mmc_buffer* buf, uint64_t bmAddr, uint64_t size);
    Result Put(const MmcBufferArray& bufArr, const MmcMemBlobDesc& blob);
    Result Get(const MmcBufferArray& bufArr, const MmcMemBlobDesc& blob);
    uint64_t GetGva() const { return reinterpret_cast<uint64_t>(gva_); }
    MediaType GetMediaType() const {return mediaType_; }
    inline uint32_t RankId() const;

private:
    void *gva_ = nullptr;
    smem_bm_t handle_ = nullptr;
    std::string name_;
    bool started_ = false;
    std::mutex mutex_;
    uint32_t bmRankId_;
    MediaType mediaType_ {MEDIA_NONE};
};

uint32_t MmcBmProxy::RankId() const {
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

#endif  //MEM_FABRIC_MMC_BM_PROXY_H
