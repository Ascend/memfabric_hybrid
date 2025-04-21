file(GLOB_RECURSE UT_SOURCE_FILES *.cpp
        testcase/*
        ../stub/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/*
)
message(STATUS "llt build")
add_executable(test_mf_hy ${UT_SOURCE_FILES})

set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/test/3rdparty)
set(GTEST_INSTALL_DIR ${PROJECT_3RDPARTY_BIN_DIR}/googletest)

set(CHECK_FILE_LINKS ${GTEST_INSTALL_DIR}/include/gtest/gtest.h)

if(NOT EXISTS ${CHECK_FILE_LINKS})
    add_dependencies(test_mf_hy googletest)
endif()

add_definitions(-DOUT_LIB_PATH="${TARGET_INSTALL_DIR}")

add_link_options(-lgcov --coverage)
target_compile_options(test_mf_hy PUBLIC -D_GNU_SOURCE -g -fprofile-arcs -ftest-coverage -DUT_ENABLED)
target_link_directories(test_mf_hy PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib/)
set_target_properties(test_mf_hy PROPERTIES OUTPUT_NAME "test_mf_hy")
set_target_properties(test_mf_hy PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(test_mf_hy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_PATH}/bin)
target_link_libraries(test_mf_hy gtest dl libacc_tcp_net.a gcov)
target_include_directories(test_mf_hy PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/testcase
        ${CMAKE_CURRENT_SOURCE_DIR}/shmem_wrapper
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc
        ${PROJECT_SMEM_SRC_BASE}/csrc/net
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store
        ${PROJECT_OUTPUT_PATH}/3rdparty/acc_links/net/include
        ${PROJECT_SMEM_SRC_BASE}/csrc/under_api/hybm_core
)
