/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "mmc_meta_backup_mgr_factory.h"
#include "mmc_meta_backup_mgr_default.h"
#include "mmc_msg_client_meta.h"

namespace ock {
namespace mmc {
constexpr uint32_t BATCH_BACKUP_SIZE = 1024;

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

        SendBackup2Local();
    }
}

void MMCMetaBackUpMgrDefault::SendBackup2Local()
{
    MetaReplicateRequest request;
    Response response;
    uint32_t haveCount = 1;
    std::vector<uint32_t> ops;
    std::vector<std::string> keys;
    std::vector<MmcMemBlobDesc> blobs;
    uint32_t rank;
    while (haveCount && started_) {
        {
            std::lock_guard<std::mutex> lg(backupListLock_);
            rank = PopMetas2Backup(ops, keys, blobs);
            haveCount = backupList_.size();
            MMC_LOG_DEBUG("BackupThreadFunc bm rank=" << rank);
        }
        if (metaNetServer_ == nullptr) {
            MMC_LOG_WARN("MMCMetaBackUpMgr back up net not start");
            continue;
        }

        if (!keys.empty()) {
            request.ops_ = std::move(ops);
            request.keys_ = std::move(keys);
            request.blobs_ = std::move(blobs);
            Result ret = metaNetServer_->SyncCall(rank, request, response, 60);
            if (ret != MMC_OK) {
                MMC_LOG_ERROR("mmc meta back up failed, bm rank " << rank << ", keys: " << request.KeysString());
            }
        }
    }
}

uint32_t MMCMetaBackUpMgrDefault::PopMetas2Backup(
    std::vector<uint32_t>& ops, std::vector<std::string>& keys, std::vector<MmcMemBlobDesc>& blobs)
{
    // 清空数组
    ops.clear();
    keys.clear();
    blobs.clear();

    // 获取目标bm rank
    uint32_t rank = UINT32_MAX;
    if (!backupList_.empty()) {
        rank = backupList_.front().desc_.rank_;
    }

    // 根据目标bm rank，尝试获取1024个待备份meta对象
    uint32_t count = 0;
    auto it = backupList_.begin();
    while (it != backupList_.end()) {
        const auto& opInfo = *it;
        if (opInfo.desc_.rank_ == rank) {
            ops.push_back(opInfo.op_);
            keys.push_back(opInfo.key_);
            blobs.push_back(opInfo.desc_);
            it = backupList_.erase(it);
            count++;
        } else {
            ++it;
        }
        if (count >= BATCH_BACKUP_SIZE) {
            break;
        }
    }
    return rank;
}

}
}