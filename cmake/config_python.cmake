# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

if (BUILD_PYTHON STREQUAL "ON")
    # 获取Python头文件路径
    find_program(PYTHON_EXECUTABLE NAMES python3 python)
    if (NOT PYTHON_EXECUTABLE)
        message(FATAL_ERROR "Python not found")
    endif ()

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
    if (NOT EXISTS "${pybind11_cmake_dir}" OR NOT IS_DIRECTORY "${pybind11_cmake_dir}")
        message(SEND_ERROR "Can not find pybind11 directory: ${pybind11_cmake_dir}\nPlease 'pip3 install pybind11 or pybind11-dev'")
    endif ()
endif ()