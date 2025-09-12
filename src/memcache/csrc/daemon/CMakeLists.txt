aux_source_directory(. DAEMON_SRCS)

add_executable(mmc_meta_service ${DAEMON_SRCS} $<TARGET_OBJECTS:ptracer_object> $<TARGET_OBJECTS:config_store_object>)

target_compile_options(mmc_meta_service PRIVATE -fPIC -Wno-float-equal)

target_link_libraries(mmc_meta_service PUBLIC mmc_shared pthread libacc_tcp_net.a)

if(BUILD_PYTHON STREQUAL "ON")
    target_link_libraries(mmc_meta_service PRIVATE pybind11::module Python3::Python)
endif()

install(TARGETS mmc_meta_service
        RUNTIME DESTINATION ${TARGET_INSTALL_DIR}/memcache/bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
