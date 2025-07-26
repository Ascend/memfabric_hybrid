file(GLOB_RECURSE meta_service_src
        ${CMAKE_CURRENT_SOURCE_DIR}/test_meta_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/meta_service_net.cpp
)
ADD_UNIT_TEST(test_meta_service
        FILES ${meta_service_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)

file(GLOB_RECURSE meta_manager_src
        ${CMAKE_CURRENT_SOURCE_DIR}/test_meta_manager.cpp
)
ADD_UNIT_TEST(test_meta_manager
        FILES ${meta_manager_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)

file(GLOB_RECURSE global_allocator_src
        ${CMAKE_CURRENT_SOURCE_DIR}/global_allocator.cpp
)
ADD_UNIT_TEST(test_global_allocator
        FILES ${global_allocator_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)

file(GLOB_RECURSE service_interface
        ${CMAKE_CURRENT_SOURCE_DIR}/service_interface.cpp
)
ADD_UNIT_TEST(test_service_interface
        FILES ${service_interface} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)

file(GLOB_RECURSE bm_init_src
        ${CMAKE_CURRENT_SOURCE_DIR}/bm_init.cpp
)
ADD_UNIT_TEST(test_bm_init
        FILES ${bm_init_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)

file(GLOB_RECURSE bm_proxy_src
        ${CMAKE_CURRENT_SOURCE_DIR}/bm_proxy.cpp
)
ADD_UNIT_TEST(test_bm_proxy
        FILES ${bm_proxy_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)

file(GLOB_RECURSE global_allocator_thread_src
        ${CMAKE_CURRENT_SOURCE_DIR}/global_allocator_thread.cpp
)
ADD_UNIT_TEST(test_global_allocator_thread
        FILES ${global_allocator_thread_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)