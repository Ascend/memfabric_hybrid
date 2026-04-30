if ("${BUILD_TEST}" STREQUAL "ON")
    add_executable(etcd_backend_test ${CMAKE_CURRENT_SOURCE_DIR}/etcd_backend_test.cpp)
    target_compile_options(etcd_backend_test PRIVATE
            -D_GLIBCXX_USE_CXX11_ABI=0
            -w
            -Wno-sign-compare
    )
    target_include_directories(etcd_backend_test PRIVATE
            ${PROJECT_SOURCE_DIR}/src/smem/include/host
            ${PROJECT_SOURCE_DIR}/src/smem/csrc/config_store
            ${PROJECT_SOURCE_DIR}/src/smem/csrc/config_store/backend
            ${PROJECT_SOURCE_DIR}/src/smem/csrc/common

    )

    target_link_libraries(etcd_backend_test PRIVATE
            stdc++
            hybmm_shared
            smem_shared
            etcd_client_v3
    )
    if (BUILD_ETCD_BACKEND)
        add_dependencies(etcd_backend_test etcd_client)
        set_target_properties(etcd_backend_test PROPERTIES
                BUILD_RPATH "${PROJECT_OUTPUT_PATH}/etcd/lib64"
        )
    endif ()

    set_target_properties(etcd_backend_test PROPERTIES OUTPUT_NAME "etcd_backend_test")

    install(TARGETS etcd_backend_test
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
    add_executable(etcd_backend_test ${CMAKE_CURRENT_SOURCE_DIR}/etcd_backend_test.cpp)

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(TARGET_ARCH "x86_64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(TARGET_ARCH "aarch64")
    else ()
        message(FATAL_ERROR "arch is not support!")
    endif ()

    target_compile_options(etcd_backend_test PRIVATE
            -O2 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -g -w -Wno-sign-compare
    )

    target_include_directories(etcd_backend_test PRIVATE
            ${MF_PATH}/include/smem/host
            ../../src/smem/csrc/config_store
            ../../src/smem/csrc/config_store/backend
            ../../src/smem/csrc/common
    )

    target_link_directories(etcd_backend_test PUBLIC
            ${MF_PATH}/${TARGET_ARCH}-linux/lib64
    )

    target_link_libraries(etcd_backend_test PRIVATE
            stdc++
            etcd_client_v3
            mf_hybm_core
            mf_smem
    )
    if (BUILD_ETCD_BACKEND)
        add_dependencies(etcd_backend_test etcd_client)
        set_target_properties(etcd_backend_test PROPERTIES
                BUILD_RPATH "${PROJECT_OUTPUT_PATH}/etcd/lib64"
        )
    endif ()

    set_target_properties(etcd_backend_test PROPERTIES OUTPUT_NAME "etcd_backend_test")

    install(TARGETS etcd_backend_test
            RUNTIME DESTINATION ${TARGET_INSTALL_DIR}/smem/bin
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
endif ()