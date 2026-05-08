/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef ZBAL_SMA_CONFIG_H
#define ZBAL_SMA_CONFIG_H

#include "zbal_sma_common.h"
#include "zbal_common_includes.h"

namespace zbal {
namespace sma {

const char* PytorchNPUAllocConf = "ZBAL_NPU_ALLOC_CONF";

class SMAConfig {
public:
    static size_t max_split_size()
    {
        return instance().max_split_size_;
    }

    static double garbage_collection_threshold()
    {
        return instance().garbage_collection_threshold_;
    }

    static size_t segment_size_mb()
    {
        return instance().segment_size_mb_;
    }

    static bool use_sma_allocator()
    {
        return instance().use_sma_allocator_;
    }

    static bool use_vmm_for_static_memory()
    {
        return instance().use_vmm_for_static_memory_;
    }

    static size_t small_heap_size()
    {
        return instance().small_heap_size_;
    }

    static size_t small_heap_threshold()
    {
        return instance().small_heap_threshold_;
    }

    static SMAConfig &instance() noexcept
    {
        static SMAConfig *s_instance = ([]() {
            auto inst = new SMAConfig();
            const char *env = getenv(PytorchNPUAllocConf);
            inst->parseEnv(env);
            return inst;
        })();
        return *s_instance;
    }

private:
    size_t max_split_size_;
    double garbage_collection_threshold_;
    size_t segment_size_mb_;
    bool use_sma_allocator_;
    bool use_vmm_for_static_memory_;
    size_t small_heap_size_;
    size_t small_heap_threshold_;

    SMAConfig(): max_split_size_(std::numeric_limits<size_t>::max()),
        garbage_collection_threshold_(0),
        segment_size_mb_(kSmallBuffer),
        use_sma_allocator_(true),
        use_vmm_for_static_memory_(false),
        small_heap_size_(kSmallHeapSize),
        small_heap_threshold_(kSmallThreshold)
    {}

    void parseEnv(const char *env);

    void lexArgs(const char *env, std::vector<std::string> &config);
    void consumeToken(const std::vector<std::string> &config, size_t i, const char c);

    size_t parseMaxSplitSize(const std::vector<std::string> &config, size_t i);
    size_t parseGarbageCollectionThreshold(const std::vector<std::string> &config, size_t i);
    size_t parseSegmentSizeMb(const std::vector<std::string> &config, size_t i);
    size_t parseUseSMAAllocator(const std::vector<std::string> &config, size_t i);
    size_t parseUseVMMForStaticMemory(const std::vector<std::string> &config, size_t i);
    size_t parseSmallHeapSize(const std::vector<std::string> &config, size_t i);
    size_t parseSmallHeapThresHold(const std::vector<std::string> &config, size_t i);
};
}  // namespace sma
}  // namespace zbal

#endif  // ZBAL_SMA_CONFIG_H
