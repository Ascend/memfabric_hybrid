file(GLOB_RECURSE DAEMON_SRCS *.cc *.cpp *.h)

add_executable(mmc_meta_service ${DAEMON_SRCS} $<TARGET_OBJECTS:ptracer_object>)

target_compile_options(mmc_meta_service PRIVATE -fPIC -Wno-float-equal)

target_link_libraries(mmc_meta_service PUBLIC mmc_static pthread libacc_tcp_net.a)

install(TARGETS mmc_meta_service
        RUNTIME DESTINATION ${TARGET_INSTALL_DIR}/memcache/bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
