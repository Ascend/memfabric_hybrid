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
        ${ASCEND_HOME_PATH}/include
        ${ASCEND_HOME_PATH}/include/experiment/runtime/runtime/
        ${ASCEND_HOME_PATH}/include/experiment/msprof/
        ${ASCEND_HOME_PATH}/include
        ${ASCEND_HOME_PATH}/asc/include
        ${TORCH_DIR}/include/torch/csrc/api/include
        ${TORCH_DIR}/include
        ${TORCH_DIR}/include/torch/csrc/distributed
        ${TORCH_DIR}/include/torch/csrc/utils
        ${TORCH_DIR}/include/c10/util
        ${TORCH_NPU_DIR}/include
        ${PYBIND11_DIR}/include
        ${CPYTHON_DIR}/
        ${ZBAL_BASE_DIR}
        ${ZBAL_BASE_DIR}/third_party/ska
        ${ZBAL_BASE_DIR}/third_party/mstx
        ${ZBAL_SRC_DIR}/include
        ${ZBAL_SRC_DIR}/csrc/adaptor/pytorch_npu
        ${ZBAL_SRC_DIR}/csrc/adaptor/deepep
        ${ZBAL_SRC_DIR}/csrc/
        ${ZBAL_SRC_DIR}/csrc/operators
        ${ZBAL_SRC_DIR}/csrc/operators/npu
        ${ZBAL_SRC_DIR}/csrc/common
        ${ZBAL_SRC_DIR}/csrc/sma
        ${ZBAL_SRC_DIR}/csrc/under_api/cann
        ${ZBAL_SRC_DIR}/csrc/under_api/memfabric
        ${ZBAL_SRC_DIR}/csrc/bootstrap
        ${ZBAL_SRC_DIR}/csrc/bootstrap/memory
        ${ZBAL_SRC_DIR}/csrc/bootstrap/memory/memfabric
)

# python setup need disable those settings
option(DISABLE_ADAPTOR_COMPILE "Disable the adapter module" OFF)
option(DISABLE_ALLOCATOR_COMPILE "Disable the allocator module" OFF)
option(DISABLE_SHARE_COMPILE "Disable the share lib install" OFF)

add_subdirectory(operators/npu/device/)

# we seperated those files for ut with different modules

if (NOT DISABLE_ADAPTOR_COMPILE)
    # PyTorch adaptor
    message(STATUS "app/zbal: [adaptor] compile module...")
    file(GLOB_RECURSE PYTORCH_ADAPTOR_SRCS ${ZBAL_SRC_DIR}/csrc/adaptor/pytorch_npu/*.cpp)
    add_library(pytorch_adaptor_objects OBJECT ${PYTORCH_ADAPTOR_SRCS})
    target_compile_options(pytorch_adaptor_objects PRIVATE -fPIC)
    target_include_directories(pytorch_adaptor_objects PRIVATE
            ${ZBAL_SRC_DIR}/csrc/adaptor/pytorch_npu/
    )
endif ()

# deepep adaptor objects
file(GLOB_RECURSE DEEPEP_ADAPTOR_SRCS ${ZBAL_SRC_DIR}/csrc/adaptor/deepep/*.cpp)
add_library(deepep_adaptor_objects OBJECT ${DEEPEP_ADAPTOR_SRCS})
target_compile_options(deepep_adaptor_objects PRIVATE -fPIC)

# boostrap objects
file(GLOB_RECURSE BOOSTRAP_SRCS
        ${ZBAL_SRC_DIR}/csrc/bootstrap/*.cpp
        ${ZBAL_SRC_DIR}/csrc/bootstrap/memory/*.cpp
        ${ZBAL_SRC_DIR}/csrc/bootstrap/memory/memfabric/*.cpp)
add_library(boostrap_objects OBJECT ${BOOSTRAP_SRCS})
target_compile_options(boostrap_objects PRIVATE -fPIC)

# operators objects
file(GLOB OPERATORS_SRC
        ${ZBAL_SRC_DIR}/csrc/operators/*.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/*.cpp)
add_library(operators_objects OBJECT ${OPERATORS_SRC})
target_link_libraries(operators_objects PRIVATE zbal_kernel) # link not work on object, only for compile pass
target_compile_options(operators_objects PRIVATE -fPIC)

# common objects
file(GLOB_RECURSE COMMON_SRCS
        ${ZBAL_SRC_DIR}/csrc/common/*.cpp)
add_library(common_objects OBJECT ${COMMON_SRCS})
target_compile_options(common_objects PRIVATE -fPIC)

if (NOT DISABLE_ALLOCATOR_COMPILE)
    message(STATUS "app/zbal: [allocator]: compile module...")
    # memory allocator objects
    file(GLOB_RECURSE ALLOCATOR_SRCS
            ${ZBAL_SRC_DIR}/csrc/sma/*.cpp)
    add_library(allocator_objects OBJECT ${ALLOCATOR_SRCS})
    target_compile_options(allocator_objects PRIVATE -fPIC)
endif ()

# under api objects
file(GLOB_RECURSE UNDER_API_SRCS
        ${ZBAL_SRC_DIR}/csrc/under_api/cann/*.cpp
        ${ZBAL_SRC_DIR}/csrc/under_api/memfabric/*.cpp)
add_library(under_api_objects OBJECT ${UNDER_API_SRCS})
target_compile_options(under_api_objects PRIVATE -fPIC)

# zbal objects
file(GLOB ZBAL_SRCS
        ${ZBAL_SRC_DIR}/csrc/*.cpp)
list(FILTER ZBAL_SRCS EXCLUDE REGEX "zbal_pybind\\.cpp")
add_library(zbal_objects OBJECT ${ZBAL_SRCS})
target_compile_options(zbal_objects PRIVATE -fPIC)

# static lib
add_library(zbal_static STATIC $<TARGET_OBJECTS:boostrap_objects>
        $<TARGET_OBJECTS:operators_objects>
        $<TARGET_OBJECTS:common_objects>
        $<TARGET_OBJECTS:under_api_objects>
        $<TARGET_OBJECTS:zbal_objects>
        $<TARGET_OBJECTS:deepep_adaptor_objects>
)

target_link_libraries(zbal_static PRIVATE zbal_kernel tiling_api)
if (NOT DISABLE_ALLOCATOR_COMPILE)
    target_sources(zbal_static PRIVATE $<TARGET_OBJECTS:allocator_objects>)
endif ()
if (NOT DISABLE_ADAPTOR_COMPILE)
    target_sources(zbal_static PRIVATE $<TARGET_OBJECTS:pytorch_adaptor_objects>)
endif ()
set_target_properties(zbal_static PROPERTIES OUTPUT_NAME "zbal_core")

if (NOT DISABLE_SHARE_COMPILE)
    message(STATUS "app/zbal: [share]: compile module...")
    # shared lib
    add_library(zbal_shared SHARED $<TARGET_OBJECTS:boostrap_objects>
            $<TARGET_OBJECTS:operators_objects>
            $<TARGET_OBJECTS:common_objects>
            $<TARGET_OBJECTS:under_api_objects>
            $<TARGET_OBJECTS:zbal_objects>
            $<TARGET_OBJECTS:deepep_adaptor_objects>
    )
    target_link_directories(zbal_shared PUBLIC
            ${ASCEND_HOME_PATH}/lib64
            ${TORCH_DIR}/lib
            ${TORCH_NPU_DIR}/lib
    )

    target_link_libraries(zbal_shared PRIVATE zbal_kernel)
    if (NOT DISABLE_ALLOCATOR_COMPILE)
        target_sources(zbal_shared PRIVATE $<TARGET_OBJECTS:allocator_objects>)
    endif ()

    if (NOT DISABLE_ADAPTOR_COMPILE)
        target_sources(zbal_shared PRIVATE $<TARGET_OBJECTS:pytorch_adaptor_objects>)
    endif ()
    set_target_properties(zbal_shared PROPERTIES OUTPUT_NAME "zbal_core")
    target_link_libraries(zbal_shared PUBLIC pthread opapi)
endif ()

set(TARGET_INSTALL_DIR ${PROJECT_OUTPUT_PATH}/app/)

#install
file(GLOB HEADER_FILES ${ZBAL_BASE_DIR}/src/include/*.h)

message(STATUS "app/zbal: [install] header files = ${HEADER_FILES}")

#install head files
install(FILES ${HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/zbal/include)

# install shared
if (NOT DISABLE_SHARE_COMPILE)
    install(TARGETS zbal_shared
            LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/zbal/lib64
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
    )
endif ()

# install archive
install(TARGETS zbal_static
        ARCHIVE DESTINATION ${TARGET_INSTALL_DIR}/zbal/lib64
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
)