/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_BM_PROXY_H
#define MEM_FABRIC_MMC_BM_PROXY_H

#include <mutex>
#include "smem.h"
#include "smem_bm.h"
#include "mmc_def.h"
#include "mmc_types.h"
#include "mmc_meta_common.h"

typedef struct {
    uint32_t deviceId;
    uint32_t rankId;
    uint32_t worldSize;
    std::string ipPort;
    int autoRanking;
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
class MmcBmProxy {
public:
    // 删除拷贝构造函数和赋值运算符
    MmcBmProxy(const MmcBmProxy&) = delete;
    MmcBmProxy& operator=(const MmcBmProxy&) = delete;

    // 获取单例实例的静态方法
    static MmcBmProxy& GetInstance()
    {
        static MmcBmProxy instance;  // C++11 保证静态局部变量线程安全
        return instance;
    }

    Result InitBm(const mmc_bm_init_config_t &initConfig, const mmc_bm_create_config_t &createConfig);
    void DestoryBm();
    Result Put(mmc_buffer *buf, uint64_t bmAddr);
    Result Get(mmc_buffer *buf, uint64_t bmAddr);

    uint64_t GetGva() const { return reinterpret_cast<uint64_t>(gva_); }

private:
    MmcBmProxy() {}
    ~MmcBmProxy() {}

    void *gva_ = nullptr;
    smem_bm_t handle_ = nullptr;
    std::mutex mutex_;
};

}
}

#endif  //MEM_FABRIC_MMC_BM_PROXY_H
