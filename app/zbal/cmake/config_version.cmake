# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# ZBAL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# read version content from file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" ZBAL_VERSION_CONTENT)

# verify version format which should be x.x.x
# i.e. {major_version}.{minor_version}.{fix}
# all of them should be a digital
string(STRIP "${ZBAL_VERSION_CONTENT}" ZBAL_VERSION_RAW)
if (NOT ZBAL_VERSION_RAW MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "app/zbal: invalid version format in VERSION file: '${ZBAL_VERSION_RAW}'")
endif ()

# split it version string into single field
list(GET ZBAL_VERSION_RAW 0 DUMMY)
string(REPLACE "." ";" ZBAL_ZBAL_VERSION_LIST "${ZBAL_VERSION_RAW}")
list(LENGTH ZBAL_ZBAL_VERSION_LIST ZBAL_VERSION_LIST_LEN)
if (NOT ZBAL_VERSION_LIST_LEN EQUAL 3)
    message(FATAL_ERROR "app/zbal: expected exactly 3 version components, got: ${ZBAL_VERSION_LIST_LEN}")
endif ()
list(GET ZBAL_ZBAL_VERSION_LIST 0 ZBAL_VERSION_MAJOR)
list(GET ZBAL_ZBAL_VERSION_LIST 1 ZBAL_VERSION_MINOR)
list(GET ZBAL_ZBAL_VERSION_LIST 2 ZBAL_VERSION_FIX)

# add MACRO with single field
add_compile_definitions(ZBAL_VERSION_MAJOR=${ZBAL_VERSION_MAJOR}
        ZBAL_VERSION_MINOR=${ZBAL_VERSION_MINOR}
        ZBAL_VERSION_FIX=${ZBAL_VERSION_FIX})

# print
message(STATUS "app/zbal: ZBAL_VERSION_MAJOR=${ZBAL_VERSION_MAJOR} ZBAL_VERSION_MINOR=${ZBAL_VERSION_MINOR} ZBAL_VERSION_FIX=${ZBAL_VERSION_FIX}")