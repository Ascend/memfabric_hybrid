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

File(GLOB_RECURSE NET_SERVER1_SRCS server1.cpp)
add_executable(net_server1 ${NET_SERVER1_SRCS})
target_link_libraries(net_server1 ${DEPEND_LIBS})

File(GLOB_RECURSE NET_SERVER2_SRCS server2.cpp)
add_executable(net_server2 ${NET_SERVER2_SRCS})
target_link_libraries(net_server2 ${DEPEND_LIBS})