# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# ZBAL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
project(zbal LANGUAGES CXX)

option(BUILD_ZBAL_MODULE_UT "build unit test for zbal library" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_VERBOSE_MAKEFILE ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RELEASE)
endif ()

include(cmake/config_version.cmake)
include(cmake/config_ascendc.cmake)
include(cmake/config_pybind_torch.cmake)
include(cmake/config_git_last_commit.cmake)

set(ZBAL_BASE_DIR ${PROJECT_SOURCE_DIR})
set(ZBAL_SRC_DIR ${ZBAL_BASE_DIR}/src)

# set the library output dir to the python dir for wheel package build
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ZBAL_BASE_DIR}/output)

add_compile_options(-Wunused-value -Wcast-align -Wno-macro-redefined)
add_compile_options(-fpermissive -Wno-error)
add_compile_options(-Wcast-qual -Winvalid-pch -Wwrite-strings -Wsign-compare -Wextra -Wno-deprecated-declarations)
add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)

if (${CMAKE_BUILD_TYPE} MATCHES "RELEASE")
    add_compile_options(-O3)
    add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)
    add_compile_options(-fstack-protector-strong)
    add_compile_options(-Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -fPIE -fPIC -ftrapv -s)
    message(STATUS "build type set to RELEASE")
else ()
    add_compile_options(-g -rdynamic)
endif ()

add_subdirectory(src/csrc)

if (BUILD_ZBAL_MODULE_UT)
    message(STATUS "test： unit test build is required")
    add_compile_definitions(UNIT_TEST=1)
    add_subdirectory(test)
endif ()