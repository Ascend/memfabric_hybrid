# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# read version content from file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VERSION_CONTENT)

# verify version format which should be x.x.x
# i.e. {major_version}.{minor_version}.{fix}
# all of them should be a digital
string(STRIP "${VERSION_CONTENT}" PROJECT_VERSION_RAW)
if (NOT PROJECT_VERSION_RAW MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "Invalid version format in VERSION file: '${PROJECT_VERSION_RAW}'")
endif ()

add_compile_definitions(PROJECT_VERSION_RAW=\"${PROJECT_VERSION_RAW}\")
message(STATUS "in memfabric cmakefile BUILD_FROM_MEMCACHE: ${BUILD_FROM_MEMCACHE}, version: ${PROJECT_VERSION_RAW}.")

# split it version string into single field
list(GET PROJECT_VERSION_RAW 0 DUMMY)
string(REPLACE "." ";" VERSION_LIST "${PROJECT_VERSION_RAW}")
list(LENGTH VERSION_LIST VERSION_LIST_LEN)
if (NOT VERSION_LIST_LEN EQUAL 3)
    message(FATAL_ERROR "Expected exactly 3 version components, got: ${VERSION_LIST_LEN}")
endif ()

list(GET VERSION_LIST 0 VERSION_MAJOR)
list(GET VERSION_LIST 1 VERSION_MINOR)
list(GET VERSION_LIST 2 VERSION_FIX)

# add MACRO with single field
add_compile_definitions(VERSION_MAJOR=${VERSION_MAJOR}
        VERSION_MINOR=${VERSION_MINOR}
        VERSION_FIX=${VERSION_FIX})

message(STATUS "VERSION_MAJOR = ${VERSION_MAJOR}, VERSION_MINOR = ${VERSION_MINOR}, VERSION_FIX = ${VERSION_FIX}")


# set log commit into compile definition for full library version as well
if (BUILD_GIT_COMMIT STREQUAL "ON")
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
