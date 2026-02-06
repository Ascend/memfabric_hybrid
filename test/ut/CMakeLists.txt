file(GLOB_RECURSE UT_SOURCE_FILES *.cpp
        testcase/*
        ../stub/*
)

include_directories(
        ${PROJECT_HYBM_SRC_BASE}/include
        ${PROJECT_HYBM_SRC_BASE}/csrc/common
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation/host
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver/userspace
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver/version
        ${PROJECT_HYBM_SRC_BASE}/csrc/entity
        ${PROJECT_HYBM_SRC_BASE}/csrc/mm
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/compose
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/device
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/host
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/utils
        ${PROJECT_HYBM_SRC_BASE}/csrc/ts_engine
        ${PROJECT_HYBM_SRC_BASE}/csrc/under_api
        ${PROJECT_UTIL_SRC_BASE}/ptracer/include

        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc
        ${PROJECT_SMEM_SRC_BASE}/csrc/net
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/backend
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/ha
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/tcp_store

        ${PROJECT_ACCLINKS_SRC_BASE}/include
        ${PROJECT_ACCLINKS_SRC_BASE}/csrc
        ${PROJECT_ACCLINKS_SRC_BASE}/csrc/common
        ${PROJECT_ACCLINKS_SRC_BASE}/csrc/security
        ${PROJECT_ACCLINKS_SRC_BASE}/csrc/under_api/openssl

        ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/include/
)

FUNCTION(ADD_UNIT_TEST TEST_NAME)
    set(ONE_VALUE_ARGS PREFIX)
    set(MULTI_VALUE_ARGS FILES BUILD_FLAGS DEPENDS INCLUDE_PATHS LINK_LIBRARIES THIRDPARTY_DEPENDS)
    cmake_parse_arguments(ADD_UNIT_TEST "" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})
    set(TEST_TARGET_NAME ${TEST_NAME})
    add_executable(
            ${TEST_TARGET_NAME}
            ${ADD_UNIT_TEST_FILES}
            $<TARGET_OBJECTS:ptracer_object>
            $<TARGET_OBJECTS:config_store_object>
            $<TARGET_OBJECTS:smem_objects>
            $<TARGET_OBJECTS:hybmm_objects>
            $<TARGET_OBJECTS:acc_tcp_net_object>
    )
    target_compile_options(${TEST_TARGET_NAME} PRIVATE -w)
    target_link_directories(${TEST_TARGET_NAME} PUBLIC ${PROJECT_OUTPUT_PATH}/acc_links/lib64/)
    target_link_directories(${TEST_TARGET_NAME} PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/spdlog/lib/)
    target_link_directories(${TEST_TARGET_NAME} PUBLIC ${GTEST_BUILD_LIB_DIR}/lib)
    target_link_directories(${TEST_TARGET_NAME} PUBLIC ${PROJECT_OUTPUT_PATH}/smem/lib64)
    target_link_directories(${TEST_TARGET_NAME} PUBLIC ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/lib64)
    TARGET_MOCKCPP_HEADER(${TEST_TARGET_NAME})
    if (NOT EXISTS ${CHECK_FILE_LINKS})
        add_dependencies(${TEST_TARGET_NAME} googletest)
    endif ()
    install(TARGETS ${TEST_TARGET_NAME}
            DESTINATION ${PROJECT_OUTPUT_PATH}/bin/ut/
            PERMISSIONS ${CACHE_INSTALL_EXE_PERMISSIONS})
    IF (ADD_UNIT_TEST_INCLUDE_PATHS)
        target_include_directories(${TEST_TARGET_NAME} PUBLIC ${ADD_UNIT_TEST_INCLUDE_PATHS})
    ENDIF ()

    IF (ADD_UNIT_TEST_LINK_LIBRARIES)
        target_link_libraries(${TEST_TARGET_NAME} ${ADD_UNIT_TEST_LINK_LIBRARIES})
    ENDIF ()

    IF (ADD_UNIT_TEST_DEPENDS)
        add_dependencies(${TEST_TARGET_NAME} ${ADD_UNIT_TEST_DEPENDS})
    ENDIF ()

    IF (ADD_UNIT_TEST_BUILD_FLAGS)
        target_compile_options(${TEST_TARGET_NAME} PUBLIC ${ADD_UNIT_TEST_BUILD_FLAGS})
    ENDIF ()

    add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_TARGET_NAME}>)

ENDFUNCTION()

set(TEST_SRC_PATHS ${CMAKE_CURRENT_SOURCE_DIR})
set(TEST_DEPEND_LIBS gtest dl gcov pthread)

add_subdirectory(testcase)