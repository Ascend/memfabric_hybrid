/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <dlfcn.h>
#include "hybm_core_api.h"

namespace ock {
namespace smem {

#ifndef UT_ENABLED // skip init in ut_test
    bool HybmCoreApi::gLoaded = false;
    std::mutex HybmCoreApi::gMutex;
    void *HybmCoreApi::coreHandle = nullptr;
    const char *HybmCoreApi::gHybmCoreLibName = "libmf_hybm_core.so";

    hybmInitFunc HybmCoreApi::pHybmInit = nullptr;
    hybmUninitFunc HybmCoreApi::pHybmUninit = nullptr;
    hybmSetLoggerFunc HybmCoreApi::pHybmSetLogger = nullptr;
    hybmSetLogLevelFunc HybmCoreApi::pHybmSetLogLevel = nullptr;
    hybmGetErrorFunc HybmCoreApi::pHybmGetError = nullptr;

    hybmCreateEntityFunc HybmCoreApi::pHybmCreateEntity = nullptr;
    hybmDestroyEntityFunc HybmCoreApi::pHybmDestroyEntity = nullptr;
    hybmReserveMemFunc HybmCoreApi::pHybmReserveMem = nullptr;
    hybmUnreserveMemFunc HybmCoreApi::pHybmUnreserveMem = nullptr;
    hybmAllocLocalMemFunc HybmCoreApi::pHybmAllocLocalMem = nullptr;
    hybmFreeLocalMemFunc HybmCoreApi::pHybmFreeLocalMem = nullptr;
    hybmExportFunc HybmCoreApi::pHybmExport = nullptr;
    hybmImportFunc HybmCoreApi::pHybmImport = nullptr;
    hybmSetExtraContextFunc HybmCoreApi::pHybmSetExtraContext = nullptr;
    hybmStartFunc HybmCoreApi::pHybmStart = nullptr;
    hybmStopFunc HybmCoreApi::pHybmStop = nullptr;
    hybmMmapFunc HybmCoreApi::pHybmMmap = nullptr;
    hybmJoinFunc HybmCoreApi::pHybmJoin = nullptr;
    hybmLeaveFunc HybmCoreApi::pHybmLeave = nullptr;
    hybmDataCopyFunc HybmCoreApi::pHybmDataCopy = nullptr;
    hybmTransportInitFunc HybmCoreApi::gHybmTransportInit = nullptr;
    hybmTransportGetAddressFunc HybmCoreApi::gHybmTransportGetAddress = nullptr;
    hybmTransportSetAddressesFunc HybmCoreApi::gHybmTransportSetAddresses = nullptr;
    hybmTransportSetAddressesFunc HybmCoreApi::gHybmTransportMakeConnections = nullptr;
    hybmTransportAiQpInfoAddressFunc HybmCoreApi::gHybmTransportAiQpInfoAddress = nullptr;
#endif

#define DL_LOAD_SYM(TARGET_FUNC_VAR, TARGET_FUNC_TYPE, FILE_HANDLE, SYMBOL_NAME)          \
    do {                                                                                  \
        TARGET_FUNC_VAR = (TARGET_FUNC_TYPE)dlsym(FILE_HANDLE, SYMBOL_NAME);              \
        if (TARGET_FUNC_VAR == nullptr) {                                                 \
            SM_LOG_ERROR("Failed to call dlsym to load SYMBOL_NAME, error" << dlerror()); \
            dlclose(FILE_HANDLE);                                                         \
            return SM_ERROR;                                                              \
        }                                                                                 \
    } while (0)

Result HybmCoreApi::LoadLibrary(const std::string &libDirPath)
{
    SM_LOG_DEBUG("try to load library: " << gHybmCoreLibName << ", dir: " << libDirPath);
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return SM_OK;
    }

    std::string realPath;
    if (!libDirPath.empty()) {
        auto ret = CommonFunc::LibraryRealPath(libDirPath, std::string(gHybmCoreLibName), realPath);
        if (ret != SM_OK) {
            SM_LOG_ERROR(libDirPath << "get lib path failed, ret: " << ret);
        }
    } else {
        realPath = std::string(gHybmCoreLibName);
    }

    /* dlopen library */
    coreHandle = dlopen(realPath.c_str(), RTLD_NOW);
    if (coreHandle == nullptr) {
        SM_LOG_ERROR("open library failed, path: " << realPath << ", error: " << dlerror());
        return SM_ERROR;
    }

    /* load sym */
    DL_LOAD_SYM(pHybmInit, hybmInitFunc, coreHandle, "hybm_init");
    DL_LOAD_SYM(pHybmUninit, hybmUninitFunc, coreHandle, "hybm_uninit");
    DL_LOAD_SYM(pHybmSetLogger, hybmSetLoggerFunc, coreHandle, "hybm_set_extern_logger");
    DL_LOAD_SYM(pHybmSetLogLevel, hybmSetLogLevelFunc, coreHandle, "hybm_set_log_level");
    DL_LOAD_SYM(pHybmGetError, hybmGetErrorFunc, coreHandle, "hybm_get_error_string");

    DL_LOAD_SYM(pHybmCreateEntity, hybmCreateEntityFunc, coreHandle, "hybm_create_entity");
    DL_LOAD_SYM(pHybmDestroyEntity, hybmDestroyEntityFunc, coreHandle, "hybm_destroy_entity");
    DL_LOAD_SYM(pHybmReserveMem, hybmReserveMemFunc, coreHandle, "hybm_reserve_mem_space");
    DL_LOAD_SYM(pHybmUnreserveMem, hybmUnreserveMemFunc, coreHandle, "hybm_unreserve_mem_space");
    DL_LOAD_SYM(pHybmAllocLocalMem, hybmAllocLocalMemFunc, coreHandle, "hybm_alloc_local_memory");
    DL_LOAD_SYM(pHybmFreeLocalMem, hybmFreeLocalMemFunc, coreHandle, "hybm_free_local_memory");
    DL_LOAD_SYM(pHybmExport, hybmExportFunc, coreHandle, "hybm_export");
    DL_LOAD_SYM(pHybmImport, hybmImportFunc, coreHandle, "hybm_import");
    DL_LOAD_SYM(pHybmSetExtraContext, hybmSetExtraContextFunc, coreHandle, "hybm_set_extra_context");
    DL_LOAD_SYM(pHybmStart, hybmStartFunc, coreHandle, "hybm_start");
    DL_LOAD_SYM(pHybmStop, hybmStopFunc, coreHandle, "hybm_stop");
    DL_LOAD_SYM(pHybmMmap, hybmMmapFunc, coreHandle, "hybm_mmap");
    DL_LOAD_SYM(pHybmJoin, hybmJoinFunc, coreHandle, "hybm_join");
    DL_LOAD_SYM(pHybmLeave, hybmLeaveFunc, coreHandle, "hybm_leave");
    DL_LOAD_SYM(pHybmDataCopy, hybmDataCopyFunc, coreHandle, "hybm_data_copy");

    DL_LOAD_SYM(gHybmTransportInit, hybmTransportInitFunc, coreHandle, "hybm_transport_init");
    DL_LOAD_SYM(gHybmTransportGetAddress, hybmTransportGetAddressFunc, coreHandle, "hybm_transport_get_address");
    DL_LOAD_SYM(gHybmTransportSetAddresses, hybmTransportSetAddressesFunc, coreHandle, "hybm_transport_set_addresses");
    DL_LOAD_SYM(gHybmTransportMakeConnections, hybmTransportMakeConnectionsFunc, coreHandle, "hybm_transport_make_connections");
    DL_LOAD_SYM(gHybmTransportAiQpInfoAddress, hybmTransportAiQpInfoAddressFunc, coreHandle, "hybm_transport_ai_qp_info_address");
    gLoaded = true;
    return SM_OK;
}
}
}