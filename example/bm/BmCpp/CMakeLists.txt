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
add_executable(bm_example ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_compile_options(bm_example PRIVATE
        -O2 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

include_directories(
        ${MF_PATH}/aarch64-linux/include/smem/host
        ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/include/
)
target_link_directories(bm_example PUBLIC
        ${MF_PATH}/aarch64-linux/lib
        ${MF_PATH}/aarch64-linux/lib64
        ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/lib64
)

target_link_libraries(bm_example PRIVATE
        mf_smem
        ascendcl
)

set_target_properties(bm_example PROPERTIES OUTPUT_NAME "bm_example")
set_target_properties(bm_example PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(bm_example PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/)
