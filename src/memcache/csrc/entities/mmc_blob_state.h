/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MMC_BLOB_STATE_H
#define MEMFABRIC_HYBRID_MMC_BLOB_STATE_H

#include "functional"
#include "mmc_common_includes.h"
#include "mmc_meta_lease_manager.h"
namespace ock {
namespace mmc {

/**
 * @brief State of blob
 */
using BlobLeaseFunction = std::function<int32_t(MmcMetaLeaseManager &leaseMgr, uint32_t rankId, uint32_t requestId)>;

enum BlobState : uint8_t {
    NONE,
    ALLOCATED,
    DATA_READY,
    DATA_READING,
    REMOVING,
    FINAL,
};

struct BlobStateAction {
    BlobState state_ = NONE;
    BlobLeaseFunction action_ = nullptr;
    BlobStateAction(BlobState state, BlobLeaseFunction action) : state_(state), action_(action){}
    BlobStateAction() = default;
};

/**
 * @brief Block action result, which a part of transition table
 */
enum BlobActionResult : uint8_t {
    MMC_RESULT_NONE,
    MMC_REMOVE_START,
    MMC_REMOVE_OK,
    MMC_WRITE_OK,
    MMC_WRITE_FAIL,
    MMC_ALLOCATED_OK,
    MMC_READ_START,
    MMC_READ_OK,
    MMC_FIND_OK,
    MMC_RECV_LOC_SIGN_REMOVE_OK,
};

using StateTransTable = std::unordered_map<BlobState, std::unordered_map<Result , BlobStateAction>>;

class BlobStateMachine : public MmcReferable {
public:
    static StateTransTable GetGlobalTransTable();
};

}  // namespace mmc
}  // namespace ock

#endif