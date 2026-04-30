if ("${BUILD_TEST}" STREQUAL "ON")
    set(ASCEND_CANN_PACKAGE_PATH $ENV{ASCEND_HOME_PATH})
    message(STATUS "ASCEND_CANN_PACKAGE_PATH = ${ASCEND_CANN_PACKAGE_PATH}")
    if ("${ASCEND_CANN_PACKAGE_PATH}" STREQUAL "")
        message(WARNING "ascend cann path not found!Please set the environment variable: ASCEND_HOME_PATH")
        return()
    endif ()
    file(GLOB BM_BENCHMARK_FILES *.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/*
    )
    add_executable(bm_perf_benchmark ${BM_BENCHMARK_FILES})

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(TARGET_ARCH "x86_64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(TARGET_ARCH "aarch64")
    else ()
        message(FATAL_ERROR "arch is not support!")
    endif ()

    target_compile_options(bm_perf_benchmark PRIVATE -D_GLIBCXX_USE_CXX11_ABI=0)

    include_directories(
            ${PROJECT_SOURCE_DIR}/src/smem/include/host
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/include/
    )
    target_link_directories(bm_perf_benchmark PUBLIC
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/lib64
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/lib64/plugin/opskernel
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/lib64/lib64/plugin/nnengine
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/devlib
    )

    target_link_libraries(bm_perf_benchmark PRIVATE
            stdc++
            hybmm_shared
            smem_shared
            ascendcl
    )
    target_compile_options(bm_perf_benchmark PRIVATE -w)
    target_compile_options(bm_perf_benchmark PRIVATE -Wno-sign-compare)
    target_compile_options(bm_perf_benchmark PRIVATE -Wno-stack-usage)
    set_target_properties(bm_perf_benchmark PROPERTIES OUTPUT_NAME "bm_perf_benchmark")

    install(TARGETS bm_perf_benchmark
            RUNTIME DESTINATION ${TARGET_INSTALL_DIR}/smem/bin
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
else ()
    cmake_minimum_required(VERSION 3.12)
    project(BM_EXAMPLE)

    set(MF_PATH $ENV{MEMFABRIC_HYBRID_HOME_PATH})
    if ("${MF_PATH}" STREQUAL "")
        message(FATAL_ERROR "memfabric_hybrid path not found!")
    endif ()
    set(ASCEND_CANN_PACKAGE_PATH $ENV{ASCEND_HOME_PATH})
    if ("${ASCEND_CANN_PACKAGE_PATH}" STREQUAL "")
        message(FATAL_ERROR "ascend cann path not found!")
    endif ()
    file(GLOB BM_BENCHMARK_FILES *.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/*
    )
    add_executable(bm_perf_benchmark ${BM_BENCHMARK_FILES})

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(TARGET_ARCH "x86_64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(TARGET_ARCH "aarch64")
    else ()
        message(FATAL_ERROR "arch is not support!")
    endif ()

    target_compile_options(bm_perf_benchmark PRIVATE
            -O2 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -g
    )

    include_directories(
            ${CMAKE_CURRENT_SOURCE_DIR}/
            ${MF_PATH}/include/smem/host
            ${MF_PATH}/include/hybm
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/include/
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
endif ()
