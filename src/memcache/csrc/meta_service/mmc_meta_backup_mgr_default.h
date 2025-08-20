/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MF_HYBRID_MMC_META_BACKUP_MGR_DEFAULT_H
#define MF_HYBRID_MMC_META_BACKUP_MGR_DEFAULT_H

#include <mutex>
#include <thread>
#include <functional>
#include <list>
#include <condition_variable>
#include "mmc_ref.h"
#include "mmc_logger.h"
#include "mmc_types.h"
#include "mmc_blob_common.h"
#include "mmc_meta_net_server.h"
#include "mmc_meta_backup_mgr.h"

namespace ock {
namespace mmc {
enum BackUpOperate {
    META_BACKUP_ADD = 0,
    META_BACKUP_REMOVE = 1
};

struct MetaBackUpOperate {
    uint32_t op_;
    std::string key_;
    MmcMemBlobDesc desc_;
    MetaBackUpOperate() {}
    MetaBackUpOperate(uint32_t op, const std::string &key, MmcMemBlobDesc &desc) : op_(op), key_(key), desc_(desc) {}
};

struct MMCMetaBackUpConfDefault : public MMCMetaBackUpConf {
    MetaNetServerPtr serverPtr_;

    explicit MMCMetaBackUpConfDefault(MetaNetServerPtr serverPtr) : serverPtr_(serverPtr) {}
};
using MMCMetaBackUpConfDefaultPtr = MmcRef<MMCMetaBackUpConfDefault>;

class MMCMetaBackUpMgrDefault : public MMCMetaBackUpMgr {
public:
    explicit MMCMetaBackUpMgrDefault() {}

    ~MMCMetaBackUpMgrDefault() override
    {
        Stop();
    }

    Result Start(MMCMetaBackUpConfPtr &confPtr) override
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (started_) {
            MMC_LOG_INFO("MMCMetaBackUpMgr already started");
            return MMC_OK;
        }
        started_ = true;
        MMCMetaBackUpConfDefaultPtr defaultPtr = Convert<MMCMetaBackUpConf, MMCMetaBackUpConfDefault>(confPtr);
        metaNetServer_ = defaultPtr->serverPtr_;
        backupThread_ = std::thread(std::bind(&MMCMetaBackUpMgrDefault::BackupThreadFunc, this));
        return MMC_OK;
    }

    void Stop() override
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!started_) {
            return;
        }
        {
            std::lock_guard<std::mutex> lk(backupThreadLock_);
            started_ = false;
            backupThreadCv_.notify_all();
        }
        backupThread_.join();
        metaNetServer_ = nullptr;
        MMC_LOG_INFO("Stop MMCMetaBackUpMgr");
    }
    void BackupThreadFunc();

    Result Add(const std::string &key, MmcMemBlobDesc &blobDesc) override
    {
        {
            std::lock_guard<std::mutex> lg(backupListLock_);
            backupList_.push_back({META_BACKUP_ADD, key, blobDesc});
        }
        {
            std::lock_guard<std::mutex> lk(backupThreadLock_);
            backupThreadCv_.notify_all();
        }
        return MMC_OK;
    }

    Result Remove(const std::string &key, MmcMemBlobDesc &blobDesc) override
    {
        {
            std::lock_guard<std::mutex> lg(backupListLock_);
            backupList_.push_back({META_BACKUP_REMOVE, key, blobDesc});
        }
        {
            std::lock_guard<std::mutex> lk(backupThreadLock_);
            backupThreadCv_.notify_all();
        }
        return MMC_OK;
    }

private:
    uint32_t PopMetas2Backup(
        std::vector<uint32_t>& ops, std::vector<std::string>& keys, std::vector<MmcMemBlobDesc>& blobs);
    void SendBackup2Local();

    MetaNetServerPtr metaNetServer_;
    std::mutex mutex_;
    bool started_ = false;
    std::thread backupThread_;
    std::mutex backupThreadLock_;
    std::condition_variable backupThreadCv_;
    std::mutex backupListLock_;
    std::list<MetaBackUpOperate> backupList_;
};
}
}
#endif // MF_HYBRID_MMC_META_BACKUP_MGR_H
