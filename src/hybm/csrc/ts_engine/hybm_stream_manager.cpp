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

#include "hybm_stream_manager.h"
#include "dl_acl_api.h"
#include "hybm_logger.h"

namespace ock {
namespace mf {
HybmStreamPtr HybmStreamManager::GetThreadHybmStream(uint32_t devId, uint32_t prio, uint32_t flags)
{
    static thread_local HybmStreamPtr hybmStream_ = nullptr;
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
    static uint32_t ACL_STREAM_FAST_LAUNCH = 1U;
    static uint32_t ACL_STREAM_FAST_SYNC = 2U;
    if (stream_ != nullptr) {
        return stream_;
    }
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

    static thread_local AclrtStream aclrtStream_(stream_);
    return stream_;
}
}
}
