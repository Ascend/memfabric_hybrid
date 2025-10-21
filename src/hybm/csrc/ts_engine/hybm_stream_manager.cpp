/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#include "hybm_stream_manager.h"
#include "dl_acl_api.h"
#include "hybm_logger.h"

namespace ock {
namespace mf {
HybmStreamPtr HybmStreamManager::GetThreadHybmStream(uint32_t devId, uint32_t prio, uint32_t flags)
{
    static thread_local HybmStreamPtr hybmStream_ = nullptr;
    if (hybmStream_ != nullptr) {
        return hybmStream_;
    }
    hybmStream_ = std::make_shared<HybmStream>(devId, prio, flags);
    auto ret = hybmStream_->Initialize();
    if (ret != BM_OK) {
        BM_LOG_ERROR("HybmStream init failed: " << ret);
        hybmStream_ = nullptr;
    }
    return hybmStream_;
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
    if (stream_ != nullptr) {
        return stream_;
    }
    auto ret = DlAclApi::AclrtSetDevice(devId);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Set device id to be " << devId << " failed: " << ret);
        return nullptr;
    }
    ret = DlAclApi::AclrtCreateStream(&stream_);
    if (ret != 0) {
        BM_LOG_ERROR("create stream failed: " << ret);
        return nullptr;
    }

    static thread_local AclrtStream aclrtStream_(stream_);
    return stream_;
}
}
}
