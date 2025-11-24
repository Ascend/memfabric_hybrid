file(GLOB_RECURSE common_src
        ${CMAKE_CURRENT_SOURCE_DIR}/test_locks.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_thread_pool.cpp
)
ADD_UNIT_TEST(test_common
        FILES ${common_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)
