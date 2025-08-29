aux_source_directory(. DAEMON_SRCS)
aux_source_directory(ha_api DAEMON_SRCS)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ha_api)

add_subdirectory(ha)

add_executable(mmc_meta_service ${DAEMON_SRCS} $<TARGET_OBJECTS:ptracer_object>)

target_compile_options(mmc_meta_service PRIVATE -fPIC -Wno-float-equal)

target_link_libraries(mmc_meta_service PUBLIC mmc_static pthread libacc_tcp_net.a)

install(TARGETS mmc_meta_service
        RUNTIME DESTINATION ${TARGET_INSTALL_DIR}/memcache/bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
