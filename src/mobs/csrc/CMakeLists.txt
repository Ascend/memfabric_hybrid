include_directories(
        ${PROJECT_MOBS_SRC_BASE}/include/
        ${PROJECT_MOBS_SRC_BASE}/csrc/common
        ${PROJECT_MOBS_SRC_BASE}/csrc/client
        ${PROJECT_MOBS_SRC_BASE}/csrc/local_service
        ${PROJECT_MOBS_SRC_BASE}/csrc/meta_service
        ${PROJECT_MOBS_SRC_BASE}/csrc/net
        ${PROJECT_MOBS_SRC_BASE}/csrc/proto
        ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/include/
)

## define src variables
set(MOBS_SRC_COMMON "")
set(MOBS_SRC_CLIENT "")
set(MOBS_SRC_LOCAL_SERVICE "")
set(MOBS_SRC_META_SERVICE "")
set(MOBS_SRC_NET "")
set(MOBS_SRC_PROTO "")

## add files into src variables
aux_source_directory(common MOBS_SRC_COMMON)
aux_source_directory(client MOBS_SRC_CLIENT)
aux_source_directory(local_service MOBS_SRC_LOCAL_SERVICE)
aux_source_directory(meta_service MOBS_SRC_META_SERVICE)
aux_source_directory(net MOBS_SRC_NET)
aux_source_directory(proto MOBS_SRC_PROTO)

# set whole source for platform
set(PLATFORM_SRC ${MOBS_SRC_COMMON}
        ${MOBS_SRC_LOCAL_SERVICE}
        ${MOBS_SRC_LOCAL_SERVICE}
        ${MOBS_SRC_META_SERVICE}
        ${MOBS_SRC_NET}
        ${MOBS_SRC_PROTO}
        mobs.cpp
        mobs_client.cpp
        mobs_service.cpp)

## build the static and shared library
add_library(mobs_objects OBJECT ${PLATFORM_SRC})

target_link_libraries(mobs_objects PUBLIC smem)
target_compile_options(mobs_objects PRIVATE -fPIC)

add_library(mobs_static STATIC $<TARGET_OBJECTS:mobs_objects>)
set_target_properties(mobs_static PROPERTIES OUTPUT_NAME "mf_mobs")
target_link_directories(mobs_static PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib)
target_link_libraries(mobs_static PUBLIC pthread libacc_tcp_net.a)

add_library(mobs_shared SHARED $<TARGET_OBJECTS:mobs_objects>)
set_target_properties(mobs_shared PROPERTIES OUTPUT_NAME "mf_mobs")
target_link_directories(mobs_shared PUBLIC ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/lib)
target_link_libraries(mobs_shared PUBLIC pthread libacc_tcp_net.a)

add_subdirectory(python_wrapper)

#install
file(GLOB HEADER_FILES ../include/*.h)

#install head files
install(FILES ${HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/mobs/include/mobs)

# install library
install(TARGETS mobs_shared
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/mobs/lib64
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
)