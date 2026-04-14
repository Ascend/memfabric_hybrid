cmake_minimum_required(VERSION 3.16)
project(Ascend_c)

set(ASCEND_CANN_PACKAGE_PATH $ENV{ASCEND_HOME_PATH})
if ("${ASCEND_CANN_PACKAGE_PATH}" STREQUAL "")
    message(FATAL_ERROR "ascend cann path not found!")
endif ()

message(STATUS "ENV_TYPE:${ENV_VERSION}")
if(ENV_VERSION STREQUAL "A5")
    set(CCE_AICORE_ARCH
            --cce-aicore-arch=dav-c310
    )
else()
    set(CCE_AICORE_ARCH
            --cce-aicore-arch=dav-c220
    )
endif()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type Release/Debug (default Debug)" FORCE)
endif ()
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/out)

set(MF_PATH $ENV{MEMFABRIC_HYBRID_HOME_PATH})
if ("${MF_PATH}" STREQUAL "")
    message(FATAL_ERROR "memfabric_hybrid path not found!")
endif ()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
    set(TARGET_ARCH "x86_64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(TARGET_ARCH "aarch64")
else ()
    message(FATAL_ERROR "arch is not support!")
endif ()

execute_process(
        COMMAND bisheng -x asc ${CMAKE_CURRENT_SOURCE_DIR}/shm_all_shift.cpp -shared -g -o out/libshm_example_kernel.so ${CCE_AICORE_ARCH} -I${MF_PATH}/include/smem/device
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE BISHENG_RESULT
)
if(BISHENG_RESULT EQUAL 0)
    message(STATUS "build kernel ok")
else()
    message(FATAL_ERROR "build kernel failed ${BISHENG_RESULT}")
endif()

add_executable(shm_example ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_compile_options(shm_example PRIVATE -O2 -g)
target_include_directories(shm_example PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/include
        ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/include/acl
        ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/include/experiment/runtime/runtime
        ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/include/experiment/msprof

        ${CMAKE_CURRENT_SOURCE_DIR}
        ${MF_PATH}/include/smem/host
)
target_link_directories(shm_example PUBLIC
        ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/lib64
        ${CMAKE_CURRENT_SOURCE_DIR}/out/
        ${MF_PATH}/${TARGET_ARCH}-linux/lib64
        /usr/lib64/
)
target_link_libraries(shm_example PRIVATE
        mf_smem
        shm_example_kernel
        ascendcl
)
target_compile_options(shm_example PRIVATE ${MPI_CXX_COMPILE_FLAGS})

install(TARGETS shm_example
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
