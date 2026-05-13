# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# ZBAL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

message(STATUS "build testcase is required")

include_directories(
        ${ASCEND_HOME_PATH}/include
        ${TORCH_DIR}/include/torch/csrc/api/include
        ${TORCH_DIR}/include
        ${TORCH_DIR}/include/torch/csrc/distributed
        ${TORCH_DIR}/include/torch/csrc/utils
        ${TORCH_DIR}/include/c10/util
        ${TORCH_NPU_DIR}/include
        ${PYBIND11_DIR}/include
        ${CPYTHON_DIR}
        ${ZBAL_BASE_DIR}
        ${ZBAL_BASE_DIR}/third_party/ska
        ${ZBAL_BASE_DIR}/third_party/mstx
        ${ZBAL_SRC_DIR}/include
        ${ZBAL_SRC_DIR}/csrc/adaptor/pytorch_npu
        ${ZBAL_SRC_DIR}/csrc/
        ${ZBAL_SRC_DIR}/csrc/operators
        ${ZBAL_SRC_DIR}/csrc/operators/npu
        ${ZBAL_SRC_DIR}/csrc/common
        ${ZBAL_SRC_DIR}/csrc/sma
        ${ZBAL_SRC_DIR}/csrc/under_api/cann
        ${ZBAL_SRC_DIR}/csrc/under_api/memfabric
        ${ZBAL_SRC_DIR}/csrc/bootstrap
        ${ZBAL_SRC_DIR}/csrc/bootstrap/memory
        ${ZBAL_SRC_DIR}/csrc/bootstrap/memory/memfabric
        ${ZBAL_TEST_3RDPARTY_OUTPUT_DIR}/googletest/include
)

file(GLOB_RECURSE ZBAL_TEST_CASE_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/csrc/ccl/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/csrc/common/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ut_main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../mock/acl_stub.cpp
)

add_executable(test_zbal ${ZBAL_TEST_CASE_SRCS}
        $<TARGET_OBJECTS:common_objects>
        $<TARGET_OBJECTS:operators_objects>
        $<TARGET_OBJECTS:under_api_objects>
        $<TARGET_OBJECTS:boostrap_objects>
        )

target_compile_options(test_zbal PRIVATE -fPIC -Wno-float-equal)
target_link_options(test_zbal PRIVATE -pie)
target_link_libraries(test_zbal PUBLIC gtest ascendcl runtime zbal_kernel)
#target_link_libraries(test_zbal PUBLIC gtest zbal_kernel acl_shared)

install(TARGETS test_zbal
        RUNTIME DESTINATION ${ZBAL_BASE_DIR}/output/ut
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)