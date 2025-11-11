# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.

include_directories(
        ${PROJECT_HYBM_SRC_BASE}/include
        ${PROJECT_HYBM_SRC_BASE}/csrc/common
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation/host
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver/userspace
        ${PROJECT_HYBM_SRC_BASE}/csrc/entity
        ${PROJECT_HYBM_SRC_BASE}/csrc/mm
        ${PROJECT_HYBM_SRC_BASE}/csrc/ts_engine
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/compose
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/device
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/host
        ${PROJECT_HYBM_SRC_BASE}/csrc/under_api
        ${PROJECT_DRIVER_BASE}/hybm/userspace
)

file(GLOB_RECURSE HYBMM_SRC_FILES
        ${PROJECT_HYBM_SRC_BASE}/csrc/*.cpp
)
set(HYBM_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../include")

## build the static and shared library
add_library(hybmm_objects OBJECT ${HYBMM_SRC_FILES})
target_compile_options(hybmm_objects PRIVATE -fPIC)
target_include_directories(hybmm_objects INTERFACE ${HYBM_INCLUDE_DIR})

add_library(hybmm_static STATIC $<TARGET_OBJECTS:hybmm_objects>)
set_target_properties(hybmm_static PROPERTIES OUTPUT_NAME "mf_hybm_core")
target_link_libraries(hybmm_static PUBLIC pthread dl)
target_include_directories(hybmm_static INTERFACE ${HYBM_INCLUDE_DIR})

add_library(hybmm_shared SHARED $<TARGET_OBJECTS:hybmm_objects>)
set_target_properties(hybmm_shared PROPERTIES OUTPUT_NAME "mf_hybm_core")
target_link_libraries(hybmm_shared PUBLIC pthread dl)
target_include_directories(hybmm_shared INTERFACE ${HYBM_INCLUDE_DIR})

set_target_properties(hybmm_shared PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_HYBM_OUTPUT}/lib"
)

set_target_properties(hybmm_shared PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
)

install(TARGETS hybmm_shared
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

## install header files
file(GLOB HEADER_FILES ${PROJECT_HYBM_SRC_BASE}/include/*.h)
install(
        FILES ${HEADER_FILES}
        DESTINATION ${PROJECT_HYBM_OUTPUT}/include
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

## install library
install(
        TARGETS hybmm_static hybmm_shared
        DESTINATION ${PROJECT_HYBM_OUTPUT}/lib64
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
