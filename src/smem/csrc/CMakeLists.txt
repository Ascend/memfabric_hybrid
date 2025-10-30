# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.

include_directories(
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store
        ${PROJECT_SMEM_SRC_BASE}/csrc/net
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_shm
        ${PROJECT_HYBM_SRC_BASE}/include
        ${PROJECT_ACCLINKS_SRC_BASE}/include
)

file(GLOB_RECURSE PLATFORM_SRC
        ${PROJECT_SMEM_SRC_BASE}/csrc/common/*.cpp
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/*.cpp
        ${PROJECT_SMEM_SRC_BASE}/csrc/net/*.cpp
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_shm/*.cpp
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_bm/*.cpp
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_trans/*.cpp
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem.cpp
)

## build the static and shared library
add_library(smem_objects OBJECT ${PLATFORM_SRC})
target_link_libraries(smem_objects PRIVATE hybmm_objects)

target_link_libraries(smem_objects PUBLIC smem)
target_compile_options(smem_objects PRIVATE -fPIC)

add_library(smem_static STATIC $<TARGET_OBJECTS:smem_objects>)
set_target_properties(smem_static PROPERTIES OUTPUT_NAME "mf_smem")
target_link_directories(smem_static PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib)
target_link_libraries(smem_static PUBLIC pthread acc_tcp_net_static hybmm_static)

add_library(smem_shared SHARED $<TARGET_OBJECTS:smem_objects>)
set_target_properties(smem_shared PROPERTIES OUTPUT_NAME "mf_smem")
target_link_directories(smem_shared PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib)
target_link_libraries(smem_shared PUBLIC pthread acc_tcp_net_static hybmm_shared)

if(BUILD_PYTHON STREQUAL "ON")
    add_subdirectory(python_wrapper)
endif()

#install
file(GLOB HEADER_FILES ../include/host/*.h)
file(GLOB DEVICE_HEADER_FILES ../include/device/*.h)

#install head files
install(FILES ${HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/smem/include/smem/host)
install(FILES ${DEVICE_HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/smem/include/smem/device/)

# install library
install(TARGETS smem_shared
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/smem/lib64
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        )