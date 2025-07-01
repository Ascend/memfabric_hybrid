/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "dl_api.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"

namespace ock {
namespace mf {

Result DlApi::LoadLibrary(const std::string &libDirPath)
{
    auto result = DlAclApi::LoadLibrary(libDirPath);
    if (result != BM_OK) {
        return result;
    }

    result = DlHalApi::LoadLibrary();
    if (result != BM_OK) {
        return result;
    }

    return BM_OK;
}
}
}