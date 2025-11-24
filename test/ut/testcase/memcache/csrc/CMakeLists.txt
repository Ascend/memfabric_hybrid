file(GLOB_RECURSE mmc_test_src
        ${CMAKE_CURRENT_SOURCE_DIR}/meta_service/*
        ${CMAKE_CURRENT_SOURCE_DIR}/local_service/*
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/*
        ${CMAKE_CURRENT_SOURCE_DIR}/common/*
        ${CMAKE_CURRENT_SOURCE_DIR}/config/*
        ${CMAKE_CURRENT_SOURCE_DIR}/log/*
#        ${CMAKE_CURRENT_SOURCE_DIR}/net/*
)
ADD_UNIT_TEST(test_mmc_test
        FILES ${mmc_test_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)