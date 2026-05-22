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
#ifndef ZBAL_INIT_STATE_H
#define ZBAL_INIT_STATE_H

#include "zbal_common_includes.h"

namespace zbal {
struct ZBALInitStateExt {
    zbal_bootstrap_type_t btType;        /* bootstrap type */
    uint16_t worldSize = 0;              /* world size */
    uint16_t worldRankId = 0;            /* world rank id */
    uint16_t deviceId = 0;               /* device id */
    uint16_t commMetaSpaceSize;          /* optional, in KB, default 1MB, min: 512KB, max: 4MB */
    uint16_t commGroupCap;               /* optional, max count of comm Group, default 128, min: 1, max: 512 */
    void *gvaDevice = nullptr;           /* global gva */
    void *myCommMetaDeviceGva = nullptr; /* gva of comm meta of this rank */
    uint64_t metaSizeOfDevice = 0;       /* size of device memory for SMA */
    void *mySMAGva = nullptr;            /* gva of sma of this rank */
    uint64_t smaSizeOfDevice = 0;        /* size of device memory for SMA */
    uint64_t localDeviceMemSize = 0;     /* local device mem size */
    uint64_t symmetricMemSpace = 0;      /* space size, as localDeviceMemSize could be less than symmetricMemSpace */
    uint32_t dataOperationType = 0;      /* data operation type, see zbal_data_op_type_t */
};

class ZBALInitState {
public:
    static ZBALInitState &Instance()
    {
        static ZBALInitState gInitState;
        return gInitState;
    }

public:
    ZBALInitState() = default;
    ~ZBALInitState() = default;

    void Bootstrapped(bool bootstrapped) noexcept;
    bool Bootstrapped() const noexcept;

    bool HasCommunicator() const noexcept;
    void CommunicatorCreated(uint16_t count = 1) noexcept;
    void CommunicatorDestroy(uint16_t count = 1) noexcept;

    void SmaInitialized(bool smaInited) noexcept;
    bool SmaInitialized() const noexcept;

    void Reset() noexcept;

public:
    ZBALInitStateExt ext_{};

private:
    std::atomic<bool> bootstrapped_{false};
    std::atomic<bool> smaInited_{false};
    std::atomic<int16_t> communicatorCount_{0};
};

ALWAYS_INLINE void ZBALInitState::Bootstrapped(bool bootstrapped) noexcept
{
    bootstrapped_ = bootstrapped;
}

ALWAYS_INLINE bool ZBALInitState::Bootstrapped() const noexcept
{
    return bootstrapped_.load();
}

ALWAYS_INLINE bool ZBALInitState::HasCommunicator() const noexcept
{
    return communicatorCount_.load() > 0;
}

ALWAYS_INLINE void ZBALInitState::CommunicatorCreated(uint16_t count) noexcept
{
    communicatorCount_ += count;
}
ALWAYS_INLINE void ZBALInitState::CommunicatorDestroy(uint16_t count) noexcept
{
    communicatorCount_ -= count;
}

ALWAYS_INLINE void ZBALInitState::SmaInitialized(bool smaInited) noexcept
{
    smaInited_ = smaInited;
}

ALWAYS_INLINE bool ZBALInitState::SmaInitialized() const noexcept
{
    return smaInited_.load();
}

ALWAYS_INLINE void ZBALInitState::Reset() noexcept
{
    bootstrapped_ = false;
    smaInited_ = false;
    communicatorCount_ = 0;
    bzero(&ext_, sizeof(ZBALInitStateExt));
}

} // namespace zbal

#endif // ZBAL_INIT_STATE_H
