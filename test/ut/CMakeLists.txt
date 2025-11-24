# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty "${CMAKE_CURRENT_BINARY_DIR}/3rdparty_build")
file(GLOB_RECURSE UT_SOURCE_FILES *.cpp
        testcase/*
        ../stub/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/common/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/net/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_bm/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_shm/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem.cpp
)

include_directories(
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc
        ${PROJECT_SMEM_SRC_BASE}/csrc/net
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store
        ${PROJECT_SMEM_SRC_BASE}/csrc/under_api/hybm_core
        ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/include/
)
message(STATUS "llt build")
# add_executable(test_mf_hy ${UT_SOURCE_FILES})

# set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/test/3rdparty)
# set(GTEST_INSTALL_DIR ${PROJECT_3RDPARTY_BIN_DIR}/googletest)

# set(CHECK_FILE_LINKS ${GTEST_INSTALL_DIR}/include/gtest/gtest.h)

# if(NOT EXISTS ${CHECK_FILE_LINKS})
#     add_dependencies(test_mf_hy googletest)
# endif()

# add_definitions(-DOUT_LIB_PATH="${TARGET_INSTALL_DIR}")

# add_link_options(-lgcov --coverage)
# TARGET_SPDLOG_HEADER_ONLY(test_mf_hy)
# target_compile_options(test_mf_hy PUBLIC -D_GNU_SOURCE -g -fprofile-arcs -ftest-coverage -DUT_ENABLED)
# target_link_directories(test_mf_hy PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib/)
# target_link_directories(test_mf_hy PUBLIC ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/lib64)
# set_target_properties(test_mf_hy PROPERTIES OUTPUT_NAME "test_mf_hy")
# set_target_properties(test_mf_hy PROPERTIES CLEAN_DIRECT_OUTPUT 1)
# set_target_properties(test_mf_hy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_PATH}/bin)
# target_link_libraries(test_mf_hy gtest dl libacc_tcp_net.a gcov pthread hybmm_static)
# target_include_directories(test_mf_hy PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}/testcase
#         ${CMAKE_CURRENT_SOURCE_DIR}/shmem_wrapper
#         ${PROJECT_OUTPUT_PATH}/3rdparty/acc_links/net/include
#         ${PROJECT_UTIL_SRC_BASE}
# )

FUNCTION(ADD_UNIT_TEST TEST_NAME)
    set(ONE_VALUE_ARGS PREFIX)
    set(MULTI_VALUE_ARGS FILES BUILD_FLAGS DEPENDS INCLUDE_PATHS LINK_LIBRARIES THIRDPARTY_DEPENDS)
    cmake_parse_arguments(ADD_UNIT_TEST "" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})
    set(TEST_TARGET_NAME ${TEST_NAME})
    add_executable(${TEST_TARGET_NAME} ${ADD_UNIT_TEST_FILES} $<TARGET_OBJECTS:ptracer_object> $<TARGET_OBJECTS:config_store_object>)
    target_link_directories(${TEST_TARGET_NAME}  PUBLIC ${PROJECT_OUTPUT_PATH}/acc_links/lib64/)
    target_link_directories(${TEST_TARGET_NAME}  PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/spdlog/lib/)
    target_link_directories(${TEST_TARGET_NAME}  PUBLIC ${GTEST_BUILD_LIB_DIR}/lib)
    target_link_directories(${TEST_TARGET_NAME}  PUBLIC ${PROJECT_OUTPUT_PATH}/smem/lib64)
    target_link_directories(${TEST_TARGET_NAME}  PUBLIC ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/lib64)
    TARGET_MOCKCPP_HEADER(${TEST_TARGET_NAME})
    if(NOT EXISTS ${CHECK_FILE_LINKS})
        add_dependencies(${TEST_TARGET_NAME} googletest)
    endif()
    install(TARGETS ${TEST_TARGET_NAME}
            DESTINATION ${PROJECT_OUTPUT_PATH}/bin/ut/
            PERMISSIONS ${CACHE_INSTALL_EXE_PERMISSIONS})
    IF(ADD_UNIT_TEST_INCLUDE_PATHS)
        target_include_directories(${TEST_TARGET_NAME} PUBLIC ${ADD_UNIT_TEST_INCLUDE_PATHS})
    ENDIF()

    IF(ADD_UNIT_TEST_LINK_LIBRARIES)
        target_link_libraries(${TEST_TARGET_NAME} ${ADD_UNIT_TEST_LINK_LIBRARIES})
    ENDIF()

    IF(ADD_UNIT_TEST_DEPENDS)
        add_dependencies(${TEST_TARGET_NAME} ${ADD_UNIT_TEST_DEPENDS})
    ENDIF()

    IF(ADD_UNIT_TEST_BUILD_FLAGS)
        target_compile_options(${TEST_TARGET_NAME} PUBLIC ${ADD_UNIT_TEST_BUILD_FLAGS})
    ENDIF()

    add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_TARGET_NAME}>)

ENDFUNCTION()

set(TEST_SRC_PATHS ${CMAKE_CURRENT_SOURCE_DIR})
set(TEST_DEPEND_LIBS gtest dl acc_tcp_net_static smem_static hybmm_static gcov pthread)

#add_subdirectory(testcase/shmem_wrapper)
add_subdirectory(testcase/smem/common)