# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

if (CMAKE_C_COMPILER STREQUAL "gcc")
    add_compile_options(-fabi-version=0)
endif ()
add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
add_library(pymf_hybrid OBJECT pymf_hybrid.cpp)

if ((BUILD_PYTHON STREQUAL "ON") AND (NOT BUILD_TESTS STREQUAL "ON"))
    find_program(PYTHON_EXECUTABLE NAMES python3 python)

    # python
    message(STATUS "PYTHON_EXECUTABLE:${PYTHON_EXECUTABLE}")
    execute_process(
            COMMAND ${PYTHON_EXECUTABLE} "-c"
            "from __future__ import print_function; import os; import pybind11;
print(os.path.dirname(pybind11.__file__),end='');"
            RESULT_VARIABLE _PYTHON_SUCCESS
            OUTPUT_VARIABLE PYBIND11_DIR)

    if(NOT _PYTHON_SUCCESS MATCHES 0)
        message(STATUS "PYBIND11_DIR: ${PYBIND11_DIR}")
        message(FATAL_ERROR "Pybind11 config Error.")
    endif()

    # pybind11
    message(STATUS "PYBIND11_DIR = ${PYBIND11_DIR}")
    list(APPEND CMAKE_PREFIX_PATH ${PYBIND11_DIR})

    find_package(pybind11 REQUIRED)
    include_directories(
            ${Python3_INCLUDE_DIRS}
            ${pybind11_INCLUDE_DIRS}
    )
    pybind11_add_module(_pymf_hybrid pymf_hybrid.cpp)
    set_target_properties(_pymf_hybrid PROPERTIES CXX_STANDARD 17)
    set_target_properties(_pymf_hybrid PROPERTIES OUTPUT_NAME "_pymf_hybrid")
    target_link_libraries(_pymf_hybrid PRIVATE smem_shared)

endif ()
