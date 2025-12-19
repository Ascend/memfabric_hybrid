/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#ifndef MEMFABRIC_HYBRID_MF_NUMA_UTIL_H
#define MEMFABRIC_HYBRID_MF_NUMA_UTIL_H

#include <filesystem>
#include <stdexcept>
#include <string>

namespace ock {
namespace mf {
const std::string NUMA_DIR = "/sys/devices/system/node/";
const std::string NUMA_PREFIX = "node";
class MfNumaUtil {
public:
    static int32_t GetNumaNum()
    {
        static int32_t numaNodes = -1;
        if (numaNodes >= 0) {
            return 0;
        }
        try {
            if (!std::filesystem::exists(NUMA_DIR)) {
                return 0;
            }
            int count = 0;
            for (const auto& entry : std::filesystem::directory_iterator(NUMA_DIR)) {
                std::string_view name = entry.path().filename().string();
                if (name.length() >= NUMA_PREFIX.size() && name.substr(0, NUMA_PREFIX.size()) == NUMA_PREFIX) {
                    ++count;
                }
            }
            numaNodes = count;
        } catch (const std::filesystem::filesystem_error& e) {
            numaNodes = 0;
        } catch (const std::exception& e) {
            numaNodes = 0;
        }
        return numaNodes;
    }
};
}
}

#endif // MEMFABRIC_HYBRID_MF_NUMA_UTIL_H
