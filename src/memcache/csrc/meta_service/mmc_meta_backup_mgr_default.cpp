/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "mmc_meta_backup_mgr_factory.h"
#include "mmc_meta_backup_mgr_default.h"
#include "mmc_msg_client_meta.h"

namespace ock {
namespace mmc {
std::map<std::string, MmcRef<MMCMetaBackUpMgr>> MMCMetaBackUpMgrFactory::instances_;
std::mutex MMCMetaBackUpMgrFactory::instanceMutex_;

void MMCMetaBackUpMgrDefault::BackupThreadFunc()
{
    while (true) {
        {
            std::unique_lock<std::mutex> lock(backupThreadLock_);
            backupThreadCv_.wait(lock, [this] { return backupList_.size() || !started_; });
        }
        if (!started_) {
            MMC_LOG_INFO("backup thread destroy, thread id " << pthread_self());
            break;
        }
        MMC_LOG_DEBUG("MMCMetaBackU thread will backup count " << backupList_.size() << " thread id " << pthread_self());
        MetaReplicateRequest request;
        Response response;
        uint32_t haveCount = 1;
        MetaBackUpOperate opInfo;
        while (haveCount && started_) {
            {
                std::lock_guard<std::mutex> lg(backupListLock_);
                if (backupList_.size() == 0) {
                    break;
                }
                opInfo = backupList_.front();
                backupList_.pop_front();
                haveCount = backupList_.size();
                MMC_LOG_INFO("BackupThreadFunc " << opInfo.op_);
            }
            if (metaNetServer_ == nullptr) {
                MMC_LOG_WARN("MMCMetaBackUpMgr back up net not start");
                continue;
            }

            request.key_ = opInfo.key_;
            request.op_ = opInfo.op_;
            request.blob_ = opInfo.desc_;
            Result ret = metaNetServer_->SyncCall(opInfo.desc_.rank_, request, response, 60);
            if (ret != MMC_OK) {
                MMC_LOG_ERROR("mmc meta back up failed, op " << opInfo.op_ << " rank " << opInfo.desc_.rank_
                              <<" key " << opInfo.key_);
            }
        }
    }
}

}
}