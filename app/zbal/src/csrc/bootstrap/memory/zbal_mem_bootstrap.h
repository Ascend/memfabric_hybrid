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
#ifndef ZBAL_MEM_BOOTSTRAP_H
#define ZBAL_MEM_BOOTSTRAP_H

#include "zbal_common_includes.h"
#include "zbal_bootstrap_types.h"

namespace zbal {
namespace bootstrap {
class MemBootstrap : public ZReferable
{
public:
    static MemBootstrapPtr Create(const MemBootstrapOptions &options);

public:
    ~MemBootstrap() override = default;

    /**
     * @brief Do real initialization according to bootstrap options, including
     * a) create symm object
     * b) setup gva
     *
     * @return 0 if successful
     */
    virtual ZResult Initialize() noexcept = 0;

    /**
     * @brief Un-initialization
     */
    virtual void UnInitialize() noexcept = 0;

    /**
     * @brief Get output after initialized
     *
     * @return output
     */
    const MemBootstrapOutput &GetOutput() const;

    /**
     * @brief Get an unique id for communicator (i.e. a group)
     * which should start from 0
     *
     * @param max          [in] max number that the 'uniqueId' could be
     * @param uniqueId     [in/out] id acquired
     *
     * @return 0 if successful
     */
    virtual ZResult AcquireCommGroupId(uint32_t max, uint32_t &uniqueId) noexcept = 0;

    /**
     * @brief Release the id acquired by AcquireCommGroupId() function
     *
     * @param uniqueId     [in] the id to be released
     */
    virtual ZResult ReleaseCommGroupId(uint32_t uniqueId) noexcept = 0;

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
     * @return 0 if successful
     */
    virtual ZResult SubGroupAllGather(const std::string &key, uint32_t rankSize, uint32_t rankId, const char *sendBuf,
                                      uint32_t sendSize, char *recvBuf, uint32_t recvSize) noexcept = 0;

    /**
     * @brief Do barrier operation with sub partition of world, there is no need to setup sub group firstly,
     * just need to make sure the key is following the rule:
     * a) key is a string
     * b) key should be the same for all participators (i.e. same in the sub group)
     * c) key should be different with other sub group, otherwise it will be messed up
     *
     * @param key
     * @param rankSize
     * @param rankId
     * @return 0 if successful
     */
    virtual ZResult SubGroupBarrier(const std::string &key, uint32_t rankSize, uint32_t rankId) noexcept = 0;

    /**
     * @brief Level logger of bootstrap
     *
     * @param level        [in] level to be set
     *
     * @return 0 if successful
     */
    virtual ZResult SetLoggerLevel(int level) noexcept = 0;

protected:
    MemBootstrap(const MemBootstrapOptions &options) : options_(options) {}
    ZResult VerifyOptions();

protected:
    bool initialized_ = false;
    std::mutex mutex_;

    MemBootstrapOptions options_;
    MemBootstrapOutput output_;
};

inline const MemBootstrapOutput &MemBootstrap::GetOutput() const
{
    return output_;
}
}  // namespace bootstrap
}  // namespace zbal

#endif  // ZBAL_MEM_BOOTSTRAP_H
