# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

include_directories(
        ${PROJECT_HYBM_SRC_BASE}/include
        ${PROJECT_HYBM_SRC_BASE}/csrc/common
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation/host
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver/userspace
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver/version
        ${PROJECT_HYBM_SRC_BASE}/csrc/entity
        ${PROJECT_HYBM_SRC_BASE}/csrc/mm
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/compose
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/device
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/host
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/utils
        ${PROJECT_HYBM_SRC_BASE}/csrc/ts_engine
        ${PROJECT_HYBM_SRC_BASE}/csrc/under_api

        ${PROJECT_UTIL_SRC_BASE}/ptracer/include

        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_shm
        ${PROJECT_SMEM_SRC_BASE}/csrc/net
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/backend
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/ha
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/tcp_store
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_bm

        ${PROJECT_ACCLINKS_SRC_BASE}/include
        ${PROJECT_ACCLINKS_SRC_BASE}/csrc
        ${PROJECT_ACCLINKS_SRC_BASE}/csrc/common
        ${PROJECT_ACCLINKS_SRC_BASE}/csrc/security
        ${PROJECT_ACCLINKS_SRC_BASE}/csrc/under_api/openssl

        ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/include/
)

file(GLOB_RECURSE MEMFABRIC_TEST_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/hybm/*
        ${CMAKE_CURRENT_SOURCE_DIR}/smem/*
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/*
        ${CMAKE_CURRENT_SOURCE_DIR}/acc_links/*
        ${CMAKE_CURRENT_SOURCE_DIR}/../ut_main.cpp
)

set(TEST_TARGET_NAME test_memfabric)

add_executable(${TEST_TARGET_NAME} ${MEMFABRIC_TEST_SRCS}
        $<TARGET_OBJECTS:ptracer_object>
        $<TARGET_OBJECTS:config_store_object>
        $<TARGET_OBJECTS:smem_objects>
        $<TARGET_OBJECTS:hybmm_objects>
        $<TARGET_OBJECTS:acc_tcp_net_object>
)

target_compile_options(${TEST_TARGET_NAME} PRIVATE -w -fpermissive --coverage)
target_link_directories(${TEST_TARGET_NAME} PUBLIC ${PROJECT_OUTPUT_PATH}/acc_links/lib64/)
target_link_directories(${TEST_TARGET_NAME} PUBLIC ${GTEST_BUILD_LIB_DIR}/lib)
target_link_directories(${TEST_TARGET_NAME} PUBLIC ${PROJECT_OUTPUT_PATH}/smem/lib64)
target_link_directories(${TEST_TARGET_NAME} PUBLIC ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/lib64)

target_link_libraries(${TEST_TARGET_NAME} PUBLIC gtest gcov mockcpp dl pthread)

install(TARGETS ${TEST_TARGET_NAME}
        RUNTIME DESTINATION ${PROJECT_OUTPUT_PATH}/bin/ut
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)