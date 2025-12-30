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
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store
        ${PROJECT_SMEM_SRC_BASE}/csrc/net
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_shm
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_trans
        ${PROJECT_HYBM_SRC_BASE}/include
        ${PROJECT_UTIL_SRC_BASE}/ptracer/include
        ${PROJECT_ACCLINKS_SRC_BASE}/include
)

file(GLOB_RECURSE PLATFORM_SRC
        ${PROJECT_SMEM_SRC_BASE}/csrc/common/*.cpp
        ${PROJECT_SMEM_SRC_BASE}/csrc/net/*.cpp
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_shm/*.cpp
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_bm/*.cpp
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_trans/*.cpp
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem.cpp
)

## build the static and shared library
add_library(smem_objects OBJECT ${PLATFORM_SRC})

target_link_libraries(smem_objects PUBLIC smem)
target_compile_options(smem_objects PRIVATE -fPIC)

add_library(smem_static STATIC $<TARGET_OBJECTS:smem_objects> $<TARGET_OBJECTS:config_store_object>)
set_target_properties(smem_static PROPERTIES OUTPUT_NAME "mf_smem")
target_link_libraries(smem_static PUBLIC pthread acc_tcp_net_static hybmm_static)

add_library(smem_shared SHARED $<TARGET_OBJECTS:smem_objects> $<TARGET_OBJECTS:config_store_object>)
set_target_properties(smem_shared PROPERTIES OUTPUT_NAME "mf_smem")
target_link_libraries(smem_shared PUBLIC pthread acc_tcp_net_static hybmm_shared)

if ((BUILD_PYTHON STREQUAL "ON") AND (NOT BUILD_UT STREQUAL "ON"))
    add_subdirectory(python_wrapper/mk_transfer_adapter)
    add_subdirectory(python_wrapper/memfabric_hybrid)
endif()

#install
file(GLOB HEADER_FILES ../include/host/*.h)
file(GLOB DEVICE_HEADER_FILES ../include/device/*.h)

#install head files
install(FILES ${HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/smem/include/host)
install(FILES ${DEVICE_HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/smem/include/device/)

# install library
install(TARGETS smem_shared
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/smem/lib64
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        )