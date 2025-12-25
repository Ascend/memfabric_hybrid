# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.


include_directories(
        ${PROJECT_HYBM_SRC_BASE}/include
        ${PROJECT_HYBM_SRC_BASE}/csrc/common
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation/host
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver/userspace
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver/version
        ${PROJECT_HYBM_SRC_BASE}/csrc/entity
        ${PROJECT_HYBM_SRC_BASE}/csrc/mm
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/compose
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/device
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/host
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/utils
        ${PROJECT_HYBM_SRC_BASE}/csrc/ts_engine
        ${PROJECT_HYBM_SRC_BASE}/csrc/under_api
        ${PROJECT_UTIL_SRC_BASE}/ptracer/include
)

set(HYBM_PUBLIC_INCLUDE_DIR "${PROJECT_HYBM_SRC_BASE}/include")

## add files into src variables
file(GLOB_RECURSE HYBMM_SRC_FILES
        ${PROJECT_HYBM_SRC_BASE}/csrc/*.cpp
        ${PROJECT_HYBM_SRC_BASE}/csrc/*.h
)

## build the static and shared library
add_library(hybmm_objects OBJECT ${HYBMM_SRC_FILES})
target_compile_options(hybmm_objects PRIVATE -fPIC)
target_include_directories(hybmm_objects INTERFACE ${HYBM_PUBLIC_INCLUDE_DIR})

add_library(hybmm_static STATIC $<TARGET_OBJECTS:hybmm_objects> $<TARGET_OBJECTS:ptracer_object>)
set_target_properties(hybmm_static PROPERTIES OUTPUT_NAME "mf_hybm_core")
target_link_libraries(hybmm_static PUBLIC pthread dl)
target_include_directories(hybmm_static INTERFACE ${HYBM_PUBLIC_INCLUDE_DIR})

add_library(hybmm_shared SHARED $<TARGET_OBJECTS:hybmm_objects> $<TARGET_OBJECTS:ptracer_object>)
set_target_properties(hybmm_shared PROPERTIES OUTPUT_NAME "mf_hybm_core")
target_link_libraries(hybmm_shared PUBLIC pthread dl)
target_include_directories(hybmm_shared INTERFACE ${HYBM_PUBLIC_INCLUDE_DIR})

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
