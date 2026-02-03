# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
cmake_minimum_required(VERSION 3.12.0)
project(memfabric_hybrid LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

option(ENABLE_TRACE_LOG "Enable trace log" OFF)
option(ENABLE_ASAN "Enable ASAN" OFF)
option(BUILD_UT "build ut or not" OFF)
option(BUILD_OPEN_ABI "build open _GLIBCXX_USE_CXX11_ABI" ON)
option(ENABLE_CPU_MONOTONIC "enable monotonic time using cpu hard instruction" OFF)
option(BUILD_GIT_COMMIT "build library version with commit" ON)
option(BUILD_PYTHON "build python file" ON)
option(ENABLE_PTRACER "build ptracer" OFF)
option(BUILD_TEST "Enable/disable build and package test utilities and examples" OFF)
option(BUILD_HCOM "Enable/disable build and package hcom" OFF)

include(cmake/config_version.cmake)
include(cmake/config_xpu.cmake)
include(cmake/config_python.cmake)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RELEASE)
endif ()

message(STATUS "BUILD_UT = ${BUILD_UT}")
message(STATUS "BUILD_USE_CXX11_ABI = ${BUILD_OPEN_ABI}")
message(STATUS "ENABLE_CPU_MONOTONIC = ${ENABLE_CPU_MONOTONIC}")
message(STATUS "BUILD_GIT_COMMIT = ${BUILD_GIT_COMMIT}")
message(STATUS "BUILD_PYTHON = ${BUILD_PYTHON}")
message(STATUS "ENABLE_PTRACER = ${ENABLE_PTRACER}")
message(STATUS "CMAKE_COMPILER = ${CMAKE_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER = ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
message(STATUS "XPU_TYPE = ${XPU_TYPE}")
message(STATUS "BUILD_TEST = ${BUILD_TEST}")
message(STATUS "BUILD_HCOM = ${BUILD_HCOM}")

find_program(NINJA_EXE NAMES ninja)
if (NINJA_EXE)
    set(MAKE_CMD ninja)
else ()
    set(MAKE_CMD make)
endif ()
message(STATUS "MAKE_CMD = ${MAKE_CMD}")

add_compile_options(-Wunused-variable -Wunused-value -Wcast-align)
add_compile_options(-fpermissive -Wno-error)
add_compile_options(-Wcast-qual -Winvalid-pch -Wwrite-strings -Wsign-compare -Wfloat-equal -Wextra -Wno-deprecated-declarations)

if (${CMAKE_BUILD_TYPE} MATCHES "RELEASE")
    #add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)
    add_compile_options(-fstack-protector-strong -Wstack-usage=16384 -Werror)
    add_compile_options(-Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -fPIE -fPIC -ftrapv -s)
    add_compile_options(-D_FORTIFY_SOURCE=2 -O2)
    add_link_options(-Wl,-z,noexecstack,-z,relro,-z,now)
    add_link_options(-s)
    set(CMAKE_SKIP_RPATH TRUE)
    message(STATUS "added compile options for RELEASE")
else ()
    add_compile_options(-g -ggdb3 -O0 -fno-omit-frame-pointer -rdynamic -fPIC)
endif ()

if (${CMAKE_BUILD_TYPE} MATCHES "ASAN")
    # 开启多种 sanitizer   "address,undefined,pointer-compare,pointer-subtract,alignment"
    set(SANITIZERS "address,undefined,pointer-compare,pointer-subtract,alignment")
    # 编译选项
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${SANITIZERS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=${SANITIZERS}")
    # 链接选项
    add_link_options(-fsanitize=${SANITIZERS})
endif ()

if (BUILD_UT STREQUAL "ON")
    add_definitions(-DUT_ENABLED)
    if (NOT CMAKE_COMPILER MATCHES bisheng)
        add_compile_options(-fprofile-arcs -ftest-coverage)
        add_link_options(-lgcov --coverage)
    endif ()
    set(BUILD_OPEN_ABI "ON")
endif ()

if (ENABLE_PTRACER STREQUAL "ON")
    add_definitions(-DENABLE_PTRACER=ON)
endif ()

if (NOT BUILD_OPEN_ABI STREQUAL "ON")
    add_compile_options(-D_GLIBCXX_USE_CXX11_ABI=0)
endif ()

if (ENABLE_CPU_MONOTONIC STREQUAL "ON")
    add_compile_options(-DENABLE_CPU_MONOTONIC)
endif ()

if (ENABLE_TRACE_L0G)
    add_definitions(-DLOG_TRACE_INFO_ENABLED)
endif ()

set(PROJECT_BUILD_PATH ${PROJECT_BINARY_DIR})
set(PROJECT_3RDPARTY_SRC_DIR ${PROJECT_SOURCE_DIR}/3rdparty)
set(PROJECT_3RDPARTY_BIN_DIR ${PROJECT_SOURCE_DIR}/output/3rdparty)
set(PROJECT_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/output)

# set src base dir of util
set(PROJECT_UTIL_SRC_BASE ${PROJECT_SOURCE_DIR}/src/util/csrc)

# set src base dir and output dir of smem
set(PROJECT_SMEM_SRC_BASE ${PROJECT_SOURCE_DIR}/src/smem)
set(PROJECT_SMEM_OUTPUT ${PROJECT_SOURCE_DIR}/output/smem)

# set src base dir and output dir of smem
set(PROJECT_HYBM_SRC_BASE ${PROJECT_SOURCE_DIR}/src/hybm)
set(PROJECT_HYBM_OUTPUT ${PROJECT_SOURCE_DIR}/output/hybm)

# set src base dir and output dir of adapter
set(PROJECT_ADAPTER_SRC_BASE ${PROJECT_SOURCE_DIR}/src/smem/csrc/python_wrapper/mk_transfer_adapter)
set(PROJECT_ADAPTER_OUTPUT ${PROJECT_SOURCE_DIR}/output/mf_transfer)

# set src base dir and output dir of acc_links
set(PROJECT_ACCLINKS_SRC_BASE ${PROJECT_SOURCE_DIR}/src/acc_links)
set(PROJECT_ACCLINKS_OUTPUT ${PROJECT_SOURCE_DIR}/output/acc_links)

# set install target top dir
message(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
if (${CMAKE_INSTALL_PREFIX} MATCHES "/usr/local")
    set(TARGET_INSTALL_DIR ${PROJECT_OUTPUT_PATH}/)
elseif (NOT EXISTS ${CMAKE_INSTALL_PREFIX})
    set(TARGET_INSTALL_DIR ${PROJECT_OUTPUT_PATH}/)
else ()
    set(TARGET_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/)
endif ()
message(STATUS "TARGET_INSTALL_DIR = ${TARGET_INSTALL_DIR}")

# 判断环境变量 CMAKE_PREFIX_PATH 是否存在
if (DEFINED ENV{CMAKE_PREFIX_PATH})
    # 如果环境变量存在，将其值赋给 CMake 变量
    set(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")
    list(APPEND CMAKE_PREFIX_PATH "${pybind11_cmake_dir}")
    message(STATUS "CMAKE_PREFIX_PATH is exist, and append: ${pybind11_cmake_dir}")
    message(STATUS "Now CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
else ()
    # 如果环境变量不存在，设置 CMake 变量的默认值
    set(CMAKE_PREFIX_PATH "${pybind11_cmake_dir}")
    message(STATUS "Not set CMAKE_PREFIX_PATH, and initialize to: ${CMAKE_PREFIX_PATH}")
endif ()

include_directories(
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SOURCE_DIR}/include/hybm
        ${PROJECT_SOURCE_DIR}/src/util/include
        ${PROJECT_UTIL_SRC_BASE}
)

# include hcom cmake after TARGET_INSTALL_DIR set
include(cmake/config_hcom.cmake)

add_subdirectory(src)

if (BUILD_UT STREQUAL "ON")
    set(CMAKE_CXX_STANDARD 17)
    message(STATUS "BUILD_UT = ON, add compile gov")
    add_subdirectory(test)
endif ()

if ("${BUILD_TEST}" STREQUAL "ON")
    add_subdirectory(example/bm/BmCpp)
    add_subdirectory(example/bm/BmBenchmark)
    add_subdirectory(example/trans/perf)
endif ()