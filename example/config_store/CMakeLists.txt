if ("${BUILD_TEST}" STREQUAL "ON")
    set(ASCEND_CANN_PACKAGE_PATH $ENV{ASCEND_HOME_PATH})
    message(STATUS "ASCEND_CANN_PACKAGE_PATH = ${ASCEND_CANN_PACKAGE_PATH}")
    if ("${ASCEND_CANN_PACKAGE_PATH}" STREQUAL "")
        message(WARNING "ascend cann path not found!Please set the environment variable: ASCEND_HOME_PATH")
        return()
    endif ()
    add_executable(config_store_etcd_test ${CMAKE_CURRENT_SOURCE_DIR}/config_store_etcd_test.cpp)

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(TARGET_ARCH "x86_64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(TARGET_ARCH "aarch64")
    else ()
        message(FATAL_ERROR "arch is not support!")
    endif ()

    target_compile_options(config_store_etcd_test PRIVATE -D_GLIBCXX_USE_CXX11_ABI=0)

    include_directories(
            ${PROJECT_SOURCE_DIR}/src/smem/include/host
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/include/
    )
    target_link_directories(config_store_etcd_test PUBLIC
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/lib64
    )

    target_link_libraries(config_store_etcd_test PRIVATE
            stdc++
            hybmm_shared
            smem_shared
            ascendcl
    )
    target_compile_options(config_store_etcd_test PRIVATE -w)
    target_compile_options(config_store_etcd_test PRIVATE -Wno-sign-compare)

    set_target_properties(config_store_etcd_test PROPERTIES OUTPUT_NAME "config_store_etcd_test")

    install(TARGETS config_store_etcd_test
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
    add_executable(config_store_etcd_test ${CMAKE_CURRENT_SOURCE_DIR}/config_store_etcd_test.cpp)

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(TARGET_ARCH "x86_64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(TARGET_ARCH "aarch64")
    else ()
        message(FATAL_ERROR "arch is not support!")
    endif ()

    target_compile_options(config_store_etcd_test PRIVATE
            -O2 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -g
    )

    include_directories(
            ${MF_PATH}/include/smem/host
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/include/
    )
    message(STATUS "include: ${MF_PATH}/include/smem/host")
    target_link_directories(config_store_etcd_test PUBLIC
            ${MF_PATH}/${TARGET_ARCH}-linux/lib64
            ${ASCEND_CANN_PACKAGE_PATH}/${TARGET_ARCH}-linux/lib64
    )

    target_link_libraries(config_store_etcd_test PRIVATE
            stdc++
            mf_smem
            mf_hybm_core
            ascendcl
    )

    set_target_properties(config_store_etcd_test PROPERTIES OUTPUT_NAME "config_store_etcd_test")
    set_target_properties(config_store_etcd_test PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    set_target_properties(config_store_etcd_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/)
endif ()
