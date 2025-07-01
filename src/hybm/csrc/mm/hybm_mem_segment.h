/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_MEM_SEGMENT_H
#define MEM_FABRIC_HYBRID_HYBM_MEM_SEGMENT_H

#include <memory>
#include <vector>
#include "hybm_common_include.h"
#include "hybm_mem_common.h"
#include "hybm_mem_slice.h"

namespace ock {
namespace mf {
class MemSegment;
using MemSegmentPtr = std::shared_ptr<MemSegment>;

class MemSegment {
public:
    static MemSegmentPtr Create(MemSegType segType, const MemSegmentOptions &options, int entityId);

public:
    explicit MemSegment(const MemSegmentOptions &options, int eid) : options_{options}, entityId_{eid} {}

    /*
     * Validate options
     * @return 0 if ok
     */
    virtual Result ValidateOptions() noexcept = 0;

    virtual Result PrepareVirtualMemory(uint32_t rankNo, uint32_t rankCnt, void **address) noexcept = 0;

    /*
     * Allocate memory according to segType
     * @return 0 if successful
     */
    virtual Result AllocMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept = 0;

    /*
     * Export exchange info according to infoExType
     * @return exchange info
     */
    virtual Result Export(std::string &exInfo) noexcept = 0;

    virtual Result Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept = 0;

    /*
     * Import exchange info and translate it into data structure
     * @param allExInfo
     */
    virtual Result Import(const std::vector<std::string> &allExInfo) noexcept = 0;

    virtual Result Mmap() noexcept = 0;

    virtual Result Start() noexcept = 0;

    virtual Result Stop() noexcept = 0;

    virtual Result Join(uint32_t rank) noexcept = 0;

    virtual Result Leave(uint32_t rank) noexcept = 0;

    virtual std::shared_ptr<MemSlice> GetMemSlice(hybm_mem_slice_t slice) const noexcept = 0;

    virtual bool MemoryInRange(const void *begin, uint64_t size) const noexcept = 0;

protected:
    const MemSegmentOptions options_;
    const int entityId_;
};
}
}

#endif // MEM_FABRIC_HYBRID_HYBM_MEM_SEGMENT_H
