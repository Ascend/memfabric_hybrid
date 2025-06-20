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
};

/**
 * @brief State transition table of mem object meta in meta service
 */
StateTransitionItem g_metaStateTransItemTable[]{
    {INIT, MMC_ALLOC_OK, ALLOCATED},
    {INIT, MMC_ALLOC_FAIL, FINAL},  // if still cannot allow in slow path(evict with thresholds), fail
    {INIT, MMC_REMOVE_OK, FINAL},   // at INIT state, can directly delete
    {ALLOCATED, MMC_RECV_LOC_SIGN_WRITE_START, DATA_WRITING},
    {ALLOCATED, MMC_RECV_LOC_SIGN_WRITE_OK, DATA_READY},
    {ALLOCATED, MMC_REMOVE_START, REMOVING},  // data may be transfering at local; can only start remove
    {DATA_WRITING, MMC_RECV_LOC_SIGN_WRITE_OK, DATA_READY},
    {DATA_WRITING, MMC_RECV_LOC_SIGN_WRITE_FAIL, ALLOCATED},
    {DATA_READY, MMC_RECV_LOC_SIGN_WRITE_OK, COPYING},
    {DATA_READY, MMC_REMOVE_START, REMOVING},           // data may be transfering at local; can only start remove
    {COPYING, MMC_RECV_LOC_SIGN_COPY_END, DATA_READY},  // whether success or not, still end at data ready state
    {REMOVING, MMC_RECV_LOC_SIGN_REMOVE_OK, FINAL},
};

/**
 * @brief State transition table of mem object meta in local service
 */
StateTransitionItem g_localStateTransItemTable[]{
    {INIT, MMC_ALLOC_OK, ALLOCATED},  // not actually alloc, just copy the info from meta serv
    {INIT, MMC_REMOVE_OK, FINAL},     // can remove
    {ALLOCATED, MMC_WRITE_START, DATA_WRITING},
    {ALLOCATED, MMC_REMOVE_OK, FINAL},  // can remove (cannot remove from data writing)
    {DATA_WRITING, MMC_WRITE_OK, DATA_READY},
    {DATA_WRITING, MMC_WRITE_FAIL, ALLOCATED},
    {DATA_READY, MMC_COPY_START, COPYING},
    {DATA_READY, MMC_REMOVE_OK, FINAL},  // can remove (cannot remove from data copying)
    {COPYING, MMC_COPY_END, DATA_READY},
};

}  // namespace mmc
}  // namespace ock
