cmake_minimum_required(VERSION 3.12.0)
project(SMEM VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)

option(BUILD_EXAMPLE "build example or not" ON)
option(BUILD_TESTS "build test or not" OFF)
option(BUILD_OPEN_ABI "build open _GLIBCXX_USE_CXX11_ABI" ON)
option(ENABLE_CPU_MONOTONIC "enable monotonic time using cpu hard instruction" OFF)

message(STATUS "BUILD_EXAMPLE = ${BUILD_EXAMPLE}")
message(STATUS "BUILD_TESTS = ${BUILD_TESTS}")
message(STATUS "BUILD_USE_CXX11_ABI = ${BUILD_OPEN_ABI}")
message(STATUS "ENABLE_CPU_MONOTONIC = ${ENABLE_CPU_MONOTONIC}")

message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

#set(CMAKE_VERBOSE_MAKEFILE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

if (${CMAKE_BUILD_TYPE} MATCHES "RELEASE")
    add_compile_options(-O2 -fpic)
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