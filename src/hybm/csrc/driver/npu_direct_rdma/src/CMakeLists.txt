# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# Invoke the Makefile to build the kernel module
execute_process(
        COMMAND ${CMAKE_MAKE_PROGRAM} -C ${NDR_SRC_DIR}
        WORKING_DIRECTORY ${NDR_SRC_DIR}
)

# Install
file(COPY ${NDR_SRC_DIR}/ndr_peer_mem.ko DESTINATION ${NDR_OUTPUT_PATH}/NDR/lib)

# Delete intermediate files 
file(GLOB CMD_FILES "${NDR_SRC_DIR}/*.cmd")
file(REMOVE ${CMD_FILES})