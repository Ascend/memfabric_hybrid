# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

if (${CMAKE_INSTALL_PREFIX} MATCHES "/usr/local")
    set(DEPS_INSTALL_DIR ${PROJECT_OUTPUT_PATH}/3rdparty)
elseif (NOT EXISTS ${CMAKE_INSTALL_PREFIX})
    set(DEPS_INSTALL_DIR ${PROJECT_OUTPUT_PATH}/3rdparty)
else ()
    set(DEPS_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/3rdparty)
endif ()

####################################################################
# hcom
####################################################################
message(STATUS "BUILD_HCOM = ${BUILD_HCOM}")
if (BUILD_HCOM)
    set(BUILD_TESTS OFF)
    include(FetchContent)

    find_program(CMAKE_C_COMPILER_PATH NAMES ${CMAKE_C_COMPILER} REQUIRED)
    find_program(CMAKE_CXX_COMPILER_PATH NAMES ${CMAKE_CXX_COMPILER} REQUIRED)

    set(CMAKE_C_COMPILER ${CMAKE_C_COMPILER_PATH} CACHE FILEPATH "C compiler" FORCE)
    set(CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER_PATH} CACHE FILEPATH "CXX compiler" FORCE)
    set(CMAKE_INSTALL_PREFIX ${DEPS_INSTALL_DIR}/hcom)
    FetchContent_Declare(
            hcom
            GIT_REPOSITORY https://atomgit.com/openeuler/ubs-comm.git
            GIT_TAG dev-kvcache
    )

    message(STATUS "Configuring hcom with CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}, options: BUILD_WITH_RDMA:${BUILD_WITH_RDMA} BUILD_WITH_UB:${BUILD_WITH_UB}")
    FetchContent_MakeAvailable(hcom)

    FetchContent_GetProperties(hcom)
    set(HCOM_SOURCE_DIR ${hcom_SOURCE_DIR})
    set(HCOM_BINARY_DIR ${hcom_BINARY_DIR})

    set(HCOM_INCLUDE_DIR ${HCOM_SOURCE_DIR}/src)
    set(HCOM_LIBS_DIR ${HCOM_BINARY_DIR}/src)

    set(HCOM_LIBS_DIR ${HCOM_LIBS_DIR} PARENT_SCOPE)
    set(HCOM_INCLUDE_DIR ${HCOM_INCLUDE_DIR} PARENT_SCOPE)

    message(STATUS "HCOM_LIBS_DIR: ${HCOM_LIBS_DIR}")
    message(STATUS "HCOM_INCLUDE_DIR: ${HCOM_INCLUDE_DIR}")

    set(HCOM_REAL_STATIC_LIB "${HCOM_BINARY_DIR}/src/hcom/libhcom_static.a")
    set(HCOM_FAKE_STATIC_LIB "${CMAKE_BINARY_DIR}/src/hcom/libhcom_static.a")

    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/src/hcom")

    message(STATUS "CMAKE_COMMAND = ${CMAKE_COMMAND} ${HCOM_REAL_STATIC_LIB} ${HCOM_FAKE_STATIC_LIB}")

    execute_process(
            COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${HCOM_REAL_STATIC_LIB}" "${HCOM_FAKE_STATIC_LIB}"
            RESULT_VARIABLE SYMLINK_RESULT
    )

    if (SYMLINK_RESULT EQUAL 0)
        message(STATUS "Created symlink for hcom install: ${HCOM_FAKE_STATIC_LIB} -> ${HCOM_REAL_STATIC_LIB}")
    else ()
        message(WARNING "Failed to create symlink for hcom static lib. Install may fail.")
    endif ()

    macro(TARGET_HCOM target)
        if (TARGET hcom)
            target_link_libraries(${target} PUBLIC hcom)
            target_include_directories(${target} PUBLIC ${HCOM_INCLUDE_DIR})
        else ()
            add_dependencies(${target} hcom)
            target_include_directories(${target} PUBLIC ${HCOM_INCLUDE_DIR})
            target_link_directories(${target} PUBLIC ${HCOM_LIBS_DIR})
        endif ()
    endmacro()

    macro(TARGET_HCOM_HEADER_ONLY target)
        add_dependencies(${target} hcom)
        target_include_directories(${target} PUBLIC ${HCOM_INCLUDE_DIR})
    endmacro()
endif ()