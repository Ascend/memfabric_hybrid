/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_DL_API_H
#define MEM_FABRIC_HYBRID_DL_API_H

#include <string>
#include "hybm_types.h"

namespace ock {
namespace mf {

enum DlApiExtendLibraryType {
  DL_EXT_LIB_DEVICE_RDMA,
  DL_EXT_LIB_HOST_RDMA,
};

class DlApi {
public:
    static Result LoadLibrary(const std::string &libDirPath);
    static void CleanupLibrary();
    static Result LoadExtendLibrary(DlApiExtendLibraryType libraryType);
};
}
}

#endif  // MEM_FABRIC_HYBRID_DL_API_H
