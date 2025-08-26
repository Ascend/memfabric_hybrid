/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_DL_API_H
#define MEM_FABRIC_HYBRID_DL_API_H

#include <string>
#include "hybm_types.h"

namespace ock {
namespace mf {

class DlApi {
public:
    static Result LoadLibrary(const std::string &libDirPath, uint64_t flags = 0);
    static void CleanupLibrary();
};
}
}

#endif  // MEM_FABRIC_HYBRID_DL_API_H
