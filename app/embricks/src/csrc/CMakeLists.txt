# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# Embricks is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

include_directories(
        ${EMBRICKS_SRC_DIR}/include
        ${EMBRICKS_SRC_DIR}/csrc/common
        ${EMBRICKS_SRC_DIR}/csrc/hashmap
        ${EMBRICKS_SRC_DIR}/csrc/net
        ${EMBRICKS_SRC_DIR}/csrc/service
        ${EMBRICKS_SRC_DIR}/csrc/table
        ${EMBRICKS_SRC_DIR}/csrc/under_api
)

# common objects
file(GLOB_RECURSE COMMON_SRCS
        ${EMBRICKS_SRC_DIR}/csrc/common/*.cpp)
add_library(common_objects OBJECT ${COMMON_SRCS})
target_compile_options(common_objects PRIVATE -fPIC)

# hashmap objects
file(GLOB_RECURSE HASHMAP_SRCS
        ${EMBRICKS_SRC_DIR}/csrc/hashmap/*.cpp)
add_library(hashmap_objects OBJECT ${HASHMAP_SRCS})
target_compile_options(hashmap_objects PRIVATE -fPIC)

# net objects
file(GLOB_RECURSE NET_SRCS
        ${EMBRICKS_SRC_DIR}/csrc/net/*.cpp)
add_library(net_objects OBJECT ${NET_SRCS})
target_compile_options(net_objects PRIVATE -fPIC)

# table objects
file(GLOB_RECURSE TABLE_SRCS
        ${EMBRICKS_SRC_DIR}/csrc/table/*.cpp)
add_library(table_objects OBJECT ${TABLE_SRCS})
target_compile_options(table_objects PRIVATE -fPIC)

# under_api objects
file(GLOB_RECURSE UNDER_API_SRCS
        ${EMBRICKS_SRC_DIR}/csrc/under_api/*.cpp)
add_library(under_api_objects OBJECT ${UNDER_API_SRCS})
target_compile_options(under_api_objects PRIVATE -fPIC)

# service objects
file(GLOB_RECURSE SERVICE_SRCS
        ${EMBRICKS_SRC_DIR}/csrc/service/*.cpp)
add_library(service_objects OBJECT ${SERVICE_SRCS})
target_compile_options(service_objects PRIVATE -fPIC)

# entry objects
file(GLOB ENTRY_SRCS
        ${EMBRICKS_SRC_DIR}/csrc/*.cpp)
add_library(entry_objects OBJECT ${ENTRY_SRCS})
target_compile_options(entry_objects PRIVATE -fPIC)

## build the static and shared library
add_library(embricks_static STATIC 
        $<TARGET_OBJECTS:common_objects> 
        $<TARGET_OBJECTS:hashmap_objects>
        $<TARGET_OBJECTS:net_objects>
        $<TARGET_OBJECTS:table_objects>
        $<TARGET_OBJECTS:under_api_objects>
        $<TARGET_OBJECTS:service_objects>
        $<TARGET_OBJECTS:entry_objects>
)
set_target_properties(embricks_static PROPERTIES OUTPUT_NAME "embricks")
target_include_directories(embricks_static INTERFACE ${HYBM_PUBLIC_INCLUDE_DIR})

add_library(embricks_shared SHARED
        $<TARGET_OBJECTS:common_objects>
        $<TARGET_OBJECTS:hashmap_objects>
        $<TARGET_OBJECTS:net_objects>
        $<TARGET_OBJECTS:table_objects>
        $<TARGET_OBJECTS:under_api_objects>
        $<TARGET_OBJECTS:service_objects>
        $<TARGET_OBJECTS:entry_objects>
)
set_target_properties(embricks_shared PROPERTIES OUTPUT_NAME "embricks")
target_link_libraries(embricks_shared PUBLIC pthread dl)

## install header files
file(GLOB HEADER_FILES ${EMBRICKS_SRC_DIR}/include/*.h)
install(
        FILES ${HEADER_FILES}
        DESTINATION ${EMBRICKS_OUTPUT_DIR}/include/embricks
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

## install library
install(
        TARGETS embricks_static embricks_shared
        DESTINATION ${EMBRICKS_OUTPUT_DIR}/lib64
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
