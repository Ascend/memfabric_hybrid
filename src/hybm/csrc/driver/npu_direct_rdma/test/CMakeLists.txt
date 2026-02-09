# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
cmake_minimum_required(VERSION 3.14)
project(ndr_ut C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

# Enable coverage
add_compile_options(--coverage)
add_link_options(--coverage)

#Find googletest
set(GTEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/googletest)
if(EXISTS "${GTEST_ROOT}/googletest/include/gtest/gtest.h")
    add_subdirectory(${GTEST_ROOT} gtest-build EXCLUDE_FROM_ALL)
    set(GTEST_INCLUDE_DIRS ${GTEST_ROOT}/googletest/include)
    set(GTEST_LIBRARIES gtest gtest_main)
else()
    message(FATAL_ERROR "googletest not found in test/3rdparty/")
endif()

set(MOCK_KERNEL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ut/mock_kernel)

add_library(mock_kernel
        ut/mock_impl.cpp
)
target_include_directories(mock_kernel PUBLIC ${MOCK_KERNEL_DIR})
target_compile_options(mock_kernel PRIVATE -fpermissive)

add_executable(run_tests
        ut/ndr_test.cpp
)

target_include_directories(run_tests BEFORE PRIVATE
        ${MOCK_KERNEL_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/ut
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(run_tests
        mock_kernel
        ${GTEST_LIBRARIES}
        pthread
)

target_compile_options(run_tests PRIVATE
        -fpermissive
        -Wno-error
        -w
)