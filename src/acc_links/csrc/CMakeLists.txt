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
        ${PROJECT_ACCLINKS_SRC_BASE}/include
        ${PROJECT_ACCLINKS_SRC_BASE}/csrc/
        ${PROJECT_ACCLINKS_SRC_BASE}/csrc/common
        ${PROJECT_ACCLINKS_SRC_BASE}/csrc/security
        ${PROJECT_ACCLINKS_SRC_BASE}/csrc/under_api/openssl
)

file(GLOB_RECURSE ACC_TCP_NET_SRCS *.cpp)
add_library(acc_tcp_net_object OBJECT ${ACC_TCP_NET_SRCS})
add_library(acc_tcp_net_static STATIC $<TARGET_OBJECTS:acc_tcp_net_object>)
set_target_properties(acc_tcp_net_static PROPERTIES OUTPUT_NAME "acc_tcp_net")
set_target_properties(acc_tcp_net_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
target_link_libraries(acc_tcp_net_static
        -Wl,--start-group
        pthread dl rt
        -Wl,--end-group
)

add_library(acc_tcp_net_shared SHARED $<TARGET_OBJECTS:acc_tcp_net_object>)
set_target_properties(acc_tcp_net_shared PROPERTIES OUTPUT_NAME "acc_tcp_net")
set_target_properties(acc_tcp_net_shared PROPERTIES CLEAN_DIRECT_OUTPUT 1)
target_link_libraries(acc_tcp_net_shared
        -Wl,--start-group
        pthread dl rt
        -Wl,--end-group
)

file(GLOB HEADER_FILES ../include/*.h)
install(FILES ${HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/acc_links/include)
install(TARGETS acc_tcp_net_shared acc_tcp_net_static
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/acc_links/lib64
        ARCHIVE DESTINATION ${TARGET_INSTALL_DIR}/acc_links/lib64
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
)