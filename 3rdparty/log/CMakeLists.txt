set(SOURCE_DIR ${PROJECT_3RDPARTY_SRC_DIR}/log/spdlog)
set(BUILD_DIR ${PROJECT_BUILD_PATH}/build/spdlog)
set(INSTALL_DIR ${PROJECT_3RDPARTY_BIN_DIR}/spdlog)
set(CHECK_FILE_LINKS ${INSTALL_DIR}/lib/libspdlog.a)
set(SPDLOG_BUILD_TYPE release)
IF (x${CMAKE_BUILD_TYPE} STREQUAL "xDEBUG")
    set(SPDLOG_BUILD_TYPE debug)
    set(CHECK_FILE_LINKS ${INSTALL_DIR}/lib/libspdlogd.a)
endif ()
message(STATUS "spdlog: check if need to build at ${CHECK_FILE_LINKS}")

# build the lib if not built yet
if(EXISTS ${CHECK_FILE_LINKS})
    message(STATUS "spdlog: ${CHECK_FILE_LINKS}")
    message(STATUS "spdlog: has been built, ignored")
else()
    # create dir
    exec_program(mkdir ${BUILD_DIR} ARGS -p ${BUILD_DIR})
    exec_program(mkdir ${BUILD_DIR} ARGS -p ${INSTALL_DIR})

    # configure
    exec_program(cmake ${BUILD_DIR} ARGS -DCMAKE_BUILD_TYPE=${SPDLOG_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${SOURCE_DIR})

    # make and make install
    exec_program(make ${BUILD_DIR} ARGS clean)
    exec_program(make ${BUILD_DIR} ARGS install -j8)

    message("-- spdlog: has been success installed")
endif()
