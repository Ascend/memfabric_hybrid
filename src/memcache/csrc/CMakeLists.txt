include_directories(
        ${PROJECT_MMC_SRC_BASE}/include/
        ${PROJECT_MMC_SRC_BASE}/csrc/common
        ${PROJECT_MMC_SRC_BASE}/csrc/config
        ${PROJECT_MMC_SRC_BASE}/csrc/client
        ${PROJECT_MMC_SRC_BASE}/csrc/local_service
        ${PROJECT_MMC_SRC_BASE}/csrc/entities
        ${PROJECT_MMC_SRC_BASE}/csrc/meta_service
        ${PROJECT_MMC_SRC_BASE}/csrc/net
        ${PROJECT_MMC_SRC_BASE}/csrc/net/acc_links_impl
        ${PROJECT_MMC_SRC_BASE}/csrc/proto
        ${PROJECT_MMC_SRC_BASE}/csrc/under_api/mf_smem
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_UTIL_SRC_BASE}
        ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/include/
        ${PROJECT_SOURCE_DIR}/3rdparty/net/acc_links/src/net/include
)

## define src variables
set(MMC_SRC_COMMON "")
set(MMC_SRC_CLIENT "")
set(MMC_SRC_LOCAL_SERVICE "")
set(MMC_SRC_ENTITIES "")
set(MMC_SRC_META_SERVICE "")
set(MMC_SRC_NET "")
set(MMC_SRC_PROTO "")
set(MMC_SRC_UNDER_API_MF_SMEM "")

## add files into src variables
aux_source_directory(common MMC_SRC_COMMON)
aux_source_directory(config MMC_SRC_CONFIG)
aux_source_directory(client MMC_SRC_CLIENT)
aux_source_directory(local_service MMC_SRC_LOCAL_SERVICE)
aux_source_directory(entities MMC_SRC_ENTITIES)
aux_source_directory(meta_service MMC_SRC_META_SERVICE)
aux_source_directory(net MMC_SRC_NET)
aux_source_directory(net/acc_links_impl MMC_SRC_NET_ACC)
aux_source_directory(proto MMC_SRC_PROTO)
aux_source_directory(under_api/mf_smem MMC_SRC_UNDER_API_MF_SMEM)

# set whole source for platform
set(PLATFORM_SRC ${MMC_SRC_COMMON}
        ${MMC_SRC_CONFIG}
        ${MMC_SRC_CLIENT}
        ${MMC_SRC_LOCAL_SERVICE}
        ${MMC_SRC_ENTITIES}
        ${MMC_SRC_META_SERVICE}
        ${MMC_SRC_NET}
        ${MMC_SRC_NET_ACC}
        ${MMC_SRC_PROTO}
        ${MMC_SRC_UNDER_API_MF_SMEM}
        mmc.cpp
        mmc_client.cpp
        mmc_service.cpp)

## build the static and shared library
add_library(mmc_objects OBJECT ${PLATFORM_SRC})

target_link_libraries(mmc_objects PUBLIC smem)
target_compile_options(mmc_objects PRIVATE -fPIC)

add_library(mmc_static STATIC $<TARGET_OBJECTS:mmc_objects>)
set_target_properties(mmc_static PROPERTIES OUTPUT_NAME "mf_memcache")
target_link_directories(mmc_static PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib)
target_link_libraries(mmc_static PUBLIC pthread libacc_tcp_net.a)

add_library(mmc_shared SHARED $<TARGET_OBJECTS:mmc_objects>)
set_target_properties(mmc_shared PROPERTIES OUTPUT_NAME "mf_memcache")
target_link_directories(mmc_shared PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib)
target_link_libraries(mmc_shared PUBLIC smem_shared pthread libacc_tcp_net.a)

if(BUILD_PYTHON STREQUAL "ON")
    add_subdirectory(python_wrapper)
endif()
add_subdirectory(daemon)

#install
file(GLOB HEADER_FILES ../include/*.h)

#install head files
install(FILES ${HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/memcache/include/memcache)

# install library
install(TARGETS mmc_shared
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/memcache/lib64
        PERMISSIONS OWNER_READ OWNER_EXECUTE
)