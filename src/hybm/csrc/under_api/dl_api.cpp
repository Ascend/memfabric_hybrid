/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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

Result DlApi::LoadExtendLibrary(DlApiExtendLibraryType libraryType)
{
    if (libraryType == DlApiExtendLibraryType::DL_EXT_LIB_DEVICE_RDMA) {
        return DlHccpApi::LoadLibrary();
    }

    if (libraryType == DlApiExtendLibraryType::DL_EXT_LIB_HOST_RDMA) {
        return DlHcomApi::LoadLibrary();
    }

    return BM_OK;
}
}
}