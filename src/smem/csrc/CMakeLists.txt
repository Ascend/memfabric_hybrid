include_directories(
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store
        ${PROJECT_SMEM_SRC_BASE}/csrc/net
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_shm
        ${PROJECT_SMEM_SRC_BASE}/csrc/under_api/hybm_core
        ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/include/
)

## define src variables
set(SMEM_SRC_COMMON "")
set(SMEM_SRC_NET "")
set(SMEM_SRC_SMEM_SHM "")
set(SMEM_SRC_STORE "")
set(SMEM_SRC_CORE_API "")
set(SMEM_SRC_SMEM_BM "")

## add files into src variables
aux_source_directory(common SMEM_SRC_COMMON)
aux_source_directory(config_store SMEM_SRC_STORE)
aux_source_directory(net/ SMEM_SRC_NET)
aux_source_directory(smem_shm SMEM_SRC_SMEM_SHARE)
aux_source_directory(under_api/hybm_core/ SMEM_SRC_CORE_API)
aux_source_directory(smem_bm SMEM_SRC_SMEM_BM)

# set whole source for platform
set(PLATFORM_SRC ${SMEM_SRC_COMMON}
        ${SMEM_SRC_NET}
        ${SMEM_SRC_SMEM_SHARE}
        ${SMEM_SRC_STORE}
        ${SMEM_SRC_CORE_API}
        ${SMEM_SRC_SMEM_BM}
        smem.cpp)

## build the static and shared library
add_library(smem_objects OBJECT ${PLATFORM_SRC})

target_link_libraries(smem_objects PUBLIC smem)
target_compile_options(smem_objects PRIVATE -fPIC)

add_library(smem_static STATIC $<TARGET_OBJECTS:smem_objects>)
set_target_properties(smem_static PROPERTIES OUTPUT_NAME "mf_smem")
target_link_directories(smem_static PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib)
target_link_libraries(smem_static PUBLIC pthread libacc_tcp_net.a)

add_library(smem_shared SHARED $<TARGET_OBJECTS:smem_objects>)
set_target_properties(smem_shared PROPERTIES OUTPUT_NAME "mf_smem")
target_link_directories(smem_shared PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib)
target_link_libraries(smem_shared PUBLIC pthread libacc_tcp_net.a)

#install
file(GLOB HEADER_FILES ../include/host/*.h)
file(GLOB DEVICE_HEADER_FILES ../include/device/low_level/*.h)
file(GLOB DEVICE_HEADER_FILES_HIGH ../include/device/high_level/*.h)

#install head files
install(FILES ${HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/smem/include/smem/host)
install(FILES ${DEVICE_HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/smem/include/smem/device/lowlevel)

if(INSTALL_HIGH_LEVEL_API STREQUAL "ON")
    install(FILES ${DEVICE_HEADER_FILES_HIGH} DESTINATION ${TARGET_INSTALL_DIR}/smem/include/smem/device/highlevel)
endif()

# install library
install(TARGETS smem_shared
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/smem/lib64
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        )