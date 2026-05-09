/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef ZBAL_MF_BOOTSTRAP_H
#define ZBAL_MF_BOOTSTRAP_H

#include "smem_shm_def.h"
#include "zbal_mem_bootstrap.h"

namespace zbal {
namespace bootstrap {
class MemFabricBoostrap : public MemBootstrap {
public:
    explicit MemFabricBoostrap(const MemBootstrapOptions &options) : MemBootstrap(options) {}
    ~MemFabricBoostrap() override
    {
        UnInitialize();
    }

    ZResult Initialize() noexcept override;
    void UnInitialize() noexcept override;

    ZResult AcquireCommGroupId(uint32_t max, uint32_t &uniqueId) noexcept override;
    ZResult ReleaseCommGroupId(uint32_t uniqueId) noexcept override;

    ZResult SubGroupAllGather(const std::string &key, uint32_t rankSize, uint32_t rankId, const char *sendBuf,
                              uint32_t sendSize, char *recvBuf, uint32_t recvSize) noexcept override;

    ZResult SubGroupBarrier(const std::string &key, uint32_t rankSize, uint32_t rankId) noexcept override;

    ZResult SetLoggerLevel(int level) noexcept override;

private:
    ZResult GetMemFabricLibPath(std::string &path) noexcept;
    ZResult GetAscendLibPath(std::string &path) noexcept;
    ZResult InitPreCheck() noexcept;
    ZResult CreateSHMSpace() noexcept;

private:
    /* one bootstrap maps to one shm handle */
    smem_shm_config_t shmConfig_;
    uint32_t shmId_ = 0;
    smem_shm_t shmHandle_ = nullptr;
    void *shmGva = nullptr;
};
} // namespace bootstrap
} // namespace zbal

#endif // ZBAL_MF_BOOTSTRAP_H
