/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MMC_BLOB_STATE_H
#define MEMFABRIC_HYBRID_MMC_BLOB_STATE_H

#include "mmc_common_includes.h"

namespace ock {
namespace mmc {
/**
 * @brief State of blob
 */
enum BlobState : uint8_t {
    INIT,
    ALLOCATED,
    DATA_WRITING,
    DATA_READY,
    COPYING,
    REMOVING,
    FINAL,
};

/**
 * @brief Block action result, which a part of transition table
 */
enum BlobActionResult : uint8_t {
    MMC_ALLOC_OK,
    MMC_ALLOC_FAIL,
    MMC_REMOVE_OK,     // directly remove
    MMC_REMOVE_START,  // need response from local
    MMC_RECV_LOC_SIGN_REMOVE_OK,
    MMC_WRITE_START,
    MMC_WRITE_OK,
    MMC_WRITE_FAIL,
    MMC_COPY_START,
    MMC_COPY_END,
    MMC_RECV_LOC_SIGN_WRITE_START,  // t o d o
    MMC_RECV_LOC_SIGN_WRITE_OK,     // t o d o
    MMC_RECV_LOC_SIGN_WRITE_FAIL,   // t o d o
    MMC_RECV_LOC_SIGN_COPY_START,   // t o d o
    MMC_RECV_LOC_SIGN_COPY_END,     // t o d o
};

}  // namespace mmc
}  // namespace ock

#endif