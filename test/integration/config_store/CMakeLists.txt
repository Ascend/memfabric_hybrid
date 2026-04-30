if ("${BUILD_TEST}" STREQUAL "ON")
    add_executable(config_store_etcd_test ${CMAKE_CURRENT_SOURCE_DIR}/config_store_etcd_test.cpp)
    target_compile_options(config_store_etcd_test PRIVATE
            -D_GLIBCXX_USE_CXX11_ABI=0
            -w
            -Wno-sign-compare
    )
    target_include_directories(config_store_etcd_test PRIVATE
            ${PROJECT_SOURCE_DIR}/src/smem/include/host
    )

    target_link_libraries(config_store_etcd_test PRIVATE
            stdc++
            hybmm_shared
            smem_shared
    )
    if (BUILD_ETCD_BACKEND)
        add_dependencies(config_store_etcd_test etcd_client)
        set_target_properties(config_store_etcd_test PROPERTIES
                BUILD_RPATH "${PROJECT_OUTPUT_PATH}/etcd/lib64"
        )
    endif ()

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
    add_executable(config_store_etcd_test ${CMAKE_CURRENT_SOURCE_DIR}/config_store_etcd_test.cpp)

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(TARGET_ARCH "x86_64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(TARGET_ARCH "aarch64")
    else ()
        message(FATAL_ERROR "arch is not support!")
    endif ()

    target_compile_options(config_store_etcd_test PRIVATE
            -O2 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -g -w -Wno-sign-compare
    )

    target_include_directories(config_store_etcd_test PRIVATE
            ${MF_PATH}/include/smem/host
    )
    message(STATUS "include: ${MF_PATH}/include/smem/host")
    target_link_directories(config_store_etcd_test PUBLIC
            ${MF_PATH}/${TARGET_ARCH}-linux/lib64
    )

    target_link_libraries(config_store_etcd_test PRIVATE
            stdc++
            mf_smem
            mf_hybm_core
    )

    set_target_properties(config_store_etcd_test PROPERTIES OUTPUT_NAME "config_store_etcd_test")
    set_target_properties(config_store_etcd_test PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    set_target_properties(config_store_etcd_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/)
endif ()
