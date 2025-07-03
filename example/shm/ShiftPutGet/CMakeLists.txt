cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B3" CACHE STRING "system on chip type")
set(ASCEND_CANN_PACKAGE_PATH $ENV{ASCEND_HOME_PATH})
if("${ASCEND_CANN_PACKAGE_PATH}" STREQUAL "")
    message(FATAL_ERROR "ascend cann path not found!")
endif()
set(ASCENDC_CMAKE_DIR ${ASCEND_CANN_PACKAGE_PATH}/compiler/tikcpp/ascendc_kernel_cmake)
include(${ASCENDC_CMAKE_DIR}/ascendc.cmake)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type Release/Debug (default Debug)" FORCE)
endif()
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_LIST_DIR}/out)

set(MF_PATH $ENV{MEMFABRIC_HYBRID_HOME_PATH})
if("${MF_PATH}" STREQUAL "")
    message(FATAL_ERROR "memfabric_hybrid path not found!")
endif()

ascendc_library(shm_shift_kernels SHARED ${CMAKE_CURRENT_LIST_DIR}/shm_all_shift.cpp)
ascendc_include_directories(shm_shift_kernels
        PUBLIC
        ${MF_PATH}/aarch64-linux/include/smem/device/
)

add_executable(shm_example ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_compile_options(shm_example PRIVATE
    -O2 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

include_directories(${MF_PATH}/aarch64-linux/include/smem/host)
target_link_directories(shm_example PUBLIC ${MF_PATH}/aarch64-linux/lib64)

target_link_libraries(shm_example PRIVATE
    shm_shift_kernels
    mf_smem
)

install(TARGETS shm_example
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
