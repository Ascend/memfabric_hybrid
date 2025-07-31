/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_blob_state.h"

namespace ock {
namespace mmc {
/**
 * @brief tuple struct of transition table of state machine
 */
struct StateTransitionItem {
    BlobState curState;
    Result retCode;
    BlobState nextState;
    BlobLeaseFunction function;
};

Result LeaseAdd(MmcMetaLeaseManager &leaseMgr, uint32_t rankId, uint32_t requestId)
{
    return leaseMgr.Add(rankId, requestId, MMC_DATA_TTL_MS);
}

Result LeaseRemove(MmcMetaLeaseManager &leaseMgr, uint32_t rankId, uint32_t requestId)
{
    return leaseMgr.Remove(rankId, requestId);
}

Result LeaseWait(MmcMetaLeaseManager &leaseMgr, uint32_t rankId, uint32_t requestId)
{
    if (leaseMgr.UseCount() == 0) {
        return MMC_OK;
    }
    leaseMgr.Wait();
    return MMC_OK;
}

Result LeaseExtend(MmcMetaLeaseManager &leaseMgr, uint32_t rankId, uint32_t requestId)
{
    return leaseMgr.Extend(MMC_DATA_TTL_MS);
}

/**
 * @brief State transition table of mem object meta in local service
 */
StateTransitionItem g_localStateTransItemTable[]{
    {ALLOCATED, MMC_WRITE_OK, DATA_READY, nullptr},
    {ALLOCATED, MMC_WRITE_FAIL, ALLOCATED, nullptr},
    {DATA_READY, MMC_REMOVE_OK, FINAL, nullptr},
    {ALLOCATED, MMC_REMOVE_OK, FINAL, nullptr},
};

StateTransTable BlobStateMachine::GetGlobalTransTable()
{
    StateTransTable table;
    /**
     * @brief State transition table of mem object meta in meta service
    */
    StateTransitionItem g_metaStateTransItemTable[] {
        {NONE, MMC_ALLOCATED_OK, ALLOCATED, LeaseAdd},
        {ALLOCATED, MMC_WRITE_OK, DATA_READY, LeaseRemove},
        {ALLOCATED, MMC_REMOVE_START, REMOVING, nullptr},   // data may be transfering at local; can only start remove
        {DATA_READY, MMC_READ_START, DATA_READING, LeaseAdd},   // data may be transfering at local; can only start remove
        {DATA_READING, MMC_READ_OK, DATA_READY, LeaseRemove},   // data may be transfering at local; can only start remove
        {DATA_READY, MMC_REMOVE_START, REMOVING, nullptr},  // data may be transfering at local; can only start remove
        {DATA_READING, MMC_REMOVE_START, REMOVING, LeaseWait},  // data may be transfering at local; can only start remove
        {DATA_READY, MMC_FIND_OK, DATA_READY, LeaseExtend},  // data may be transfering at local; can only start remove
        {DATA_READING, MMC_FIND_OK, DATA_READING, LeaseExtend},  // data may be transfering at local; can only start remove
        {REMOVING, MMC_REMOVE_OK, FINAL, nullptr},
    };
    for (size_t i = 0; i < sizeof(g_metaStateTransItemTable) / sizeof(StateTransitionItem); i++) {
        BlobStateAction action(g_metaStateTransItemTable[i].nextState, g_metaStateTransItemTable[i].function);
        table[g_metaStateTransItemTable[i].curState][g_metaStateTransItemTable[i].retCode] = action;
    }
    return table;
}

}  // namespace mmc
}  // namespace ock
