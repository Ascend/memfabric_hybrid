
set(ENABLE_FUZZ $ENV{ENABLE_FUZZ})
message(STATUS "ENABLE_FUZZ is ${ENABLE_FUZZ}")
SET(ROOT_DIR ${CMAKE_SOURCE_DIR})
set(SECODEFUZZ_INSTALL_DIR "${ROOT_DIR}/test/3rdparty/secodefuzz")
message(STATUS "SECODEFUZZ_INSTALL_DIR is ${SECODEFUZZ_INSTALL_DIR}")
if (NOT EXISTS ${SECODEFUZZ_INSTALL_DIR})
    message(FATAL_ERROR "SECODEFUZZ_INSTALL_DIR(${SECODEFUZZ_INSTALL_DIR}) is invalid, git clone secodefuzz first.")
endif()

include_directories(${SECODEFUZZ_INSTALL_DIR}/include)
link_directories(${SECODEFUZZ_INSTALL_DIR}/lib)

file(GLOB_RECURSE UT_SOURCE_FILES *.cpp)
message(STATUS "fuzz build")
add_executable(fuzz_acc_links ${UT_SOURCE_FILES})

TARGET_GTEST_HEADER(fuzz_acc_links)
TARGET_MOCKCPP_HEADER(fuzz_acc_links)

add_link_options(-lgcov --coverage)
target_compile_options(fuzz_acc_links PUBLIC -D_GNU_SOURCE -g -fprofile-arcs -ftest-coverage)
target_compile_definitions(fuzz_acc_links PRIVATE BUILD_PATH="${OCK_TEST_PROJECT_BINARY}")

set_target_properties(fuzz_acc_links PROPERTIES OUTPUT_NAME "fuzz_acc_links")
set_target_properties(fuzz_acc_links PROPERTIES CLEAN_DIRECT_OUTPUT 1)
target_link_libraries(fuzz_acc_links acc_tcp_net_static gcov libSecodefuzz.a)
target_include_directories(fuzz_acc_links PRIVATE testcase/
        ${PROJECT_SOURCE_DIR}/src/net/csrc
        ${PROJECT_SOURCE_DIR}/src/net/csrc/common
        ${PROJECT_SOURCE_DIR}/src/net/csrc/security
        ${PROJECT_SOURCE_DIR}/src/net/csrc/under_api/openssl
        ${PROJECT_SOURCE_DIR}/src/net/include
        )