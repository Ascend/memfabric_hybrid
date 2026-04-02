# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# Embricks is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set(EMBRICKS_TEST_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(EMBRICKS_TEST_3RDPARTY_OUTPUT_DIR ${PROJECT_SOURCE_DIR}/output/3rdparty)

include_directories(${EMBRICKS_TEST_3RDPARTY_OUTPUT_DIR}/googletest/include)
include_directories(${EMBRICKS_TEST_3RDPARTY_OUTPUT_DIR}/googletest/googlemock/include)
include_directories(${EMBRICKS_TEST_3RDPARTY_OUTPUT_DIR}/mockcpp/include)
include_directories(${EMBRICKS_TEST_3RDPARTY_OUTPUT_DIR}/mockcpp/3rdparty)

link_directories(${EMBRICKS_TEST_3RDPARTY_OUTPUT_DIR}/googletest/lib)

if (BUILD_UT)
    message(STATUS "app/embricks: build embricks unit test is required")
    message(STATUS "app/embricks: EMBRICKS_TEST_3RDPARTY_OUTPUT_DIR = ${EMBRICKS_TEST_3RDPARTY_OUTPUT_DIR}")
    add_subdirectory(ut/testcase)
endif ()