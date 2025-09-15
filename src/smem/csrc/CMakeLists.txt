include_directories(
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store
        ${PROJECT_SMEM_SRC_BASE}/csrc/net
        ${PROJECT_SMEM_SRC_BASE}/csrc/smem_shm
        ${PROJECT_HYBM_SRC_BASE}/include
        ${PROJECT_UTIL_SRC_BASE}/ptracer/include
        ${PROJECT_UTIL_SRC_BASE}
        ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/include/
        ${PROJECT_SOURCE_DIR}/3rdparty/net/acc_links/src/net/include
)

## define src variables
set(SMEM_SRC_COMMON "")
set(SMEM_SRC_NET "")
set(SMEM_SRC_SMEM_SHM "")
set(SMEM_SRC_CORE_API "")
set(SMEM_SRC_SMEM_BM "")

## add files into src variables
aux_source_directory(common SMEM_SRC_COMMON)
aux_source_directory(net/ SMEM_SRC_NET)
aux_source_directory(smem_shm SMEM_SRC_SMEM_SHARE)
aux_source_directory(smem_bm SMEM_SRC_SMEM_BM)

# set whole source for platform
set(PLATFORM_SRC ${SMEM_SRC_COMMON}
        ${SMEM_SRC_NET}
        ${SMEM_SRC_SMEM_SHARE}
        ${SMEM_SRC_SMEM_BM}
        smem.cpp)

## build the static and shared library
add_library(smem_objects OBJECT ${PLATFORM_SRC})

target_link_libraries(smem_objects PUBLIC smem)
target_compile_options(smem_objects PRIVATE -fPIC)

add_library(smem_static STATIC $<TARGET_OBJECTS:smem_objects> $<TARGET_OBJECTS:config_store_object>)
set_target_properties(smem_static PROPERTIES OUTPUT_NAME "mf_smem")
target_link_directories(smem_static PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib)
target_link_libraries(smem_static PUBLIC pthread libacc_tcp_net.a hybmm_static)

add_library(smem_shared SHARED $<TARGET_OBJECTS:smem_objects> $<TARGET_OBJECTS:config_store_object>)
set_target_properties(smem_shared PROPERTIES OUTPUT_NAME "mf_smem")
target_link_directories(smem_shared PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib)
target_link_libraries(smem_shared PUBLIC pthread libacc_tcp_net.a hybmm_shared)

if(BUILD_PYTHON STREQUAL "ON")
    add_subdirectory(python_wrapper)
endif()

#install
file(GLOB HEADER_FILES ../include/host/*.h)
file(GLOB DEVICE_HEADER_FILES ../include/device/*.h)

#install head files
install(FILES ${HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/smem/include/host)
install(FILES ${DEVICE_HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/smem/include/device/)

# install library
install(TARGETS smem_shared
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/smem/lib64
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        )