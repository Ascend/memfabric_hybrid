/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_META_SERVICE_DEFAULT_H
#define MEM_FABRIC_MMC_META_SERVICE_DEFAULT_H

#include "mmc_def.h"
#include "mmc_global_allocator.h"
#include "mmc_meta_common.h"
#include "mmc_meta_net_server.h"
#include "mmc_meta_service.h"
#include "mmc_meta_mgr_proxy_default.h"

namespace ock {
namespace mmc {
#define OBJ_MAX_LOG_FILE_SIZE 20971520 // 每个日志文件的最大大小
#define OBJ_MAX_LOG_FILE_NUM 50
class MmcMetaServiceDefault : public MmcMetaService {
public:
    explicit MmcMetaServiceDefault(const std::string& name) : name_(name) {}

    Result Start(const mmc_meta_service_config_t &options) override;

    void Stop() override;

    Result BmRegister(uint32_t rank, uint16_t mediaType, uint64_t bm, uint64_t capacity,
        std::map<std::string, MmcMemBlobDesc> &blobMap);

    Result BmUnregister(uint32_t rank, uint16_t mediaType);

    Result ClearResource(uint32_t rank);

    const std::string &Name() const override;

    const mmc_meta_service_config_t &Options() const override;

    const MmcMetaMgrProxyPtr& GetMetaMgrProxy() const override;

private:

    Result GetLogPath(std::string &logPath, std::string &logAuditPath);
    MetaNetServerPtr metaNetServer_;
    MmcMetaMgrProxyPtr metaMgrProxy_;
    MMCMetaBackUpMgrPtr metaBackUpMgrPtr_;

    std::mutex mutex_;
    bool started_ = false;
    std::string name_;
    uint32_t registerRank_ = 0;
    mmc_meta_service_config_t options_;
    std::unordered_map<uint32_t, std::unordered_set<uint16_t>> rankMediaTypeMap_;
};
inline const std::string &MmcMetaServiceDefault::Name() const
{
    return name_;
}

inline const mmc_meta_service_config_t &MmcMetaServiceDefault::Options() const
{
    return options_;
}

inline const MmcMetaMgrProxyPtr& MmcMetaServiceDefault::GetMetaMgrProxy() const
{
    return metaMgrProxy_;
}

using MmcMetaServiceDefaultPtr = MmcRef<MmcMetaServiceDefault>;
}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_META_SERVICE_DEFAULT_H
