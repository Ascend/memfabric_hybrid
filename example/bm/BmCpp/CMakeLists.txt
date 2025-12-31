if ("${BUILD_TEST}" STREQUAL "ON")
    set(ASCEND_CANN_PACKAGE_PATH $ENV{ASCEND_HOME_PATH})
    message(STATUS "ASCEND_CANN_PACKAGE_PATH = ${ASCEND_CANN_PACKAGE_PATH}")
    if ("${ASCEND_CANN_PACKAGE_PATH}" STREQUAL "")
        message(WARNING "ascend cann path not found!Please set the environment variable: ASCEND_HOME_PATH")
        return()
    endif ()
    add_executable(bm_example ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(TARGET_ARCH "x86_64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(TARGET_ARCH "aarch64")
    else ()
        message(FATAL_ERROR "arch is not support!")
    endif ()

    target_compile_options(bm_example PRIVATE -D_GLIBCXX_USE_CXX11_ABI=0)

    include_directories(
            ${PROJECT_SOURCE_DIR}/src/smem/include/host
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/include/
    )
    target_link_directories(bm_example PUBLIC
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/lib64
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/lib64/plugin/opskernel
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/lib64/lib64/plugin/nnengine
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/devlib
    )

    target_link_libraries(bm_example PRIVATE
            hybmm_shared
            smem_shared
            ascendcl
    )
    target_compile_options(bm_example PRIVATE -W)
    target_compile_options(bm_example PRIVATE -Wno-sign-compare)

    set_target_properties(bm_example PROPERTIES OUTPUT_NAME "bm_example")

    install(TARGETS bm_example
            RUNTIME DESTINATION ${TARGET_INSTALL_DIR}/smem/bin
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
else ()
    cmake_minimum_required(VERSION 3.12)
    project(BM_EXAMPLE)
    message(STATUS "BUILD_TEST is empty, Compile separately")

    set(MF_PATH $ENV{MEMFABRIC_HYBRID_HOME_PATH})
    if ("${MF_PATH}" STREQUAL "")
        message(FATAL_ERROR "memfabric_hybrid path not found!")
    endif ()
    set(ASCEND_CANN_PACKAGE_PATH $ENV{ASCEND_HOME_PATH})
    if ("${ASCEND_CANN_PACKAGE_PATH}" STREQUAL "")
        message(FATAL_ERROR "ascend cann path not found!")
    endif ()
    add_executable(bm_example ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(TARGET_ARCH "x86_64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(TARGET_ARCH "aarch64")
    else ()
        message(FATAL_ERROR "arch is not support!")
    endif ()

    target_compile_options(bm_example PRIVATE
            -O2 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -g
    )

    include_directories(
            ${MF_PATH}/${TARGET_ARCH}-linux/include/smem/include/host
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/include/
    )
    message(STATUS "include: ${MF_PATH}/${TARGET_ARCH}-linux/include/smem/host")
    target_link_directories(bm_example PUBLIC
            ${MF_PATH}/${TARGET_ARCH}-linux/lib64
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/lib64
    )

    target_link_libraries(bm_example PRIVATE
            mf_smem
            mf_hybm_core
            ascendcl
    )

    set_target_properties(bm_example PROPERTIES OUTPUT_NAME "bm_example")
    set_target_properties(bm_example PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    set_target_properties(bm_example PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/)
endif ()
