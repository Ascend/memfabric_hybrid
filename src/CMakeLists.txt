# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# added very strict compile option here, but other module maybe need these
add_compile_options(-Wunused-variable -Wunused-value -Wcast-align)
add_compile_options(-fpermissive -Wno-error)
add_compile_options(-Wcast-qual -Winvalid-pch -Wwrite-strings -Wsign-compare -Wfloat-equal -Wextra -Wno-deprecated-declarations)

add_subdirectory(acc_links/csrc)
add_subdirectory(util/csrc/ptracer)
add_subdirectory(smem/csrc/config_store)
add_subdirectory(smem/csrc)
add_subdirectory(hybm/csrc)