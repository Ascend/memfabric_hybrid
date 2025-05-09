/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __SMEM_SHM_ENTRY_H__
#define __SMEM_SHM_ENTRY_H__

#include <map>
#include "hybm_def.h"
#include "smem.h"
#include "smem_shm.h"
#include "smem_net_group_engine.h"

namespace ock {
namespace smem {

class SmemShmEntry : public SmReferable {
public:
    explicit SmemShmEntry(uint32_t id);
    ~SmemShmEntry() override;

    int32_t Initialize(hybm_options &options);

    void SetConfig(const smem_shm_config_t &config);

    Result SetExtraContext(const void *context, uint32_t size);

    void *GetGva() const;

    SmemGroupEnginePtr GetGroup() const;

    uint32_t Id() const;

private:
    Result CreateGlobalTeam(uint32_t rankSize, uint32_t rankId);

private:
    SmemGroupEnginePtr globalGroup_ = nullptr;
    smem_shm_config_t extraConfig_;

    bool inited_ = false;
    const uint32_t id_;
    uint32_t localRank_ = UINT32_MAX;
    hybm_entity_t entity_ = nullptr;
    void *gva_ = nullptr;

    std::mutex entryMutex_;
};
using SmemShmEntryPtr = SmRef<SmemShmEntry>;

inline SmemGroupEnginePtr SmemShmEntry::GetGroup() const
{
    return globalGroup_;
}

inline uint32_t SmemShmEntry::Id() const
{
    return id_;
}
}
}

#endif // __SMEM_SHM_ENTRY_H__