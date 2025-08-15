file(GLOB_RECURSE FUZZ_CUR_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(test_mmc_fuzz ${FUZZ_CUR_SOURCE_FILES} ${FUZZ_GLOBAL_SOURCE_FILES})

TARGET_MOCKCPP_HEADER(test_mmc_fuzz)

set_target_properties(test_mmc_fuzz PROPERTIES OUTPUT_NAME "test_mmc_fuzz")
set_target_properties(test_mmc_fuzz PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(test_mmc_fuzz PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_PATH}/bin)
add_link_options(-lgcov --coverage)
target_compile_options(test_mmc_fuzz PUBLIC -D_GNU_SOURCE -g -fprofile-arcs -ftest-coverage)
target_link_directories(test_mmc_fuzz PRIVATE
        ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib
        ${PROJECT_3RDPARTY_BIN_DIR}/spdlog/lib
        ${PROJECT_OUTPUT_PATH}/memcache/lib64
        ${PROJECT_OUTPUT_PATH}/smem/lib64
        ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/lib64
        ${GTEST_BUILD_LIB_DIR}/lib
)
target_link_libraries(test_mmc_fuzz dl rt gtest gcov pthread mmc_static smem_shared libacc_tcp_net.a mockcpp Secodefuzz)
target_include_directories(test_mmc_fuzz PRIVATE
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc
        ${PROJECT_SMEM_SRC_BASE}/csrc/net
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store
        ${PROJECT_SMEM_SRC_BASE}/csrc/under_api/hybm_core
        ${PROJECT_MMC_SRC_BASE}/include
        ${PROJECT_MMC_SRC_BASE}/csrc/common
        ${PROJECT_MMC_SRC_BASE}/csrc/client
        ${PROJECT_MMC_SRC_BASE}/csrc/config
        ${PROJECT_MMC_SRC_BASE}/csrc/net
        ${PROJECT_MMC_SRC_BASE}/csrc/log
        ${PROJECT_MMC_SRC_BASE}/csrc/entities
        ${PROJECT_MMC_SRC_BASE}/csrc/net/acc_links_impl
        ${PROJECT_MMC_SRC_BASE}/csrc/meta_service
        ${PROJECT_MMC_SRC_BASE}/csrc/entities
        ${PROJECT_MMC_SRC_BASE}/csrc/local_service
        ${PROJECT_MMC_SRC_BASE}/csrc/proto
        ${PROJECT_MMC_SRC_BASE}/csrc/under_api/mf_smem
        ${PROJECT_MMC_SRC_BASE}/../util/csrc
        ${FUZZ_ROOT_DIR}
)