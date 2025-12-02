file(GLOB_RECURSE utils_src
        ${CMAKE_CURRENT_SOURCE_DIR}/mf_file_util_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mf_num_util_test.cpp
)
ADD_UNIT_TEST(test_utils
        FILES ${utils_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)