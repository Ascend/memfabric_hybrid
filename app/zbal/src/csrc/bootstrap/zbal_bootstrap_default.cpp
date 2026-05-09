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
#include <csignal>
#include "zbal_signal_handler.h"
#include "zbal_bootstrap_default.h"

namespace zbal {
namespace bootstrap {
std::mutex Bootstrap::gMutex;
BootstrapPtr Bootstrap::gBootstrap = nullptr;

BootstrapPtr Bootstrap::Create(const zbal_bootstrap_options_t &options)
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gBootstrap != nullptr) {
        return gBootstrap;
    }

    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ZBAL_VALIDATE_RETURN(bootstrap != nullptr, "create bootstrap object failed, probably out of memory", nullptr);

    auto result = bootstrap->Initialize();
    ZBAL_VALIDATE_RETURN(result == Z_OK, "create bootstrap object failed, initialization failed", nullptr);

    gBootstrap = bootstrap;

    return gBootstrap;
}

void Bootstrap::Destroy()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gBootstrap == nullptr) {
        return;
    }

    gBootstrap->UnInitialize();
    gBootstrap = nullptr;
}

BootstrapPtr Bootstrap::Get()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gBootstrap != nullptr) {
        return gBootstrap;
    }

    ZBAL_LOG_DEBUG("Get bootstrap failed as it is not created");
    return nullptr;
}

ZResult Bootstrap::VerifyOptions() noexcept
{
    ZBAL_VALIDATE_RETURN(0 <= options_.btType && options_.btType < BOOT_BY_BUTT,
                         "invalid option, bootstrapType is invalid", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(options_.ipPort != nullptr, "invalid option, ipPort is nullptr", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(options_.worldSize <= RANK_COUNT_MAX_LIMIT, "invalid option, worldSize too large",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(options_.rankId < options_.worldSize, "invalid option, rankId should be less than worldSize",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(options_.deviceId < DEVICE_COUNT_MAX_LIMIT, "invalid option, deviceId is incorrect",
                         Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(options_.deviceMemorySize < MEMORY_SIZE_CAP, "invalid options, memory size is too large",
                         Z_INVALID_PARAM);

    if (options_.commGroupCap >= COMM_GROUP_COUNT_CAP_MAX) {
        ZBAL_LOG_ERROR("comm group count cap is over the limit.");
        return Z_INVALID_PARAM;
    }

    if (static_cast<uint64_t>(options_.commGroupCap) * options_.commMetaSpaceSize * 1024 >= options_.deviceMemorySize) {
        ZBAL_LOG_ERROR("total meta space size is GE total device memory size.");
        return Z_INVALID_PARAM;
    }

    if (options_.commMetaSpaceSize * 1024 <= OPERATE_PARAM_SIZE) {
        ZBAL_LOG_ERROR("single meta space size is too small");
        return Z_INVALID_PARAM;
    }
    return Z_OK;
}

ZResult Bootstrap::Initialize() noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (inited_) {
        ZBAL_LOG_INFO("Bootstrap already initialized, no action required");
        return Z_OK;
    }

    /* init signal handler */
    std::signal(SIGUSR1, signal_handler);

    /* initialize env */
    EnvHelper::Initialize();
    EnvHelper::DumpEnv();

    /* verify basic options */
    auto result = VerifyOptions();
    if (result != Z_OK) {
        ZBAL_LOG_ERROR("Initialize bootstrap options invalid, ret=" << result);
        return result;
    }

    /* create memory bootstrap, before create comm bootstrap */
    result = CreateMemBootstrap();
    if (result != Z_OK) {
        ZBAL_LOG_ERROR("Create mem bootstrap failed. ret=" << result);
        return result;
    }

    ZBAL_LOG_DEBUG("bootstrap init success.");
    inited_ = true;
    return Z_OK;
}

void Bootstrap::UnInitialize() noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!inited_) {
        ZBAL_LOG_INFO("Bootstrap not initialized");
        return;
    }

    /* destroy memory bootstrap */
    DestroyMemoryBootstrap();

    /* set flag */
    inited_ = false;
}

ZResult Bootstrap::CreateMemBootstrap() noexcept
{
    /* translate options from api options to internal options */
    MemBootstrapOptions memOptions{};
    memOptions.boostrapType = static_cast<MemBoostrapType>(options_.btType);
    memOptions.deviceId = options_.deviceId;
    memOptions.rankCount = options_.worldSize;
    memOptions.rankId = options_.rankId;
    memOptions.totalMemSize = options_.deviceMemorySize;
    memOptions.flags = options_.flags;
    memOptions.dataOperationType = options_.dataOperationType;
    memOptions.ipPort = std::string(options_.ipPort);

    ZBAL_LOG_INFO("MemBootstrapOptions dump: " << memOptions);

    /* new bootstrap */
    auto memBootstrap = MemBootstrap::Create(memOptions);
    if (memBootstrap == nullptr) {
        ZBAL_LOG_ERROR("Create MemoryBootstrap instance failed");
        return Z_ERROR;
    }

    /* initialize */
    auto result = memBootstrap->Initialize();
    if (result != Z_OK) {
        ZBAL_LOG_ERROR("mem bootstrap init failed, ret=" << result);
        return result;
    }

    /* assign output */
    auto &memOutput = memBootstrap->GetOutput();
    output_.deviceGva = memOutput.gvaDevice;
    output_.myDeviceGva = memOutput.myGvaDevice;
    output_.createdDeviceMemorySpaceSize = memOutput.memorySpaceSizeDevice;
    output_.allocatedDeviceMemorySize = memOutput.memorySizeDevice;
    output_.myCommMetaDeviceGva = memOutput.myGvaDevice;
    output_.metaSizeOfDevice = options_.commMetaSpaceSize;
    /* translate to bytes */
    output_.metaSizeOfDevice = output_.metaSizeOfDevice * 1024 * options_.commGroupCap;
    output_.mySMAGva =
        reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(output_.myDeviceGva) + output_.metaSizeOfDevice);
    output_.smaSizeOfDevice = output_.allocatedDeviceMemorySize - output_.metaSizeOfDevice;

    ZBAL_LOG_DEBUG("bootstrap output: " << output_);

    memBootstrap_ = memBootstrap;
    ZBAL_LOG_DEBUG("create mem bootstrap success.");
    return Z_OK;
}

void Bootstrap::DestroyMemoryBootstrap() noexcept
{
    if (memBootstrap_ == nullptr) {
        return;
    }

    /* swap to tmp bootstrap */
    auto tmpBootstrap = memBootstrap_;
    memBootstrap_ = nullptr;

    /* do un-initialize */
    tmpBootstrap->UnInitialize();
}

ZResult Bootstrap::AcquireCommGroupId(uint32_t max, uint32_t &uniqueId) noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (memBootstrap_ == nullptr) {
        ZBAL_LOG_DEBUG("Not bootstrapped");
        return Z_NOT_BOOTSTRAPPED;
    }

    return memBootstrap_->AcquireCommGroupId(max, uniqueId);
}

ZResult Bootstrap::ReleaseCommGroupId(uint32_t uniqueId) noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (memBootstrap_ == nullptr) {
        ZBAL_LOG_DEBUG("Not bootstrapped");
        return Z_NOT_BOOTSTRAPPED;
    }

    return memBootstrap_->ReleaseCommGroupId(uniqueId);
}

ZResult Bootstrap::SubGroupAllGather(const std::string &key, uint32_t rankSize, uint32_t rankId, const char *sendBuf,
                                     uint32_t sendSize, char *recvBuf, uint32_t recvSize)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (memBootstrap_ == nullptr) {
        ZBAL_LOG_DEBUG("Not bootstrapped");
        return Z_NOT_BOOTSTRAPPED;
    }

    return memBootstrap_->SubGroupAllGather(key, rankSize, rankId, sendBuf, sendSize, recvBuf, recvSize);
}

ZResult Bootstrap::SubGroupBarrier(const std::string &key, uint32_t rankSize, uint32_t rankId)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (memBootstrap_ == nullptr) {
        ZBAL_LOG_DEBUG("Not bootstrapped");
        return Z_NOT_BOOTSTRAPPED;
    }

    return memBootstrap_->SubGroupBarrier(key, rankSize, rankId);
}

ZResult Bootstrap::SetLoggerLevel(int level)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (memBootstrap_ == nullptr) {
        ZBAL_LOG_DEBUG("Not bootstrapped");
        return Z_NOT_BOOTSTRAPPED;
    }

    return memBootstrap_->SetLoggerLevel(level);
}
} // namespace bootstrap
} // namespace zbal