/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MMC_META_SERVICE_PROCESS_H
#define MMC_META_SERVICE_PROCESS_H

#include "mmc_leader_election.h"
#include "mmc_configuration.h"
#include "mmc_meta_service_default.h"


namespace ock {
namespace mmc {

class MmcMetaServiceProcess {
public:
    static MmcMetaServiceProcess& getInstance()
    {
        static MmcMetaServiceProcess meta;
        return meta;
    }
    MmcMetaServiceProcess(const MmcMetaServiceProcess&) = delete;
    MmcMetaServiceProcess& operator=(const MmcMetaServiceProcess&) = delete;

    int MainForExecutable();
    int MainForPython();

private:
    MmcMetaServiceProcess() = default;
    ~MmcMetaServiceProcess() = default;

    static bool CheckIsRunning();
    int LoadConfig();
    static void RegisterSignal();
    static void SignalInterruptHandler(int signal);
    static int InitLogger(const mmc_meta_service_config_t& options);
    void Exit();

    mmc_meta_service_config_t config_{};
    MmcMetaServiceDefault *metaService_{};
    MmcMetaServiceLeaderElection *leaderElection_{};
};

} // namespace mmc
} // namespace ock

#endif