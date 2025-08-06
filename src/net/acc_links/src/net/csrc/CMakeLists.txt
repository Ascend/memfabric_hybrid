include_directories(
        ${SRC_DIR}/src/net/include
        ${SRC_DIR}/src/net/csrc/
        ${SRC_DIR}/src/net/csrc/common
        ${SRC_DIR}/src/net/csrc/security
        ${SRC_DIR}/src/net/csrc/under_api/openssl
)

file(GLOB_RECURSE ACC_TCP_NET_SRCS *.cpp *.h)
add_library(acc_tcp_net_object OBJECT ${ACC_TCP_NET_SRCS})
add_library(acc_tcp_net_static STATIC $<TARGET_OBJECTS:acc_tcp_net_object>)
set_target_properties(acc_tcp_net_static PROPERTIES OUTPUT_NAME "acc_tcp_net")
set_target_properties(acc_tcp_net_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
target_link_libraries(acc_tcp_net_static
        -Wl,--start-group
        #pthread dl rt ssl crypto
        pthread dl rt
        -Wl,--end-group
        )

add_library(acc_tcp_net_shared SHARED $<TARGET_OBJECTS:acc_tcp_net_object>)
set_target_properties(acc_tcp_net_shared PROPERTIES OUTPUT_NAME "acc_tcp_net")
set_target_properties(acc_tcp_net_shared PROPERTIES CLEAN_DIRECT_OUTPUT 1)
target_link_libraries(acc_tcp_net_shared
        -Wl,--start-group
        #pthread dl rt ssl crypto
        pthread dl rt
        -Wl,--end-group
        )

file(GLOB HEADER_FILES ../include/*.h)
install(FILES ${HEADER_FILES} DESTINATION ${TARGET_INSTALL_DIR}/include/acc_links/net)
install(TARGETS acc_tcp_net_shared acc_tcp_net_static
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/lib
        ARCHIVE DESTINATION ${TARGET_INSTALL_DIR}/lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        )