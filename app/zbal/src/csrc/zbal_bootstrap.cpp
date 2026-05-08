/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "zbal_common_includes.h"
#include "zbal_bootstrap_default.h"
#include "zbal_init_state.h"
#include "zbal_mem_allocator.h"

using namespace zbal;
using namespace zbal::bootstrap;

#ifdef __cplusplus
extern "C" {
#endif

ZBAL_API int32_t zbal_bootstrap_options_init(zbal_bootstrap_options_t *options)
{
    ZBAL_VALIDATE_RETURN(options != nullptr, "invalid param, bootstrap options should not be null", Z_INVALID_PARAM);

    bzero(options, sizeof(zbal_bootstrap_options_t));
    options->btType = BOOT_BY_MEMFABRIC;
    options->startConfigServer = 0;
    options->commMetaSpaceSize = COMM_META_SPACE_SIZE_DEFAULT;
    options->commGroupCap = COMM_GROUP_COUNT_CAP_DEFAULT;

    return Z_OK;
}

ZBAL_API int32_t zbal_bootstrap(zbal_bootstrap_options_t *options, zbal_bootstrap_output_t *output)
{
    ZBAL_VALIDATE_RETURN(options != nullptr, "invalid param, bootstrap options should not be null", Z_INVALID_PARAM);
    ZBAL_VALIDATE_RETURN(output != nullptr, "invalid param, bootstrap output should not be null", Z_INVALID_PARAM);

    ZBAL_LOG_INFO("options dump, " << (*options));

    auto bootstrap = Bootstrap::Create(*options);
    if (bootstrap == nullptr) {
        return Z_ERROR;
    }

    /* get and set output */
    auto &out = bootstrap->GetOutput();
    memcpy(output, &out, sizeof(zbal_bootstrap_output_t));

    /* set init state */
    auto &state = ZBALInitState::Instance();
    state.Bootstrapped(true);
    state.ext_.btType = options->btType;
    state.ext_.worldSize = options->worldSize;
    state.ext_.worldRankId = options->rankId;
    state.ext_.deviceId = options->deviceId;
    state.ext_.commMetaSpaceSize = options->commMetaSpaceSize;
    state.ext_.commGroupCap = options->commGroupCap;
    state.ext_.gvaDevice = output->deviceGva;
    state.ext_.myCommMetaDeviceGva = output->myCommMetaDeviceGva;
    state.ext_.metaSizeOfDevice = output->metaSizeOfDevice;
    state.ext_.mySMAGva = output->mySMAGva;
    state.ext_.smaSizeOfDevice = output->smaSizeOfDevice;
    state.ext_.localDeviceMemSize = output->allocatedDeviceMemorySize;
    state.ext_.symmetricMemSpace = output->createdDeviceMemorySpaceSize;

    /* set allocator state */
    zbal_allocator_options_t alloc_state;
    alloc_state.gva = output->deviceGva;
    alloc_state.myGva = output->mySMAGva;
    alloc_state.size = output->smaSizeOfDevice;
    // flag currently is just a placeholder
    ZBAL_VALIDATE_RETURN(zbal_sma_init(&alloc_state, 0) == Z_OK, "sma init failed", Z_INVALID_PARAM);

    ZBAL_LOG_DEBUG("zbal bootstrap success, " << *output);
    return Z_OK;
}

ZBAL_API int32_t zbal_unbootstrap(uint32_t flags)
{
    /* check if sma and comm still there */
    if (ZBALInitState::Instance().HasCommunicator()) {
        ZBAL_LOG_AND_SET_LAST_ERROR(
            "Cannot un-bootstrap as there are still communicators existed, destroy all communicators firstly");
        return Z_CANNOT_UNBOOTSTRAP;
    } else if (ZBALInitState::Instance().SmaInitialized()) {
        ZBAL_LOG_AND_SET_LAST_ERROR(
            "Cannot un-bootstrap as secondary memory allocator is not un-initialized, un-initialize it firstly");
        return Z_CANNOT_UNBOOTSTRAP;
    }

    Bootstrap::Destroy();
    ZBALInitState::Instance().Reset();
    ZBAL_LOG_INFO("ZBAL un-bootstrap successfully");
    return Z_OK;
}

#ifdef __cplusplus
}
#endif