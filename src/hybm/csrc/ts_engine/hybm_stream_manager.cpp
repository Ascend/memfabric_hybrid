/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/

#include <thread>
#include <shared_mutex>
#include <unordered_map>
#include "dl_acl_api.h"
#include "hybm_logger.h"
#include "hybm_stream_manager.h"

namespace ock {
namespace mf {
static std::shared_mutex g_allThreadStreamMutex;
static std::unordered_map<uint64_t, HybmStreamPtr> g_allThreadStreams;
HybmStreamPtr HybmStreamManager::GetThreadHybmStream(uint32_t devId, uint32_t prio, uint32_t flags)
{
    const auto thisId = static_cast<uint64_t>(syscall(SYS_gettid));
    {
        std::shared_lock lock(g_allThreadStreamMutex);
        auto it = g_allThreadStreams.find(thisId);
        if (it != g_allThreadStreams.end()) {
            return it->second;
        }
    }
    HybmStreamPtr hybmStream_ = nullptr;
    {
        std::unique_lock lock(g_allThreadStreamMutex);
        auto it = g_allThreadStreams.find(thisId);
        if (it != g_allThreadStreams.end()) {
            return it->second;
        }
        hybmStream_ = std::make_shared<HybmStream>(devId, prio, flags);
        auto ret = hybmStream_->Initialize();
        if (ret != BM_OK) {
            BM_LOG_ERROR("HybmStream init failed: " << ret);
            hybmStream_->Destroy();
            return nullptr;
        }
        g_allThreadStreams[thisId] = hybmStream_;
    }
    return hybmStream_;
}

void HybmStreamManager::ResetThreadHybmStream(uint64_t tid) noexcept
{
    std::unique_lock lock_guard(g_allThreadStreamMutex);
    auto find = g_allThreadStreams.find(tid);
    if (find != g_allThreadStreams.end()) {
        find->second->Destroy();
        g_allThreadStreams.erase(find);
    }
}

void HybmStreamManager::DestroyAllThreadHybmStream()
{
    std::unique_lock<std::shared_mutex> lock_guard(g_allThreadStreamMutex);
    for (auto &stream : g_allThreadStreams) {
        if (stream.second != nullptr) {
            stream.second->Destroy();
        }
    }
    g_allThreadStreams.clear();
}

class AclrtStream {
public:
    explicit AclrtStream(void *stream) : stream_(stream) {}
    ~AclrtStream()
    {
        if (stream_ != nullptr) {
            DlAclApi::AclrtDestroyStream(stream_);
        }
    }
private:
    void *stream_;
};

void *HybmStreamManager::GetThreadAclStream(int32_t devId)
{
    static thread_local void *stream_ = nullptr;
    static uint32_t ACL_STREAM_FAST_LAUNCH = 1U;
    static uint32_t ACL_STREAM_FAST_SYNC = 2U;
    if (stream_ != nullptr) {
        return stream_;
    }
#if XPU_TYPE == XPU_NPU
    auto ret = DlAclApi::AclrtSetDevice(devId);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Set device id to be " << devId << " failed: " << ret);
        return nullptr;
    }
    ret = DlAclApi::AclrtCreateStreamWithConfig(&stream_, 0, ACL_STREAM_FAST_LAUNCH | ACL_STREAM_FAST_SYNC);
    if (ret != 0) {
        BM_LOG_ERROR("create stream failed: " << ret);
        return nullptr;
    }
#endif
    static thread_local AclrtStream aclrtStream_(stream_);
    return stream_;
}
}
}
