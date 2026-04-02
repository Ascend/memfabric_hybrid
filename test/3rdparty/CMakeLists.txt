#include(ExternalProject)
#set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/test/3rdparty)
#set(INSTALL_DIR ${PROJECT_3RDPARTY_BIN_DIR}/googletest)
#set(MOCKCPP_INSTALL_DIR ${PROJECT_3RDPARTY_BIN_DIR}/mockcpp)

set(TEST_3RDPARTY_DIR ${PROJECT_SOURCE_DIR}/test/3rdparty)
set(TEST_3RDPARTY_OUTPUT_DIR ${PROJECT_3RDPARTY_BIN_DIR})

######################################################################
# Build google test
######################################################################
set(SOURCE_DIR ${TEST_3RDPARTY_DIR}/googletest)
set(BUILD_DIR ${TEST_3RDPARTY_DIR}/googletest/build)
set(INSTALL_DIR ${TEST_3RDPARTY_OUTPUT_DIR}/googletest)

set(CHECK_FILE_LINKS ${INSTALL_DIR}/lib/libgtest.a)
message(STATUS "googletest: check if need to build at ${CHECK_FILE_LINKS}")

# build the lib if not built yet
if (EXISTS ${CHECK_FILE_LINKS})
    message(STATUS "googletest: ${CHECK_FILE_LINKS}")
    message(STATUS "googletest: has been built, ignored")
else ()
    # Create directories
    execute_process(COMMAND mkdir -p ${BUILD_DIR} RESULT_VARIABLE MKDIR1_RESULT OUTPUT_VARIABLE MKDIR1_OUT ERROR_VARIABLE MKDIR1_ERR)
    if (NOT MKDIR1_RESULT EQUAL 0)
        message(FATAL_ERROR "googletest: Failed to create directory: ${BUILD_DIR} ${MKDIR1_OUT} ${MKDIR1_ERR}")
    endif ()

    execute_process(COMMAND mkdir -p ${INSTALL_DIR} RESULT_VARIABLE MKDIR2_RESULT OUTPUT_VARIABLE MKDIR2_OUT ERROR_VARIABLE MKDIR2_ERR)
    if (NOT MKDIR2_RESULT EQUAL 0)
        message(FATAL_ERROR "googletest: Failed to create directory: ${INSTALL_DIR} ${MKDIR2_OUT} ${MKDIR2_ERR}")
    endif ()

    # Detect ninja
    find_program(NINJA_EXE NAMES ninja)
    set(GENERATOR "Unix Makefiles")
    if (NINJA_EXE)
        set(MAKE_CMD ninja)
        set(GENERATOR "Ninja")
    else ()
        set(MAKE_CMD make)
    endif ()

    # Configure
    execute_process(COMMAND cmake
            -G "${GENERATOR}" -S ${SOURCE_DIR} -B ${BUILD_DIR}
            -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
            -DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE} -DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}
            -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS} -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}
            -DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG} -DCMAKE_BUILD_TYPE=DEBUG
            -DCMAKE_INSTALL_LIBDIR=${INSTALL_DIR}/lib -DCMAKE_INSTALL_BINDIR=${INSTALL_DIR}/bin
            -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}/
            WORKING_DIRECTORY ${BUILD_DIR} RESULT_VARIABLE CMAKE_RESULT OUTPUT_VARIABLE CMAKE_OUT ERROR_VARIABLE CMAKE_ERR
    )
    if (NOT CMAKE_RESULT EQUAL 0)
        message(FATAL_ERROR "googletest: CMake configure failed! ${CMAKE_OUT} ${CMAKE_ERR}")
    endif ()

    # Clean
    execute_process(COMMAND ${MAKE_CMD} clean WORKING_DIRECTORY ${BUILD_DIR}
            RESULT_VARIABLE CLEAN_RESULT OUTPUT_VARIABLE CLEAN_OUT ERROR_VARIABLE CLEAN_ERR
    )
    if (NOT CLEAN_RESULT EQUAL 0)
        message(FATAL_ERROR "googletest: ${MAKE_CMD} clean failed! ${CLEAN_OUT} ${CLEAN_ERR}")
    endif ()

    # Install
    execute_process(COMMAND ${MAKE_CMD} install -j8 WORKING_DIRECTORY ${BUILD_DIR}
            RESULT_VARIABLE INSTALL_RESULT OUTPUT_VARIABLE INSTALL_OUT ERROR_VARIABLE INSTALL_ERR)
    if (NOT INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "googletest: ${MAKE_CMD} install failed! ${INSTALL_OUT} ${INSTALL_ERR}")
    endif ()

    message(STATUS "googletest: has been success installed by ${MAKE_CMD}")
endif ()

######################################################################
# Mockcpp
######################################################################
set(SOURCE_DIR ${TEST_3RDPARTY_DIR}/mockcpp)
set(BUILD_DIR ${TEST_3RDPARTY_DIR}/mockcpp/build)
set(INSTALL_DIR ${TEST_3RDPARTY_OUTPUT_DIR}/mockcpp)

set(MOCKCPP_PATCH ${PROJECT_SOURCE_DIR}/test/3rdparty/patch/mockcpp_support_arm64.patch)

set(CHECK_FILE_LINKS ${INSTALL_DIR}/lib/libmockcpp.a)
message(STATUS "mockcpp: check if need to build at ${CHECK_FILE_LINKS}")

# build the lib if not built yet
if (EXISTS ${CHECK_FILE_LINKS})
    message(STATUS "mockcpp: ${CHECK_FILE_LINKS}")
    message(STATUS "mockcpp: has been built, ignored")
else ()
    # Create directories
    execute_process(COMMAND mkdir -p ${BUILD_DIR} RESULT_VARIABLE MKDIR1_RESULT OUTPUT_VARIABLE MKDIR1_OUT ERROR_VARIABLE MKDIR1_ERR)
    if (NOT MKDIR1_RESULT EQUAL 0)
        message(FATAL_ERROR "mockcpp: Failed to create directory: ${BUILD_DIR} ${MKDIR1_OUT} ${MKDIR1_ERR}")
    endif ()

    execute_process(COMMAND mkdir -p ${INSTALL_DIR} RESULT_VARIABLE MKDIR2_RESULT OUTPUT_VARIABLE MKDIR2_OUT ERROR_VARIABLE MKDIR2_ERR)
    if (NOT MKDIR2_RESULT EQUAL 0)
        message(FATAL_ERROR "mockcpp: Failed to create directory: ${INSTALL_DIR} ${MKDIR2_OUT} ${MKDIR2_ERR}")
    endif ()

    message(STATUS "mockcpp: start to aplly patch ${MOCKCPP_PATCH} to ${SOURCE_DIR}")
    # execute patch
    execute_process(COMMAND patch -p1 --forward -i ${MOCKCPP_PATCH}
            WORKING_DIRECTORY ${SOURCE_DIR}
            RESULT_VARIABLE PATCH_RESULT OUTPUT_VARIABLE PATCH_OUT ERROR_VARIABLE PATCH_ERR)
    if (NOT PATCH_RESULT EQUAL 0 AND NOT ${PATCH_ERR} STREQUAL "")
        message(FATAL_ERROR "mockcpp: patch result result=${PATCH_RESULT} out=${PATCH_OUT} err=${PATCH_ERR}")
    endif ()

    # Detect ninja
    find_program(NINJA_EXE NAMES ninja)
    set(GENERATOR "Unix Makefiles")
    if (NINJA_EXE)
        set(MAKE_CMD ninja)
        set(GENERATOR "Ninja")
    else ()
        set(MAKE_CMD make)
    endif ()

    # Configure
    execute_process(COMMAND cmake
            -G "${GENERATOR}" -S ${SOURCE_DIR} -B ${BUILD_DIR}
            -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
            -DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE} -DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}
            -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS} -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}
            -DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG} -DCMAKE_BUILD_TYPE=DEBUG
            -DCMAKE_INSTALL_LIBDIR=${INSTALL_DIR}/lib -DCMAKE_INSTALL_BINDIR=${INSTALL_DIR}/bin
            -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}/
            WORKING_DIRECTORY ${BUILD_DIR} RESULT_VARIABLE CMAKE_RESULT OUTPUT_VARIABLE CMAKE_OUT ERROR_VARIABLE CMAKE_ERR
    )
    if (NOT CMAKE_RESULT EQUAL 0)
        message(FATAL_ERROR "mockcpp: CMake configure failed! ${CMAKE_OUT} ${CMAKE_ERR}")
    endif ()

    # Clean
    execute_process(COMMAND ${MAKE_CMD} clean WORKING_DIRECTORY ${BUILD_DIR}
            RESULT_VARIABLE CLEAN_RESULT OUTPUT_VARIABLE CLEAN_OUT ERROR_VARIABLE CLEAN_ERR
    )
    if (NOT CLEAN_RESULT EQUAL 0)
        message(FATAL_ERROR "mockcpp: ${MAKE_CMD} clean failed! ${CLEAN_OUT} ${CLEAN_ERR}")
    endif ()

    # Install
    execute_process(COMMAND ${MAKE_CMD} install -j8 WORKING_DIRECTORY ${BUILD_DIR}
            RESULT_VARIABLE INSTALL_RESULT OUTPUT_VARIABLE INSTALL_OUT ERROR_VARIABLE INSTALL_ERR)
    if (NOT INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "mockcpp: ${MAKE_CMD} install failed! ${INSTALL_OUT} ${INSTALL_ERR}")
    endif ()

    message("-- mockcpp: has been success installed by ${MAKE_CMD}")
endif ()


#set(MOCKCPP_CMAKE_ARGS
#        ${THIRD_PARTY_CMAKE_ARGS}
#        -DCMAKE_INSTALL_LIBDIR=${MOCKCPP_INSTALL_DIR}/lib
#        -DCMAKE_INSTALL_BINDIR=${MOCKCPP_INSTALL_DIR}/bin
#        -DCMAKE_INSTALL_PREFIX=${MOCKCPP_INSTALL_DIR}/)

#set(MOCKCPP_PATCH ${PROJECT_SOURCE_DIR}/test/3rdparty/patch/mockcpp_support_arm64.patch)

#externalproject_add(mock_cpp
#        URL ${THIRD_PARTY_DIR}/mockcpp
#        CMAKE_ARGS ${MOCKCPP_CMAKE_ARGS}
#        PATCH_COMMAND patch -p1 --forward -i ${MOCKCPP_PATCH}
#        BUILD_ALWAYS OFF
#        LOG_DOWNLOAD On
#        LOG_CONFIGURE On
#        LOG_BUILD On
#        LOG_INSTALL On
#        BUILD_BYPRODUCTS ${PROJECT_3RDPARTY_BIN_DIR}/mockcpp/lib/libmockcpp.a)
