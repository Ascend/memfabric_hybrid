project(pysmem LANGUAGES CXX)

add_compile_options(-fabi-version=0)
add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)

if(BUILD_PYTHON STREQUAL "ON")
    find_program(PYTHON_EXECUTABLE NAMES python3 python)

    # python
    message(STATUS "PYTHON_EXECUTABLE:${PYTHON_EXECUTABLE}")
    execute_process(
            COMMAND ${PYTHON_EXECUTABLE} "-c"
            "from __future__ import print_function; import os; import pybind11;
print(os.path.dirname(pybind11.__file__),end='');"
            RESULT_VARIABLE _PYTHON_SUCCESS
            OUTPUT_VARIABLE PYBIND11_DIR)

    if(NOT _PYTHON_SUCCESS MATCHES 0)
        message(STATUS "PYBIND11_DIR: ${PYBIND11_DIR}")
        message(FATAL_ERROR "Pybind11 config Error.")
    endif()

    # pybind11
    message(STATUS "PYBIND11_DIR = ${PYBIND11_DIR}")
    list(APPEND CMAKE_PREFIX_PATH ${PYBIND11_DIR})

    find_package(pybind11 REQUIRED)
    pybind11_add_module(_pysmem pysmem.cpp)
    set_target_properties(_pysmem PROPERTIES CXX_STANDARD 17)
    set_target_properties(_pysmem PROPERTIES OUTPUT_NAME "_pymf_smem")
    target_link_libraries(_pysmem PRIVATE smem_shared)

endif ()
