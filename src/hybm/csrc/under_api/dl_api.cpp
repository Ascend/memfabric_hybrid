/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
        DlApi::CleanupLibrary();
        return result;
    }

    result = DlHalApi::LoadLibrary();
    if (result != BM_OK) {
        DlApi::CleanupLibrary();
        return result;
    }

    return BM_OK;
}

void DlApi::CleanupLibrary()
{
    DlAclApi::CleanupLibrary();
    DlHalApi::CleanupLibrary();
}

}
}