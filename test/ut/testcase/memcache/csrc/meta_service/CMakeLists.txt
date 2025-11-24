file(GLOB_RECURSE meta_service_src
        ${CMAKE_CURRENT_SOURCE_DIR}/test_meta_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_meta_service_net.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_smem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_smem_bm.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/test_global_allocator.cpp
)
ADD_UNIT_TEST(test_global_allocator
        FILES ${global_allocator_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)

file(GLOB_RECURSE service_interface
        ${CMAKE_CURRENT_SOURCE_DIR}/test_service_interface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_smem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_smem_bm.cpp
)
ADD_UNIT_TEST(test_service_interface
        FILES ${service_interface} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)

file(GLOB_RECURSE service_error
        ${CMAKE_CURRENT_SOURCE_DIR}/meta_service_error.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_smem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_smem_bm.cpp
)
ADD_UNIT_TEST(test_service_error
        FILES ${service_error} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)

file(GLOB_RECURSE bm_init_src
        ${CMAKE_CURRENT_SOURCE_DIR}/test_bm_init.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_smem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_smem_bm.cpp
)
ADD_UNIT_TEST(test_bm_init
        FILES ${bm_init_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)

file(GLOB_RECURSE bm_proxy_src
        ${CMAKE_CURRENT_SOURCE_DIR}/test_bm_proxy.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_smem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_smem_bm.cpp
)
ADD_UNIT_TEST(test_bm_proxy
        FILES ${bm_proxy_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)

file(GLOB_RECURSE global_allocator_thread_src
        ${CMAKE_CURRENT_SOURCE_DIR}/test_global_allocator_thread.cpp
)
ADD_UNIT_TEST(test_global_allocator_thread
        FILES ${global_allocator_thread_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)

file(GLOB_RECURSE meta_container_lru_src
        ${CMAKE_CURRENT_SOURCE_DIR}/test_meta_container_lru.cpp
)
ADD_UNIT_TEST(test_meta_container_lru
        FILES ${meta_container_lru_src} ${TEST_SRC_PATHS}/ut_main.cpp
        INCLUDE_PATHS  ${TEST_INCLUDE_PATHS}
        LINK_LIBRARIES ${TEST_DEPEND_LIBS}
        BUILD_FLAGS -fpermissive --coverage
)
