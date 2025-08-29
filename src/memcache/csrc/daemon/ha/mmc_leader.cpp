/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc_leader_election.h"
#include "mmc_env.h"
#include "mmc_logger.h"
#include "mmc_types.h"


ock::mmc::MmcMetaServiceLeaderElection *leaderElection = nullptr;

extern "C" {
void mmc_logger(int level, const char *msg)
{
    switch (level) {
        case ock::mmc::DEBUG_LEVEL: MMC_LOG_DEBUG(msg); break;
        case ock::mmc::INFO_LEVEL: MMC_LOG_INFO(msg); break;
        case ock::mmc::WARN_LEVEL: MMC_LOG_WARN(msg); break;
        case ock::mmc::ERROR_LEVEL: MMC_LOG_ERROR(msg); break;
        default: break;
    }
}

int mmc_leader_init()
{
    leaderElection = new (std::nothrow) ock::mmc::MmcMetaServiceLeaderElection(
        "leader_election", ock::mmc::META_POD_NAME, ock::mmc::META_NAMESPACE, ock::mmc::META_LEASE_NAME);
    pybind11::gil_scoped_release release;
    if (leaderElection == nullptr || leaderElection->Start() != 0) {
        std::cerr << "Error, failed to start meta service leader election." << std::endl;
        return -1;
    }

    return 0;
}

void mmc_leader_uninit()
{
    if (leaderElection) {
        leaderElection->Stop();
    }
}

void mmc_leader_set_logger(ExternalLog func)
{
    ock::mmc::MmcOutLogger::Instance().SetExternalLogFunction(func);
}
}