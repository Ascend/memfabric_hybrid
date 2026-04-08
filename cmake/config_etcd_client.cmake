# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

####################################################################
# etcd client
####################################################################
message(STATUS "BUILD_ETCD_BACKEND = ${BUILD_ETCD_BACKEND}")
if (BUILD_ETCD_BACKEND)
    set(ETCD_SRC_DIR ${PROJECT_SOURCE_DIR}/src/util/etcd_client/etcd_store_backend)
    set(ETCD_OUTPUT_DIR ${PROJECT_OUTPUT_PATH}/etcd/lib64)
    set(ETCD_SO_FILE ${ETCD_OUTPUT_DIR}/libetcd_client_v3.so)

    find_program(GO_EXECUTABLE go)
    if (NOT GO_EXECUTABLE)
        message(FATAL_ERROR "Go is not installed. Cannot build etcd client.")
    endif ()
    message(STATUS "GO_EXECUTABLE: ${GO_EXECUTABLE}")

    add_custom_command(
        OUTPUT ${ETCD_SO_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ETCD_OUTPUT_DIR}
        COMMAND ${GO_EXECUTABLE} mod tidy
        COMMAND ${GO_EXECUTABLE} build -o libetcd_client_v3.so -buildmode=c-shared etcd_client_v3.go
        COMMAND ${CMAKE_COMMAND} -E copy libetcd_client_v3.so ${ETCD_OUTPUT_DIR}/
        WORKING_DIRECTORY ${ETCD_SRC_DIR}
        COMMENT "Building etcd client shared library"
        VERBATIM
    )

    add_custom_target(etcd_client ALL DEPENDS ${ETCD_SO_FILE})

    install(
        FILES ${ETCD_SO_FILE}
        DESTINATION ${TARGET_INSTALL_DIR}/etcd/lib64
    )

    set(ETCD_LIBS_DIR ${ETCD_OUTPUT_DIR} PARENT_SCOPE)
    message(STATUS "ETCD_LIBS_DIR: ${ETCD_LIBS_DIR}")
endif ()
