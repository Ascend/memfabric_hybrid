/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MEM_FABRIC_HYBRID_DL_API_H
#define MEM_FABRIC_HYBRID_DL_API_H

#include <string>
#include "hybm_types.h"

namespace ock {
namespace mf {

enum class DlApiExtendLibraryType {
    DL_EXT_LIB_DEVICE_RDMA,
    DL_EXT_LIB_HOST_RDMA
};

class DlApi {
public:
    static Result LoadLibrary(const std::string &libDirPath, uint32_t gvaVersion);
    static void CleanupLibrary();
    static Result LoadExtendLibrary(DlApiExtendLibraryType libraryType);
};
}
}

#endif  // MEM_FABRIC_HYBRID_DL_API_H
