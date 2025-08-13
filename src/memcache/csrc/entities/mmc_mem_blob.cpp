
#include "mmc_mem_blob.h"
namespace ock {
namespace mmc {

const StateTransTable MmcMemBlob::stateTransTable_ = BlobStateMachine::GetGlobalTransTable();

Result MmcMemBlob::UpdateState(const std::string &key, uint32_t rankId, uint32_t operateId, BlobActionResult ret)
{
    auto curStateIter = stateTransTable_.find(state_);
    if (curStateIter == stateTransTable_.end()) {
        MMC_LOG_ERROR("Cannot update state! The current state is not in the stateTransTable!");
        return MMC_UNMATCHED_STATE;
    }

    const auto retIter = curStateIter->second.find(ret);
    if (retIter == curStateIter->second.end()) {
        MMC_LOG_INFO("cannot find " << std::to_string(ret) << " from " << std::to_string(state_));

        return MMC_UNMATCHED_RET;
    }

    MMC_LOG_INFO("update [" << this << "] state from " << std::to_string(state_) << " to ("
                            << std::to_string(retIter->second.state_) << ")");

    if (state_ == ALLOCATED && ret == MMC_WRITE_OK) {
        MMC_RETURN_ERROR(Backup(key), "memBlob remove use client error");
    }

    state_ = retIter->second.state_;
    if (retIter->second.action_) {
        auto res = retIter->second.action_(metaLeaseManager_, rankId, operateId);
        if (res != MMC_OK) {
            MMC_LOG_ERROR("Blob update current state is " << std::to_string(state_) << " by ret(" << std::to_string(ret)
                                                          << ") failed! res=" << res);
            return res;
        }
    }
    return MMC_OK;
}

Result MmcMemBlob::Backup(const std::string &key)
{
    MMCMetaBackUpMgrPtr mmcBackupPtr = MMCMetaBackUpMgrFactory::GetInstance("DefaultMetaBackup");
    if (mmcBackupPtr == nullptr) {
        MMC_LOG_ERROR("key " << key << " meta back up failed");
        return MMC_META_BACKUP_ERROR;
    }
    MmcMemBlobDesc desc = GetDesc();
    MMC_LOG_DEBUG("Backup add " << key << " rank " << desc.rank_);
    return mmcBackupPtr->Add(key, desc);
}

Result MmcMemBlob::BackupRemove(const std::string &key)
{
    MMCMetaBackUpMgrPtr mmcBackupPtr = MMCMetaBackUpMgrFactory::GetInstance("DefaultMetaBackup");
    if (mmcBackupPtr == nullptr) {
        MMC_LOG_ERROR("key " << key << " meta back up failed");
        return MMC_META_BACKUP_ERROR;
    }
    MmcMemBlobDesc desc = GetDesc();
    MMC_LOG_DEBUG("Backup remove " << key << " rank " << desc.rank_);
    return mmcBackupPtr->Remove(key, desc);
}

}  // namespace mmc
}  // namespace ock