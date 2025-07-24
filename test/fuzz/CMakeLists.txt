
set(ENABLE_FUZZ $ENV{ENABLE_FUZZ})
message(STATUS "ENABLE_FUZZ is ${ENABLE_FUZZ}")
SET(ROOT_DIR ${CMAKE_SOURCE_DIR})
set(SECODEFUZZ_INSTALL_DIR "${ROOT_DIR}/test/3rdparty/secodefuzz")
if (NOT EXISTS ${SECODEFUZZ_INSTALL_DIR})
    message(FATAL_ERROR "SECODEFUZZ_INSTALL_DIR(${SECODEFUZZ_INSTALL_DIR}) is invalid, Please git clone secodefuzz first.")
endif()

include_directories(${SECODEFUZZ_INSTALL_DIR}/include)
link_directories(${SECODEFUZZ_INSTALL_DIR}/lib)

file(GLOB_RECURSE UT_SOURCE_FILES *.cpp
            ../ut/stub/*
            ${PROJECT_SMEM_SRC_BASE}/csrc/common/*
            ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/*
            ${PROJECT_SMEM_SRC_BASE}/csrc/net/*
            ${PROJECT_SMEM_SRC_BASE}/csrc/smem_bm/*
            ${PROJECT_SMEM_SRC_BASE}/csrc/smem_shm/*
            ${PROJECT_SMEM_SRC_BASE}/csrc/smem.cpp
            ${PROJECT_HYBM_SRC_BASE}/csrc/*
            ${PROJECT_HYBM_SRC_BASE}/csrc/common/*
            ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation/*
            ${PROJECT_HYBM_SRC_BASE}/csrc/entity/*
            ${PROJECT_HYBM_SRC_BASE}/csrc/mm/*
            ${PROJECT_HYBM_SRC_BASE}/csrc/transport/*
            ${PROJECT_HYBM_SRC_BASE}/csrc/under_api/*
        )

message(STATUS "fuzz build")
add_executable(test_mf_fuzz ${UT_SOURCE_FILES})

TARGET_GTEST_HEADER(test_mf_fuzz)
TARGET_MOCKCPP_HEADER(test_mf_fuzz)

add_definitions(-DOUT_LIB_PATH="${TARGET_INSTALL_DIR}")

add_link_options(-lgcov --coverage)
target_compile_options(test_mf_fuzz PUBLIC -D_GNU_SOURCE -g -fprofile-arcs -ftest-coverage -DUT_ENABLED)
target_link_directories(test_mf_fuzz PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib/)
set_target_properties(test_mf_fuzz PROPERTIES OUTPUT_NAME "test_mf_fuzz")
set_target_properties(test_mf_fuzz PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(test_mf_fuzz PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_PATH}/bin)
target_link_libraries(test_mf_fuzz dl libacc_tcp_net.a gcov pthread libSecodefuzz.a)
target_include_directories(test_mf_fuzz PRIVATE
        ../ut/stub/
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc
        ${PROJECT_SMEM_SRC_BASE}/csrc/net
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store
        ${PROJECT_OUTPUT_PATH}/3rdparty/acc_links/net/include
        ${PROJECT_3RDPARTY_SRC_DIR}/nlohmann/include/
        ${PROJECT_HYBM_SRC_BASE}/include
        ${PROJECT_HYBM_SRC_BASE}/csrc/common
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation/host
        ${PROJECT_HYBM_SRC_BASE}/csrc/entity
        ${PROJECT_HYBM_SRC_BASE}/csrc/mm
        ${PROJECT_HYBM_SRC_BASE}/csrc/transport
        ${PROJECT_HYBM_SRC_BASE}/csrc/under_api
)
