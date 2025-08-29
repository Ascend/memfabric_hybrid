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

Result DlApi::LoadLibrary(const std::string &libDirPath)
{
    auto result = DlAclApi::LoadLibrary(libDirPath);
    if (result != BM_OK) {
        return result;
    }

    result = DlHalApi::LoadLibrary();
    if (result != BM_OK) {
        DlAclApi::CleanupLibrary();
        return result;
    }

    return BM_OK;
}

void DlApi::CleanupLibrary()
{
    DlHccpApi::CleanupLibrary();
    DlAclApi::CleanupLibrary();
    DlHalApi::CleanupLibrary();
    DlHcomApi::CleanupLibrary();
}

Result DlApi::LoadExtendLibrary(DlApiExtendLibraryType libraryType)
{
    if (libraryType == DL_EXT_LIB_DEVICE_RDMA) {
        return DlHccpApi::LoadLibrary();
    }

    if (libraryType == DL_EXT_LIB_HOST_RDMA) {
        return DlHcomApi::LoadLibrary();
    }

    return BM_OK;
}

}
}