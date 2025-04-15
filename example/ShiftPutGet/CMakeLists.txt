cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

set(RUN_MODE "npu" CACHE STRING "cpu/sim/npu")
set(SOC_VERSION "Ascend910B3" CACHE STRING "system on chip type")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/ascend-toolkit/latest"
    CACHE STRING "ASCEND CANN package installation directory"
)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type Release/Debug (default Debug)" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX STREQUAL /usr/local)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/out" CACHE STRING "path for install()" FORCE)
endif()

set(SMEM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src/smem/include/device/)

# ${KERNEL_FILES} are used to compile library, push files written by ascendc in ${KERNEL_FILES}.
# ref to cmake/npu.cmake ascendc_library, cmake/cpu.cmake add_library
file(GLOB KERNEL_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/shm_all_shift.cpp
)

if("${RUN_MODE}" STREQUAL "cpu")
    include(cmake/cpu_lib.cmake)
elseif("${RUN_MODE}" STREQUAL "sim" OR "${RUN_MODE}" STREQUAL "npu")
    include(cmake/npu_lib.cmake)
else()
    message("invalid RUN_MODE: ${RUN_MODE}")
endif()
add_executable(shm_kernels ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_compile_options(shm_kernels PRIVATE
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:-g>>
    -O2 -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Werror
)

set(SMEM_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../output)
include_directories(${SMEM_LIB_DIR}/smem/include/host)
target_link_directories(shm_kernels PUBLIC ${SMEM_LIB_DIR}/smem/lib ${SMEM_LIB_DIR}/hybmm/lib)

target_link_libraries(shm_kernels PRIVATE
    $<BUILD_INTERFACE:$<$<OR:$<STREQUAL:${RUN_MODE},npu>,$<STREQUAL:${RUN_MODE},sim>>:host_intf_pub>>
    $<BUILD_INTERFACE:$<$<STREQUAL:${RUN_MODE},cpu>:ascendcl>>
    ascendc_kernels_${RUN_MODE}
    mf_smem
    mf_hybrid_bmm
)

install(TARGETS shm_kernels
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
