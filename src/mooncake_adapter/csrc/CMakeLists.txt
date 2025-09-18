if (CMAKE_C_COMPILER STREQUAL gcc)
    add_compile_options(-fabi-version=0)
endif ()
add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
include_directories(
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc/under_api/hybm_core
        ${PROJECT_ADAPTER_SRC_BASE}/csrc/common
        ${PROJECT_ADAPTER_SRC_BASE}/csrc/util
)

if (BUILD_PYTHON STREQUAL "ON")
    file(GLOB_RECURSE MOONCAKE_SRC *.cpp)
    add_library(pytransfer OBJECT  ${MOONCAKE_SRC})
    find_package(pybind11 REQUIRED)
    include_directories(
            ${Python3_INCLUDE_DIRS}
            ${pybind11_INCLUDE_DIRS}
    )
    pybind11_add_module(_pytransfer transfer/pytransfer.cpp util/transfer_util.cpp)
    set_target_properties(_pytransfer PROPERTIES CXX_STANDARD 17)
    set_target_properties(_pytransfer PROPERTIES OUTPUT_NAME "_pymf_transfer")
    target_link_libraries(_pytransfer PRIVATE smem_shared)
endif ()