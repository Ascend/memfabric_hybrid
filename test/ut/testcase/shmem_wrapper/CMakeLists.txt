file(GLOB_RECURSE common_src
        ${CMAKE_CURRENT_SOURCE_DIR}/shem_wrapper_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/smem_shm_api.cpp
)
ADD_UNIT_TEST(test_common
        FILES ${common_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)