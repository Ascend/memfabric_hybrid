/* $$$!!Warning: Huawei key information asset. No spread without permission.$$$ */
/* CODEMARK:RKeR1B8WMAfemkt1tTDGp4eOEddgxKn4NOPmdw0w+6Q3n1pxgDEX+kGBiRV20e1NKuLwOh60qWwx
7DOUvTqsDhlXzSmTU10bmKROYG5QSBH67ZQ46xlhYXj92YNPIRXSYvFanWl+2cOM7aqhteKYQCYv
nSLTJ0TxsPPiIs0cMyuYI5fbIhqP4Jm6IFum9q+7oM6p8pm5t5DXHbhq6e7P7YyJSdtp3vT0YLrF
KJQG82dXD8XgEGsf9FQfJHnlFHnrry7pkfBjddjKikKztx9fuldWAvSbjEUvgEEaaP8BS22bsw9J
NWtxRcrHiW0xLOsR# */
/* $$$!!Warning: Deleting or modifying the preceding information is prohibited.$$$ */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "dl_api.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "dl_hccl_api.h"
#include "dl_hccp_api.h"

namespace ock {
namespace mf {

Result DlApi::LoadLibrary(const std::string &libDirPath)
{
    auto result = DlAclApi::LoadLibrary(libDirPath);
    if (result != BM_OK) {
        return result;
    }

    result = RuntimeHalApi::LoadLibrary();
    if (result != BM_OK) {
        return result;
    }

    result = RuntimeHcclApi::LoadLibrary();
    if (result != BM_OK) {
        return result;
    }

    result = RuntimeHccpApi::LoadLibrary();
    if (result != BM_OK) {
        return result;
    }

    return BM_OK;
}
}
}