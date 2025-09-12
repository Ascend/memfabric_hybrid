/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc_blob_state.h"
#include "mmc_mem_blob.h"

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

StateTransTable BlobStateMachine::GetGlobalTransTable()
{
    StateTransTable table;
    /**
     * @brief State transition table of mem object meta in meta service
    */
    StateTransitionItem g_metaStateTransItemTable[] {
        {ALLOCATED, MMC_ALLOCATED_OK, ALLOCATED, LeaseAdd}, // prepare write，add <client, reqid> --> lease
        {ALLOCATED, MMC_WRITE_OK, READABLE, LeaseRemove},   // write ok,    remove <client, reqid> --> lease
        {ALLOCATED, MMC_WRITE_FAIL, REMOVING, LeaseRemove}, // write fail,  remove <client, reqid> --> lease

        {ALLOCATED, MMC_REMOVE_START, REMOVING, nullptr},   // remove blob

        {READABLE, MMC_READ_START, READABLE, LeaseAdd},     // prepare read，add <client, reqid> --> lease
        {READABLE, MMC_READ_FINISH, READABLE, LeaseRemove}, // read finish, remove <client, reqid> --> lease
        {READABLE, MMC_REMOVE_START, REMOVING, LeaseWait},  // remove blob, wait all lease timeout or no lease
    };
    for (size_t i = 0; i < sizeof(g_metaStateTransItemTable) / sizeof(StateTransitionItem); i++) {
        BlobStateAction action(g_metaStateTransItemTable[i].nextState, g_metaStateTransItemTable[i].function);
        table[g_metaStateTransItemTable[i].curState][g_metaStateTransItemTable[i].retCode] = action;
    }
    return table;
}

}  // namespace mmc
}  // namespace ock
