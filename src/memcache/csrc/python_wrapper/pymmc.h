/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MMC_PYMMC_H
#define MMC_PYMMC_H

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <csignal>
#include <mutex>
#include <string>
#include <unordered_set>
#include <sstream>

#include "mmc_def.h"
#include "mmc_types.h"
#include "mmcache.h"
#include "mmcache_store.h"

using namespace ock::mmc;

// Forward declarations
class SliceBuffer;

/**
 * @brief A class that holds a contiguous buffer of data
 * This class is responsible for freeing the buffer when it's destroyed (RAII)
 */
class SliceBuffer {
public:
    /**
     * @brief Construct a new SliceBuffer object with contiguous memory
     * @param store Reference to the MmcacheStore that owns the
     * allocator
     * @param buffer Pointer to the contiguous buffer
     * @param size Size of the buffer in bytes
     * @param use_allocator_free If true, use SimpleAllocator to free the
     * buffer, otherwise use delete[]
     */
    SliceBuffer(MmcacheStore &store, void *buffer, uint64_t size, bool use_allocator_free = true);

    /**
     * @brief Destructor that frees the buffer
     */
    ~SliceBuffer();

    /**
     * @brief Get a pointer to the data
     * @return void* Pointer to the dat
     */
    void *ptr() const;

    /**
     * @brief Get the size of the data
     * @return uint64_t Size of the data in bytes
     */
    uint64_t size() const;

private:
    MmcacheStore &store_;
    void *buffer_;
    uint64_t size_;
    bool use_allocator_free_;  // Flag to control deallocation method
};

#endif
