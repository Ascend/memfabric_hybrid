# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

cmake_minimum_required(VERSION 3.14.1)
project(NDR C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# Set compile options
add_compile_options(-O2
            -Wall
            -Wextra
            -Werror
            -Wfloat-equal
            -fstack-protector-strong
            -fno-omit-frame-pointer
            -fPIC
            -fPIE
            -fsigned-char
            -D_FORTIFY_SOURCE=2
            -unresolved-symbols=ignore-all
            )

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    add_compile_options(-DRELEASE)
    add_link_options(-Wl,-z,relro,-z,now,-z,noexecstack -s)
else()
    add_compile_options(-g -DDEBUG)
endif()

# Define options
option(NDR_BUILD_EXAMPLE "Build ndr example" OFF)

# Set directories
set(NDR_SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(NDR_3RDPARTY_SRC_DIR ${PROJECT_SOURCE_DIR}/3rdparty)
set(NDR_BUILD_PATH ${PROJECT_SOURCE_DIR}/build)

if (NOT CMAKE_INSTALL_PREFIX OR ${CMAKE_INSTALL_PREFIX} MATCHES "/usr/local")
    set(NDR_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/output)
else ()
    set(NDR_OUTPUT_PATH ${CMAKE_INSTALL_PREFIX})
endif()

# Other targets depend on target under src.
add_subdirectory(3rdparty)
add_subdirectory(src)


# ========== Unit Test Build Option ==========
message(STATUS "Checking BUILD_TESTS option...")

option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    message(STATUS "BUILD_TESTS is ON —> Adding test subdirectory")
    add_subdirectory(test)
else()
    message(STATUS "BUILD_TESTS is OFF —> Skipping unit tests")
endif()

message(STATUS "Finished processing BUILD_TESTS option")
# ===========================================

# Build example
if (NDR_BUILD_EXAMPLE STREQUAL "ON")
    add_subdirectory(example)
endif()
