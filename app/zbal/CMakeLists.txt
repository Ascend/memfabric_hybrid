# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# ZBAL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

include(cmake/config_version.cmake)
include(cmake/config_ascendc.cmake)
include(cmake/config_pybind_torch.cmake)

set(ZBAL_BASE_DIR ${APP_BASE_DIR}/zbal)
set(ZBAL_SRC_DIR ${ZBAL_BASE_DIR}/src)

# set the library output dir to the python dir for wheel package build
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ZBAL_BASE_DIR}/output)

add_compile_options(-Wunused-value -Wcast-align -Wno-macro-redefined)
add_compile_options(-fpermissive -Wno-error)
add_compile_options(-Wcast-qual -Winvalid-pch -Wwrite-strings -Wsign-compare -Wextra -Wno-deprecated-declarations)
add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)

add_subdirectory(src/csrc)

if (BUILD_ZBAL_MODULE_UT)
    message(STATUS "test： unit test build is required")
    add_compile_definitions(UNIT_TEST=1)
    add_subdirectory(test)
endif ()