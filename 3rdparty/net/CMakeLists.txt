set(SOURCE_DIR ${PROJECT_3RDPARTY_SRC_DIR}/net/acc_links)
set(BUILD_DIR ${PROJECT_BUILD_PATH}/build/acc_links)
set(INSTALL_DIR ${PROJECT_3RDPARTY_BIN_DIR}/acc_links)

set(CHECK_FILE_LINKS ${INSTALL_DIR}/lib/libacc_tcp_net.a)

message(STATUS "acc_links: check if need to build at ${CHECK_FILE_LINKS}")

set(ACCLINK_BUILD_TYPE release)
IF (x${CMAKE_BUILD_TYPE} STREQUAL "xDEBUG")
    set(ACCLINK_BUILD_TYPE debug)
endif ()
IF (x${CMAKE_BUILD_TYPE} STREQUAL "xASAN")
    set(ACCLINK_BUILD_TYPE asan)
endif ()

# build the lib if not built yet
if(EXISTS ${CHECK_FILE_LINKS})
    message(STATUS "acc_links: ${CHECK_FILE_LINKS}")
    message(STATUS "acc_links: has been built, ignored")
else()
    # create dir
    exec_program(mkdir ${BUILD_DIR} ARGS -p ${BUILD_DIR})
    exec_program(mkdir ${BUILD_DIR} ARGS -p ${INSTALL_DIR})

    # configure
    exec_program(cmake ${BUILD_DIR} ARGS -DBUILD_OPEN_ABI=${BUILD_OPEN_ABI} -DCMAKE_BUILD_TYPE=${ACCLINK_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${SOURCE_DIR})

    # make and make install
    exec_program(make ${BUILD_DIR} ARGS clean)
    exec_program(make ${BUILD_DIR} ARGS install -j8)

    message("-- acc_links: has been success installed")
endif()
