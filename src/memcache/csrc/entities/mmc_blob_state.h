/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MMC_BLOB_STATE_H
#define MEMFABRIC_HYBRID_MMC_BLOB_STATE_H

#include <utility>

#include "functional"
#include "mmc_common_includes.h"
#include "mmc_meta_lease_manager.h"
namespace ock {
namespace mmc {

/**
* @brief State of blob
*/
using BlobLeaseFunction = std::function<Result(MmcMetaLeaseManager &leaseMgr, uint32_t rankId, uint32_t requestId)>;

/**
 state machine:
NONE -----> ALLOCATED -----------
                |               |
                |               |
                |               |
            READABLE -----> REMOVING -----> NONE
*/
enum BlobState : uint8_t {
    ALLOCATED,
    READABLE,
    REMOVING,
    NONE,
};

struct BlobStateAction {
    BlobState state_ = NONE;
    BlobLeaseFunction action_ = nullptr;
    BlobStateAction(BlobState state, BlobLeaseFunction action) : state_(state), action_(std::move(action)){}
    BlobStateAction() = default;
};

/**
 * @brief Block action result, which a part of transition table
 */
enum BlobActionResult : uint8_t {
    MMC_ALLOCATED_OK,  // alloc complete

    MMC_WRITE_OK,
    MMC_WRITE_FAIL,

    MMC_READ_START,
    MMC_READ_FINISH,

    MMC_REMOVE_START
};

using StateTransTable = std::unordered_map<BlobState, std::unordered_map<Result , BlobStateAction>>;

class BlobStateMachine : public MmcReferable {
public:
    static StateTransTable GetGlobalTransTable();
};

}  // namespace mmc
}  // namespace ock

#endif