/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* MemFabric_Hybrid is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/
#include "dl_op_api.h"

#include <dlfcn.h>

namespace ock {
namespace mf {
bool DlOpApi::gLoaded = false;
std::mutex DlOpApi::gMutex;
void *DlOpApi::opapiHandle;
void *DlOpApi::opbaseHandle;
const char *DlOpApi::gOpapiLibName = "libopapi.so";
const char *DlOpApi::gOpBaseLibName = "libnnopbase.so";

aclnnShmemSdmaStarsQueryGetWorkspaceSizeFunc DlOpApi::pAclnnShmemSdmaStarsQueryGetWorkspaceSize = nullptr;
aclnnShmemSdmaStarsQueryFunc DlOpApi::pAclnnShmemSdmaStarsQuery = nullptr;

aclCreateTensorFunc DlOpApi::pAclCreateTensor = nullptr;
aclDestroyTensorFunc DlOpApi::pAclDestroyTensor = nullptr;

Result DlOpApi::LoadLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }

    /* dlopen library */
    opapiHandle = dlopen(gOpapiLibName, RTLD_NOW);
    if (opapiHandle == nullptr) {
        BM_LOG_ERROR("Failed to open library error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    /* load sym */
    DL_LOAD_SYM(pAclnnShmemSdmaStarsQueryGetWorkspaceSize, aclnnShmemSdmaStarsQueryGetWorkspaceSizeFunc, opapiHandle,
                "aclnnShmemSdmaStarsQueryGetWorkspaceSize");
    DL_LOAD_SYM(pAclnnShmemSdmaStarsQuery, aclnnShmemSdmaStarsQueryFunc, opapiHandle, "aclnnShmemSdmaStarsQuery");

    opbaseHandle = dlopen(gOpBaseLibName, RTLD_NOW);
    if (opbaseHandle == nullptr) {
        BM_LOG_ERROR("Failed to open library error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    /* load sym */
    DL_LOAD_SYM(pAclCreateTensor, aclCreateTensorFunc, opbaseHandle, "aclCreateTensor");
    DL_LOAD_SYM(pAclDestroyTensor, aclDestroyTensorFunc, opbaseHandle, "aclDestroyTensor");

    gLoaded = true;
    return BM_OK;
}

void DlOpApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }

    pAclnnShmemSdmaStarsQueryGetWorkspaceSize = nullptr;
    pAclnnShmemSdmaStarsQuery = nullptr;
    pAclCreateTensor = nullptr;
    pAclDestroyTensor = nullptr;

    if (opapiHandle != nullptr) {
        dlclose(opapiHandle);
        opapiHandle = nullptr;
    }

    gLoaded = false;
}
} // namespace mf
} // namespace ock