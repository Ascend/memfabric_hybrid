cmake_minimum_required(VERSION 3.12.0)
project(SMEM VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)

option(BUILD_EXAMPLE "build example or not" ON)
option(BUILD_TESTS "build test or not" OFF)
option(BUILD_OPEN_ABI "build open _GLIBCXX_USE_CXX11_ABI" ON)
option(ENABLE_CPU_MONOTONIC "enable monotonic time using cpu hard instruction" OFF)
option(GENERATE_GIT_COMMIT_FILE "generate commit file using git log" OFF)
option(BUILD_GIT_COMMIT "build library version with commit" OFF)

message(STATUS "BUILD_EXAMPLE = ${BUILD_EXAMPLE}")
message(STATUS "BUILD_TESTS = ${BUILD_TESTS}")
message(STATUS "BUILD_USE_CXX11_ABI = ${BUILD_OPEN_ABI}")
message(STATUS "ENABLE_CPU_MONOTONIC = ${ENABLE_CPU_MONOTONIC}")
message(STATUS "GENERATE_GIT_COMMIT_FILE = ${GENERATE_GIT_COMMIT_FILE}")
message(STATUS "BUILD_GIT_COMMIT = ${BUILD_GIT_COMMIT}")

message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

#set(CMAKE_VERBOSE_MAKEFILE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

add_compile_options(-hno-unused-parameter -lno-unused-function -Wunused-variable -Wunused-value -Wcast-align)
add_compile_options(-Wcast-qual -Winvalid-pch -Wwrite-strings -Wsign-compare -Wfloat-equal -Wextra)

if (${CMAKE_BUILD_TYPE} MATCHES "RELEASE")
    add_compile_options(-O2)
    add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)
    add_compile_options(-fstack-protector-strong)
    add_compile_options(-Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -fPIE -fPIC -ftrapv -s)
    message(STATUS "build type set to RELEASE")
else ()
    add_compile_options(-g -rdynamic)
endif ()

if(BUILD_TESTS STREQUAL "ON")
    set(BUILD_OPEN_ABI "ON")
endif()

if(NOT BUILD_OPEN_ABI STREQUAL "ON")
    add_compile_options(-D_GLIBCXX_USE_CXX11_ABI=0)
endif()

if (ENABLE_CPU_MONOTONIC STREQUAL "ON")
    add_compile_options(-DENABLE_CPU_MONOTONIC)
endif ()

# generate commit log
if (GENERATE_GIT_COMMIT_FILE STREQUAL "ON")
    exec_program(sh ${PROJECT_SOURCE_DIR} ARGS ${PROJECT_SOURCE_DIR}/script/gen_last_git_commit.sh)
endif ()

# set log commit into compile definition
if (BUILD_GIT_COMMIT STREQUAL "ON")
    execute_process(COMMAND head -n 1 ${PROJECT_SOURCE_DIR}/script/git_last_commit.txt
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            RESULT_VARIABLE GET_COMMIT_RESULT
            OUTPUT_VARIABLE GIT_LAST_COMMIT)
    if (${GET_COMMIT_RESULT} EQUAL 1)
        set(GIT_LAST_COMMIT "empty")
    endif ()
    add_definitions(-DGIT_LAST_COMMIT=${GIT_LAST_COMMIT})
    message(STATUS "add gcc definition for last commit: ${GIT_LAST_COMMIT}")
else ()
    add_definitions(-DGIT_LAST_COMMIT=empty)
    message(STATUS "add gcc definition for last commit: empty")
endif ()

set(PROJECT_BUILD_PATH ${PROJECT_SOURCE_DIR}/build)
set(PROJECT_3RDPARTY_SRC_DIR ${PROJECT_SOURCE_DIR}/3rdparty)
set(PROJECT_3RDPARTY_BIN_DIR ${PROJECT_SOURCE_DIR}/output/3rdparty)
set(PROJECT_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/output)

set(PROJECT_SMEM_SRC_BASE ${PROJECT_SOURCE_DIR}/src/smem)
set(PROJECT_SMEM_OUTPUT ${PROJECT_SOURCE_DIR}/output/smem)

# set install target top dir
message(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
if (${CMAKE_INSTALL_PREFIX} MATCHES "/usr/local")
    set(TARGET_INSTALL_DIR ${PROJECT_OUTPUT_PATH}/)
elseif (NOT EXISTS ${CMAKE_INSTALL_PREFIX})
    set(TARGET_INSTALL_DIR ${PROJECT_OUTPUT_PATH}/)
else ()
    set(TARGET_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/)
endif ()
message(STATUS "install dir ${TARGET_INSTALL_DIR}")

#set(PROJECT_PLATFORM_SRC_BASE ${PROJECT_SOURCE_DIR}/src/platform)
#set(PROJECT_PLATFORM_OUTPUT ${PROJECT_SOURCE_DIR}/output/platform)

include_directories(
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SOURCE_DIR}/include/hybm
)

add_subdirectory(src)
add_subdirectory(3rdparty)

if(BUILD_TESTS STREQUAL "ON")
    set(CMAKE_CXX_STANDARD 14)
    message(STATUS "BUILD_TESTS = ON, add compile gov")
    add_subdirectory(test)
endif()