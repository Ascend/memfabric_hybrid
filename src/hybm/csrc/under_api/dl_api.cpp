/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "dl_api.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "dl_hccp_api.h"
#include "dl_hcom_api.h"

namespace ock {
namespace mf {

Result DlApi::LoadLibrary(const std::string &libDirPath, const uint32_t gvaVersion)
{
    auto result = DlAclApi::LoadLibrary(libDirPath);
    if (result != BM_OK) {
        return result;
    }
#ifdef USE_CANN
    result = DlHalApi::LoadLibrary(gvaVersion);
    if (result != BM_OK) {
        DlAclApi::CleanupLibrary();
        return result;
    }

    result = DlHccpApi::LoadLibrary();
    if (result != BM_OK) {
        DlHalApi::CleanupLibrary();
        DlAclApi::CleanupLibrary();
        return result;
    }
#endif

    DlHcomApi::LoadLibrary();
    return BM_OK;
}

void DlApi::CleanupLibrary()
{
    DlHccpApi::CleanupLibrary();
    DlAclApi::CleanupLibrary();
    DlHalApi::CleanupLibrary();
    DlHcomApi::CleanupLibrary();
}
}
}