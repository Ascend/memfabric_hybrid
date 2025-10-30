cmake_minimum_required(VERSION 3.12)
project(BM_EXAMPLE)

set(MF_PATH $ENV{MEMFABRIC_HYBRID_HOME_PATH})
if("${MF_PATH}" STREQUAL "")
    message(FATAL_ERROR "memfabric_hybrid path not found!")
endif()
set(ASCEND_CANN_PACKAGE_PATH $ENV{ASCEND_HOME_PATH})
if("${ASCEND_CANN_PACKAGE_PATH}" STREQUAL "")
    message(FATAL_ERROR "ascend cann path not found!")
endif()
add_executable(bm_perf_benchmark ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
    set(TARGET_ARCH "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(TARGET_ARCH "aarch64")
else()
    message(FATAL_ERROR "arch is not support!")
endif()

target_compile_options(bm_perf_benchmark PRIVATE
        -O2 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -g
)

include_directories(
        ${MF_PATH}/${TARGET_ARCH}-linux/include/smem/host
        ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/include/
        ${MF_PATH}/${TARGET_ARCH}-linux/include/util/
)

target_link_directories(bm_perf_benchmark PUBLIC
        ${MF_PATH}/${TARGET_ARCH}-linux/lib64
        ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/lib64
)

target_link_libraries(bm_perf_benchmark PRIVATE
        mf_smem
        mf_hybm_core
        ascendcl
)

set_target_properties(bm_perf_benchmark PROPERTIES OUTPUT_NAME "bm_perf_benchmark")
set_target_properties(bm_perf_benchmark PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(bm_perf_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/)
