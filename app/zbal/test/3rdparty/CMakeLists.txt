
set(SOURCE_DIR ${ZBAL_TEST_3RDPARTY_DIR}/googletest)
set(BUILD_DIR ${ZBAL_TEST_3RDPARTY_DIR}/googletest/build)
set(INSTALL_DIR ${ZBAL_TEST_3RDPARTY_OUTPUT_DIR}/googletest)

set(CHECK_FILE_LINKS ${INSTALL_DIR}/lib/libgtest.a)

message(STATUS "googletest: check if need to build at ${CHECK_FILE_LINKS}")

# build the lib if not built yet
if(EXISTS ${CHECK_FILE_LINKS})
    message(STATUS "googletest: ${CHECK_FILE_LINKS}")
    message(STATUS "googletest: has been built, ignored")
else()
    # Create directories
    execute_process(COMMAND mkdir -p ${BUILD_DIR} RESULT_VARIABLE MKDIR1_RESULT
                                                  OUTPUT_VARIABLE MKDIR1_OUT
                                                  ERROR_VARIABLE MKDIR1_ERR)
    if(NOT MKDIR1_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to create directory: ${BUILD_DIR} ${MKDIR1_OUT} ${MKDIR1_ERR}")
    endif()

    execute_process(COMMAND mkdir -p ${INSTALL_DIR} RESULT_VARIABLE MKDIR2_RESULT
                                                    OUTPUT_VARIABLE MKDIR2_OUT
                                                    ERROR_VARIABLE MKDIR2_ERR)
    if(NOT MKDIR2_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to create directory: ${INSTALL_DIR} ${MKDIR2_OUT} ${MKDIR2_ERR}")
    endif()

    # Detect ninja
    find_program(NINJA_EXE NAMES ninja)
    set(GENERATOR "Unix Makefiles")
    if(NINJA_EXE)
        set(MAKE_CMD ninja)
        set(GENERATOR "Ninja")
    else()
        set(MAKE_CMD make)
    endif()

    # Configure
    execute_process(COMMAND cmake
            -G "${GENERATOR}"
            -S ${SOURCE_DIR}
            -B ${BUILD_DIR}
            -DCMAKE_C_COMPILER=gcc
            -DCMAKE_CXX_COMPILER=g++
            -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
            -DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}
            -DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}
            -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
            -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}
            -DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}
            -DCMAKE_BUILD_TYPE=DEBUG
            -DCMAKE_INSTALL_LIBDIR=${INSTALL_DIR}/lib
            -DCMAKE_INSTALL_BINDIR=${INSTALL_DIR}/bin
            -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}/
            WORKING_DIRECTORY ${BUILD_DIR}
            RESULT_VARIABLE CMAKE_RESULT
            OUTPUT_VARIABLE CMAKE_OUT
            ERROR_VARIABLE CMAKE_ERR
    )
    if(NOT CMAKE_RESULT EQUAL 0)
        message(FATAL_ERROR "CMake configure failed! ${CMAKE_OUT} ${CMAKE_ERR}")
    endif()

    # Clean
    execute_process(
            COMMAND ${MAKE_CMD} clean
            WORKING_DIRECTORY ${BUILD_DIR}
            RESULT_VARIABLE CLEAN_RESULT
            OUTPUT_VARIABLE CLEAN_OUT
            ERROR_VARIABLE CLEAN_ERR
    )
    if(NOT CLEAN_RESULT EQUAL 0)
        message(FATAL_ERROR "${MAKE_CMD} clean failed! ${CLEAN_OUT} ${CLEAN_ERR}")
    endif()

    # Install
    execute_process(COMMAND ${MAKE_CMD} install -j8 WORKING_DIRECTORY ${BUILD_DIR}
                    RESULT_VARIABLE INSTALL_RESULT OUTPUT_VARIABLE INSTALL_OUT ERROR_VARIABLE INSTALL_ERR)
    if(NOT INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "${MAKE_CMD} install failed! ${INSTALL_OUT} ${INSTALL_ERR}")
    endif()

    message("-- googletest: has been success installed by ${MAKE_CMD}")
endif()