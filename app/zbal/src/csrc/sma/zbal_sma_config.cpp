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
#include <sstream>
#include <stdexcept>
#include <string>
#include "zbal_sma_config.h"

namespace zbal {
namespace sma {

static const std::string MaxSplitSizeMB = "max_split_size_mb";
static const std::string GarbageCollectionThreshold = "garbage_collection_threshold";
static const std::string SegmentSizeMB = "segment_size_mb";
static const std::string UseSMAAllocator = "use_sma_allocator";
static const std::string SmallHeapSize = "small_heap_size";
static const std::string SmallHeapThreshold = "small_heap_threshold";
static const std::string UseVMMForStaticMemory = "use_vmm_for_static_memory";

void SMAConfig::parseEnv(const char *env)
{
    // If empty, set the default values
    max_split_size_ = std::numeric_limits<size_t>::max();
    garbage_collection_threshold_ = 0;

    if (env == nullptr) {
        return;
    }

    std::vector<std::string> config;
    lexArgs(env, config);

    for (size_t i = 0; i < config.size(); i++) {
        if (config[i].compare(MaxSplitSizeMB) == 0) {
            i = parseMaxSplitSize(config, i);
        } else if (config[i].compare(GarbageCollectionThreshold) == 0) {
            i = parseGarbageCollectionThreshold(config, i);
        } else if (config[i] == UseSMAAllocator) {
            i = parseUseSMAAllocator(config, i);
        } else if (config[i] == UseVMMForStaticMemory) {
            i = parseUseVMMForStaticMemory(config, i);
        } else if (config[i] == SmallHeapSize) {
            i = parseSmallHeapSize(config, i);
        } else if (config[i] == SmallHeapThreshold) {
            i = parseSmallHeapThresHold(config, i);
        } else if (config[i] == SegmentSizeMB) {
            i = parseSegmentSizeMb(config, i);
        } else {
            ZBAL_CHECK_S(false, "Unrecognized SMAConfig option: ", config[i]);
        }

        if (i + 1 < config.size()) {
            consumeToken(config, ++i, ',');
        }
    }
}

void SMAConfig::lexArgs(const char *env, std::vector<std::string> &config)
{
    std::vector<char> buf;

    size_t env_length = strlen(env);
    for (size_t i = 0; i < env_length; i++) {
        if (env[i] == ',' || env[i] == ':' || env[i] == '[' || env[i] == ']') {
            if (!buf.empty()) {
                config.emplace_back(buf.begin(), buf.end());
                buf.clear();
            }
            config.emplace_back(1, env[i]);
        } else if (env[i] != ' ') {
            buf.emplace_back(static_cast<char>(env[i]));
        }
    }
    if (!buf.empty()) {
        config.emplace_back(buf.begin(), buf.end());
    }
}

void SMAConfig::consumeToken(const std::vector<std::string> &config, size_t i, const char c)
{
    ZBAL_CHECK_S(i < config.size() && config[i].compare(std::string(1, c)) == 0,
                 "Error parsing SMAConfig settings, expected ", c);
}

size_t SMAConfig::parseMaxSplitSize(const std::vector<std::string> &config, size_t i)
{
    consumeToken(config, ++i, ':');
    if (++i < config.size()) {
        size_t val1 = static_cast<size_t>(stoi(config[i]));
        ZBAL_CHECK_S(val1 > kLargeBuffer / (kMB),
                     "SMAConfig option max_split_size_mb too small, must be > ", kLargeBuffer / (kMB));
        val1 = std::max(val1, kLargeBuffer / (kMB));
        val1 = std::min(val1, (std::numeric_limits<size_t>::max() / (kMB)));
        max_split_size_ = val1 * kMB;
    } else {
        ZBAL_CHECK_S(false, "Error, expecting max_split_size_mb value");
    }
    return i;
}

size_t SMAConfig::parseGarbageCollectionThreshold(const std::vector<std::string> &config, size_t i)
{
    consumeToken(config, ++i, ':');
    if (++i < config.size()) {
        double val1 = stod(config[i]);
        ZBAL_CHECK_S(val1 > 0, "garbage_collect_threshold too small, set it 0.0~1.0");
        ZBAL_CHECK_S(val1 < 1.0, "garbage_collect_threshold too big, set it 0.0~1.0");
        garbage_collection_threshold_ = val1;
    } else {
        ZBAL_CHECK_S(false, "Error, expecting garbage_collection_threshold value");
    }
    return i;
}

size_t SMAConfig::parseSegmentSizeMb(const std::vector<std::string> &config, size_t i)
{
    consumeToken(config, ++i, ':');
    if (++i < config.size()) {
        size_t val = static_cast<size_t>(stoi(config[i]));
        segment_size_mb_ = val * kMB;
    } else {
        ZBAL_CHECK_S(false, "Error, expecting segment_size_mb value");
    }
    return i;
}

size_t SMAConfig::parseUseSMAAllocator(const std::vector<std::string> &config, size_t i)
{
    consumeToken(config, ++i, ':');
    if (++i < config.size()) {
        ZBAL_CHECK_S(i < config.size() && (config[i] == "True" || config[i] == "False"),
                     "Expected a single True/False argument for use_sma_allocator");
        use_sma_allocator_ = (config[i] == "True");
    } else {
        ZBAL_CHECK_S(false, "Error, expecting use_sma_allocator value");
    }
    return i;
}

size_t SMAConfig::parseUseVMMForStaticMemory(const std::vector<std::string> &config, size_t i)
{
    consumeToken(config, ++i, ':');
    if (++i < config.size()) {
        ZBAL_CHECK_S(i < config.size() && (config[i] == "True" || config[i] == "False"),
                     "Expected a single True/False argument for use_vmm_for_static_memory");
        use_vmm_for_static_memory_ = (config[i] == "True");
    } else {
        ZBAL_CHECK_S(false, "Error, expecting use_vmm_for_static_memory value");
    }
    return i;
}

size_t SMAConfig::parseSmallHeapSize(const std::vector<std::string> &config, size_t i)
{
    consumeToken(config, ++i, ':');
    if (++i < config.size()) {
        size_t val = static_cast<size_t>(stoi(config[i]));
        small_heap_size_ = val;
    } else {
        ZBAL_CHECK_S(false, "Error, expecting small_heap_size value");
    }
    return i;
}

size_t SMAConfig::parseSmallHeapThresHold(const std::vector<std::string> &config, size_t i)
{
    consumeToken(config, ++i, ':');
    if (++i < config.size()) {
        size_t val = static_cast<size_t>(stoi(config[i]));
        small_heap_threshold_ = val;
    } else {
        ZBAL_CHECK_S(false, "Error, expecting small_heap_threshold value");
    }
    return i;
}

}  // namespace sma
}  // namespace zbal
