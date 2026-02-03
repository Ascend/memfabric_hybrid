# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.

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

# Build example
if (NDR_BUILD_EXAMPLE STREQUAL "ON")
    add_subdirectory(example)
endif()
