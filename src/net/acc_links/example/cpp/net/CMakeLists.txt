if(ENABLE_ASAN)
    set(DEPEND_LIBS
        -Wl,--start-group
        pthread acc_tcp_net_static asan
        -Wl,--end-group)
else()
        set(DEPEND_LIBS
        -Wl,--start-group
        pthread acc_tcp_net_static
        -Wl,--end-group)
endif()

File(GLOB_RECURSE NET_CLIENT_SRCS client.cpp)
add_executable(net_client ${NET_CLIENT_SRCS})
target_link_libraries(net_client ${DEPEND_LIBS})

File(GLOB_RECURSE NET_SERVER_SRCS server.cpp)
add_executable(net_server ${NET_SERVER_SRCS})
target_link_libraries(net_server ${DEPEND_LIBS})