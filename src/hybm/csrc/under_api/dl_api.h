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
#ifndef MEM_FABRIC_HYBRID_DL_API_H
#define MEM_FABRIC_HYBRID_DL_API_H

#include <string>
#include "hybm_types.h"

namespace ock {
namespace mf {

enum class DlApiExtendLibraryType { DL_EXT_LIB_DEVICE_RDMA, DL_EXT_LIB_HOST_RDMA };

class DlApi {
public:
    static Result LoadLibrary(const std::string &libDirPath, uint32_t gvaVersion);
    static void CleanupLibrary();
    static Result LoadExtendLibrary(DlApiExtendLibraryType libraryType);
};
} // namespace mf
} // namespace ock

#endif // MEM_FABRIC_HYBRID_DL_API_H
