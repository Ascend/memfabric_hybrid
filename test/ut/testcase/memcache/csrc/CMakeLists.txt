file(GLOB_RECURSE mmc_test_src
        # ${CMAKE_CURRENT_SOURCE_DIR}/meta_service/meta_service_net.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/meta_service/service_interface.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/meta_service/bm_init.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/meta_service/bm_proxy.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/meta_service/test_meta_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/meta_service/global_allocator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/meta_service/global_allocator_thread.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/test_lookup_map.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/common/test_locks.cpp
)
ADD_UNIT_TEST(test_mmc_test
        FILES ${mmc_test_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)
