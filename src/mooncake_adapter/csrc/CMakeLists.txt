# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

if (CMAKE_C_COMPILER STREQUAL gcc)
    add_compile_options(-fabi-version=0)
endif ()
add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
include_directories(
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc/under_api/hybm_core
        ${PROJECT_ADAPTER_SRC_BASE}/csrc/common
        ${PROJECT_ADAPTER_SRC_BASE}/csrc/util
)

if ((BUILD_PYTHON STREQUAL "ON") AND (NOT BUILD_TESTS STREQUAL "ON"))
    file(GLOB_RECURSE MOONCAKE_SRC *.cpp)
    add_library(pytransfer OBJECT  ${MOONCAKE_SRC})
    find_package(pybind11 REQUIRED)
    include_directories(
            ${Python3_INCLUDE_DIRS}
            ${pybind11_INCLUDE_DIRS}
    )
    pybind11_add_module(_pytransfer transfer/pytransfer.cpp util/transfer_util.cpp)
    set_target_properties(_pytransfer PROPERTIES CXX_STANDARD 17)
    set_target_properties(_pytransfer PROPERTIES OUTPUT_NAME "_pymf_transfer")
    target_link_libraries(_pytransfer PRIVATE smem_shared)
endif ()