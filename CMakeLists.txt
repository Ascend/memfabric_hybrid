cmake_minimum_required(VERSION 3.12.0)
project(MF_HYBRID VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)

option(BUILD_EXAMPLE "build example or not" ON)
option(BUILD_TESTS "build test or not" OFF)
option(BUILD_OPEN_ABI "build open _GLIBCXX_USE_CXX11_ABI" ON)
option(ENABLE_CPU_MONOTONIC "enable monotonic time using cpu hard instruction" OFF)
option(BUILD_GIT_COMMIT "build library version with commit" ON)
option(BUILD_GIT_COMMIT_GEN_FILE "generate commit log file" ON)
option(INSTALL_HIGH_LEVEL_API "add device high level api into pkg" ON)
option(BUILD_PYTHON "build python file" ON)
option(BUILD_ASAN "build python file" OFF)
option(BUILD_HTRACER "build htracer" OFF)

message(STATUS "BUILD_EXAMPLE = ${BUILD_EXAMPLE}")
message(STATUS "BUILD_TESTS = ${BUILD_TESTS}")
message(STATUS "BUILD_USE_CXX11_ABI = ${BUILD_OPEN_ABI}")
message(STATUS "ENABLE_CPU_MONOTONIC = ${ENABLE_CPU_MONOTONIC}")
message(STATUS "BUILD_GIT_COMMIT = ${BUILD_GIT_COMMIT}")
message(STATUS "BUILD_GIT_COMMIT_GEN_FILE = ${BUILD_GIT_COMMIT_GEN_FILE}")
message(STATUS "BUILD_PYTHON = ${BUILD_PYTHON}")
message(STATUS "BUILD_ASAN = ${BUILD_ASAN}")
message(STATUS "BUILD_HTRACER = ${BUILD_HTRACER}")

message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

#set(CMAKE_VERBOSE_MAKEFILE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

add_compile_options(-Wunused-variable -Wunused-value -Wcast-align)
add_compile_options(-Wcast-qual -Winvalid-pch -Wwrite-strings -Wsign-compare -Wfloat-equal -Wextra)

if (${CMAKE_BUILD_TYPE} MATCHES "RELEASE")
    # add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)
    add_compile_options(-fstack-protector-strong)
    add_compile_options(-Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -fPIE -fPIC -ftrapv -s)
    add_compile_options(-D_FORTIFY_SOURCE=2 -O2)
    add_link_options(-Wl,-z,noexecstack,-z,relro,-z,now)
    add_link_options(-s)
    set(CMAKE_SKIP_RPATH TRUE)
    message(STATUS "build type set to RELEASE")
else ()
    add_compile_options(-g -rdynamic)
endif ()

if (${BUILD_ASAN} STREQUAL "ON")
    if (${CMAKE_BUILD_TYPE} MATCHES "RELEASE")
        message(FATAL_ERROR "BUILD_ASAN is ON, but CMAKE_BUILD_TYPE is RELEASE. "
                            "ASan should not be used in RELEASE builds. ")
    endif()
    add_compile_options(-fsanitize=address -g -fno-omit-frame-pointer -O0)
    add_link_options(-fsanitize=address)
endif()

if(BUILD_TESTS STREQUAL "ON")
    add_compile_options(-fprofile-arcs -ftest-coverage)
    add_link_options(-lgcov --coverage)
    set(BUILD_OPEN_ABI "ON")
endif()

if (BUILD_HTRACER STREQUAL "ON")
    add_definitions(-DBUILD_HTRACER=ON)
endif ()

if(NOT BUILD_OPEN_ABI STREQUAL "ON")
    add_compile_options(-D_GLIBCXX_USE_CXX11_ABI=0)
endif()

if (ENABLE_CPU_MONOTONIC STREQUAL "ON")
    add_compile_options(-DENABLE_CPU_MONOTONIC)
endif ()

# set log commit into compile definition
if (BUILD_GIT_COMMIT STREQUAL "ON")
    if (BUILD_GIT_COMMIT_GEN_FILE STREQUAL "ON")
        # find dos2unix command
        find_program(DOS2UNIX_CMD dos2unix)
        if (NOT EXISTS ${DOS2UNIX_CMD})
            message(FATAL_ERROR "no dos2unix found, please install")
        else ()
            message(STATUS "dos2unix found at ${DOS2UNIX_CMD}")
        endif ()

        # covert file from dos format to unix format
        set(COMMIT_GEN_FILE ${PROJECT_SOURCE_DIR}/script/gen_last_git_commit.sh)
        execute_process(COMMAND ${DOS2UNIX_CMD} ${COMMIT_GEN_FILE}
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                RESULT_VARIABLE DOS2UNIX_RESULT)
        if (${DOS2UNIX_RESULT} EQUAL 1)
            message(FATAL_ERROR "failed to dos2unix script file ${COMMIT_GEN_FILE}")
        endif ()

        # generate file
        execute_process(COMMAND sh ${COMMIT_GEN_FILE}
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                RESULT_VARIABLE GET_GEN_COMMIT_RESULT)
        if (${GET_GEN_COMMIT_RESULT} EQUAL 1)
            message(FATAL_ERROR "generate last commit file failed")
        endif ()
    endif ()

    # get commit id from file
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

#set(PROJECT_BUILD_PATH ${PROJECT_SOURCE_DIR}/build)
set(PROJECT_BUILD_PATH ${PROJECT_BINARY_DIR})
set(PROJECT_3RDPARTY_SRC_DIR ${PROJECT_SOURCE_DIR}/3rdparty)
set(PROJECT_3RDPARTY_BIN_DIR ${PROJECT_SOURCE_DIR}/output/3rdparty)
set(PROJECT_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/output)

# set src base dir of util
set(PROJECT_UTIL_SRC_BASE ${PROJECT_SOURCE_DIR}/src/util/csrc)

# set src base dir and output dir of smem
set(PROJECT_SMEM_SRC_BASE ${PROJECT_SOURCE_DIR}/src/smem)
set(PROJECT_SMEM_OUTPUT ${PROJECT_SOURCE_DIR}/output/smem)

# set src base dir and output dir of memcache
set(PROJECT_MMC_SRC_BASE ${PROJECT_SOURCE_DIR}/src/memcache)
set(PROJECT_MMC_OUTPUT ${PROJECT_SOURCE_DIR}/output/memcache)

# set src base dir and output dir of smem
set(PROJECT_HYBM_SRC_BASE ${PROJECT_SOURCE_DIR}/src/hybm)
set(PROJECT_HYBM_OUTPUT ${PROJECT_SOURCE_DIR}/output/hybm)
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

if(BUILD_PYTHON STREQUAL "ON")
    # 获取Python头文件路径
    find_program(PYTHON_EXECUTABLE NAMES python3 python)
    if(NOT PYTHON_EXECUTABLE)
        message(FATAL_ERROR "Python not found")
    endif()

    execute_process(
            COMMAND ${PYTHON_EXECUTABLE} -c
            "import sysconfig; print(sysconfig.get_path('include'))"
            OUTPUT_VARIABLE PYTHON_INCLUDE_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
            COMMAND ${PYTHON_EXECUTABLE} -c
            "import sys; import os; import pybind11; print(os.path.dirname(pybind11.__file__))"
            OUTPUT_VARIABLE PYTHON_PYBIND11_HOME
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(pybind11_cmake_dir "${PYTHON_PYBIND11_HOME}/share/cmake/pybind11")

    # 检查路径是否存在且是目录
    if(NOT EXISTS "${pybind11_cmake_dir}" OR NOT IS_DIRECTORY "${pybind11_cmake_dir}")
        message(SEND_ERROR "Can not find pybind11 directory: ${pybind11_cmake_dir}\nPlease 'pip3 install pybind11 or pybind11-dev'")
    endif()
endif()

# 判断环境变量 CMAKE_PREFIX_PATH 是否存在
if(DEFINED ENV{CMAKE_PREFIX_PATH})
    # 如果环境变量存在，将其值赋给 CMake 变量
    set(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")
    list(APPEND CMAKE_PREFIX_PATH "${pybind11_cmake_dir}")
    message(STATUS "CMAKE_PREFIX_PATH is exist, and append: ${pybind11_cmake_dir}")
    message(STATUS "Now CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
else()
    # 如果环境变量不存在，设置 CMake 变量的默认值
    set(CMAKE_PREFIX_PATH "${pybind11_cmake_dir}")
    message(STATUS "Not set CMAKE_PREFIX_PATH, and initialize to: ${CMAKE_PREFIX_PATH}")
endif()

include_directories(
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SOURCE_DIR}/include/hybm
)

add_subdirectory(3rdparty)
add_subdirectory(src)

if(BUILD_TESTS STREQUAL "ON")
    set(CMAKE_CXX_STANDARD 14)
    message(STATUS "BUILD_TESTS = ON, add compile gov")
    add_subdirectory(test)
endif()