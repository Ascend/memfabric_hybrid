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
#ifndef ZBAL_BOOTSTRAP_DEFAULT_H
#define ZBAL_BOOTSTRAP_DEFAULT_H

#include "zbal_common_includes.h"
#include "zbal_mem_bootstrap.h"

namespace zbal {
namespace bootstrap {
class Bootstrap : public ZReferable {
public:
    static BootstrapPtr Create(const zbal_bootstrap_options_t &options);
    static void Destroy();
    static BootstrapPtr Get();

public:
    explicit Bootstrap(const zbal_bootstrap_options_t &options) : options_(options) {}
    ~Bootstrap() override
    {
        UnInitialize();
    }

    /**
     * @brief Initialize bootstrap according to options
     *
     * @return 0 if successful, error if failed
     */
    ZResult Initialize() noexcept;

    /**
     * @brief Un-initialize bootstrap
     */
    void UnInitialize() noexcept;

    /**
     * @brief Get output if initialized
     *
     * @return output
     */
    const zbal_bootstrap_output_t &GetOutput();

    /**
     * @brief Get an unique id for communicator (i.e. a group)
     * which should start from 0
     *
     * @param max          [in] max number that the 'uniqueId' could be
     * @param uniqueId     [in/out] id acquired
     *
     * @return 0 if successful
     */
    ZResult AcquireCommGroupId(uint32_t max, uint32_t &uniqueId) noexcept;

    /**
     * @brief Release the id acquired by AcquireCommGroupId() function
     *
     * @param uniqueId     [in] the id to be released
     */
    ZResult ReleaseCommGroupId(uint32_t uniqueId) noexcept;

    /**
     * @brief Do allGather operation with sub partition of world, there is no need to setup sub group firstly,
     * just need to make sure the key is following the rule:
     * a) key is a string
     * b) key should be the same for all participators (i.e. same in the sub group)
     * c) key should be different with other sub group, otherwise it will be messed up
     *
     * @param key          [in] key name for this barrier, which should be same in sub group but unique in the world
     * @param rankSize     [in] rank size of the sub group
     * @param rankId       [in] rank id in the sub group
     * @param sendBuf      [in] input data buf
     * @param sendSize     [in] input data buf size
     * @param recvBuf      [in] output data buf
     * @param recvSize     [in] output data buf size
     *
     * @return 0 if successful
     */
    ZResult SubGroupAllGather(const std::string &key, uint32_t rankSize, uint32_t rankId, const char *sendBuf,
                              uint32_t sendSize, char *recvBuf, uint32_t recvSize);

    /**
     * @brief Do barrier operation with sub partition of world, there is no need to setup sub group firstly,
     * just need to make sure the key is following the rule:
     * a) key is a string
     * b) key should be the same for all participators (i.e. same in the sub group)
     * c) key should be different with other sub group, otherwise it will be messed up
     *
     *
     * @param key
     * @param rankSize
     * @param rankId
     * @return 0 if successful
     */
    ZResult SubGroupBarrier(const std::string &key, uint32_t rankSize, uint32_t rankId);

    /**
     * @brief Set logger level
     *
     * @param level        [in] level to be set
     *
     * @return 0 if successful
     */
    ZResult SetLoggerLevel(int level);

private:
    ZResult VerifyOptions() noexcept;

    ZResult CreateMemBootstrap() noexcept;
    void DestroyMemoryBootstrap() noexcept;

private:
    MemBootstrapPtr memBootstrap_{nullptr}; /* inner memory bootstrap */
    zbal_bootstrap_options_t options_;      /* options from API */
    zbal_bootstrap_output_t output_;        /* output for API */

    std::mutex mutex_;
    bool inited_{false};

private:
    static std::mutex gMutex;       /* lock for singleton boostrap */
    static BootstrapPtr gBootstrap; /* singleton bootstrap */
};

inline const zbal_bootstrap_output_t &Bootstrap::GetOutput()
{
    std::lock_guard<std::mutex> guard(std::mutex);
    /* reset to if not initialized */
    if (!inited_) {
        bzero(&output_, sizeof(zbal_bootstrap_output_t));
    }

    return output_;
}
} // namespace bootstrap
} // namespace zbal

#endif // ZBAL_BOOTSTRAP_DEFAULT_H
