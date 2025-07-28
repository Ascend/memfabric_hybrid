file(GLOB_RECURSE UT_SOURCE_FILES *.cpp
        testcase/*
        stub/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/common/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/net/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_bm/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_shm/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_trans/*
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem.cpp
        ${PROJECT_HYBM_SRC_BASE}/csrc/hybm_entry.cpp
        ${PROJECT_HYBM_SRC_BASE}/csrc/hybm_data_op_entry.cpp
        ${PROJECT_HYBM_SRC_BASE}/csrc/hybm_big_mem_entry.cpp
        ${PROJECT_HYBM_SRC_BASE}/csrc/common/*
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation/*
        ${PROJECT_HYBM_SRC_BASE}/csrc/entity/*
        ${PROJECT_HYBM_SRC_BASE}/csrc/mm/*
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport/*
        ${PROJECT_HYBM_SRC_BASE}/csrc/under_api/*
)
message(STATUS "llt build")
add_executable(test_mf_hy ${UT_SOURCE_FILES})

TARGET_GTEST_HEADER(test_mf_hy)
TARGET_MOCKCPP_HEADER(test_mf_hy)

add_definitions(-DOUT_LIB_PATH="${TARGET_INSTALL_DIR}")

add_link_options(-lgcov --coverage)
target_compile_options(test_mf_hy PUBLIC -D_GNU_SOURCE -g -fprofile-arcs -ftest-coverage -DUT_ENABLED)
target_link_directories(test_mf_hy PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib/)
set_target_properties(test_mf_hy PROPERTIES OUTPUT_NAME "test_mf_hy")
set_target_properties(test_mf_hy PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(test_mf_hy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_PATH}/bin)
target_link_libraries(test_mf_hy dl rt libacc_tcp_net.a gcov pthread)
target_include_directories(test_mf_hy PRIVATE
        stub/
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/testcase
        ${CMAKE_CURRENT_SOURCE_DIR}/shmem_wrapper
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc
        ${PROJECT_SMEM_SRC_BASE}/csrc/net
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_bm
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_shm
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_trans
        ${PROJECT_OUTPUT_PATH}/3rdparty/acc_links/net/include
        ${PROJECT_3RDPARTY_SRC_DIR}/nlohmann/include/
        ${PROJECT_HYBM_SRC_BASE}/include
        ${PROJECT_HYBM_SRC_BASE}/csrc/common
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation/host
        ${PROJECT_HYBM_SRC_BASE}/csrc/entity
        ${PROJECT_HYBM_SRC_BASE}/csrc/mm
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver/
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver/userspace
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport
        ${PROJECT_HYBM_SRC_BASE}/csrc/under_api
)
