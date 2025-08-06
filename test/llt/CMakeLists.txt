
file(GLOB_RECURSE UT_SOURCE_FILES *.cpp testcase/*)
message(STATUS "llt build")
add_executable(test_acc_links ${UT_SOURCE_FILES})

TARGET_GTEST_HEADER(test_acc_links)
TARGET_MOCKCPP_HEADER(test_acc_links)

add_link_options(-lgcov --coverage)
target_compile_options(test_acc_links PUBLIC -D_GNU_SOURCE -g -fprofile-arcs -ftest-coverage)
target_compile_definitions(test_acc_links PRIVATE BUILD_PATH="${OCK_TEST_PROJECT_BINARY}")

set_target_properties(test_acc_links PROPERTIES OUTPUT_NAME "test_acc_links")
set_target_properties(test_acc_links PROPERTIES CLEAN_DIRECT_OUTPUT 1)
target_link_libraries(test_acc_links acc_tcp_net_static gcov)
target_include_directories(test_acc_links PRIVATE testcase/
                                                ${SRC_DIR}/src/net/csrc
                                                ${SRC_DIR}/src/net/csrc/common
                                                ${SRC_DIR}/src/net/csrc/security
                                                ${SRC_DIR}/src/net/csrc/under_api/openssl
                                                ${SRC_DIR}/src/net/include
                                                )