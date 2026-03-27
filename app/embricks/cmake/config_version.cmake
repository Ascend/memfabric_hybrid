# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# Embricks is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# read version content from file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" EMB_VERSION_CONTENT)

# verify version format which should be x.x.x
# i.e. {major_version}.{minor_version}.{fix}
# all of them should be a digital
string(STRIP "${EMB_VERSION_CONTENT}" EMB_PROJECT_VERSION_RAW)
if (NOT EMB_PROJECT_VERSION_RAW MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "Invalid version format in VERSION file: '${EMB_PROJECT_VERSION_RAW}'")
endif ()


# split it version string into single field
list(GET EMB_PROJECT_VERSION_RAW 0 DUMMY)
string(REPLACE "." ";" EMB_VERSION_LIST "${EMB_PROJECT_VERSION_RAW}")
list(LENGTH EMB_VERSION_LIST EMB_VERSION_LIST_LEN)
if (NOT EMB_VERSION_LIST_LEN EQUAL 3)
    message(FATAL_ERROR "Expected exactly 3 version components, got: ${EMB_VERSION_LIST_LEN}")
endif ()

list(GET EMB_VERSION_LIST 0 EMB_VERSION_MAJOR)
list(GET EMB_VERSION_LIST 1 EMB_VERSION_MINOR)
list(GET EMB_VERSION_LIST 2 EMB_VERSION_FIX)

# add MACRO with single field
add_compile_definitions(EMB_VERSION_MAJOR=${EMB_VERSION_MAJOR}
        EMB_VERSION_MINOR=${EMB_VERSION_MINOR}
        EMB_VERSION_FIX=${EMB_VERSION_FIX})

message(STATUS "EMB_VERSION_MAJOR = ${EMB_VERSION_MAJOR}, EMB_VERSION_MINOR = ${EMB_VERSION_MINOR}, EMB_VERSION_FIX = ${EMB_VERSION_FIX}")


# set log commit into compile definition for full library version as well
if (BUILD_GIT_COMMIT)
    find_program(GIT_EXECUTABLE NAMES git)
    if (EXISTS ${GIT_EXECUTABLE})
        # get commit id from file
        execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
                RESULT_VARIABLE GIT_COMMIT_RESULT
                OUTPUT_VARIABLE GIT_LAST_COMMIT
                OUTPUT_STRIP_TRAILING_WHITESPACE)
        if (${GIT_COMMIT_RESULT} EQUAL 1)
            set(GIT_LAST_COMMIT "empty")
        endif ()
        add_definitions(-DGIT_LAST_COMMIT=${GIT_LAST_COMMIT})
        message(STATUS "add definition for last commit: ${GIT_LAST_COMMIT}")
    else ()
        add_definitions(-DGIT_LAST_COMMIT=empty)
        message(STATUS "Failed to find git command, not GIT_LAST_COMMIT will be set")
    endif ()
else ()
    add_definitions(-DGIT_LAST_COMMIT=empty)
    message(STATUS "add definition for last commit: empty")
endif ()
