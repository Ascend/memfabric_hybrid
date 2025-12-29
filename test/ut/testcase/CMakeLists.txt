file(GLOB_RECURSE memfabric_src
        ${CMAKE_CURRENT_SOURCE_DIR}/hybm/*
        ${CMAKE_CURRENT_SOURCE_DIR}/smem/*
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/*
        ${CMAKE_CURRENT_SOURCE_DIR}/acc_links/*
)
ADD_UNIT_TEST(test_memfabric
        FILES ${memfabric_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)