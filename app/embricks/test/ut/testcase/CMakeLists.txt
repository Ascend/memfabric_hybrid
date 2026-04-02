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
        ${EMBRICKS_SRC_DIR}/include
        ${EMBRICKS_SRC_DIR}/csrc/common
        ${EMBRICKS_SRC_DIR}/csrc/hashmap
        ${EMBRICKS_SRC_DIR}/csrc/net
        ${EMBRICKS_SRC_DIR}/csrc/proxy
        ${EMBRICKS_SRC_DIR}/csrc/service
        ${EMBRICKS_SRC_DIR}/csrc/table
        ${EMBRICKS_SRC_DIR}/csrc/under_api
)

file(GLOB_RECURSE EMBRICKS_TEST_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/csrc/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

set(TEST_TARGET_NAME test_embricks)

add_executable(${TEST_TARGET_NAME} ${EMBRICKS_TEST_SRCS}
        $<TARGET_OBJECTS:common_objects>
        $<TARGET_OBJECTS:hashmap_objects>
)

target_compile_options(${TEST_TARGET_NAME} PRIVATE -w)
target_link_directories(${TEST_TARGET_NAME} PUBLIC ${PROJECT_OUTPUT_PATH}/embricks/lib64/)

target_link_options(${TEST_TARGET_NAME} PRIVATE -pie)
target_link_libraries(${TEST_TARGET_NAME} PUBLIC gtest)

install(TARGETS ${TEST_TARGET_NAME}
        RUNTIME DESTINATION ${PROJECT_OUTPUT_PATH}/bin/ut
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)