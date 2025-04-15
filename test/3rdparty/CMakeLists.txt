include(ExternalProject)
set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/test/3rdparty)
set(INSTALL_DIR ${PROJECT_3RDPARTY_BIN_DIR}/googletest)

set(CHECK_FILE_LINKS ${INSTALL_DIR}/include/gtest/gtest.h)
message("-- googletest: check if need to build at ${CHECK_FILE_LINKS}")

if(EXISTS ${CHECK_FILE_LINKS})
    message("-- googletest: ${CHECK_FILE_LINKS}")
    message("-- googletest: has been built, ignored")
    return()
endif()

message("-- googletest: will build it")
set(THIRD_PARTY_CMAKE_ARGS
        -DCMAKE_C_COMPILER=gcc
        -DCMAKE_CXX_COMPILER=g++
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}
        -DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}
        -DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}
        -DCMAKE_BUILD_TYPE=RELEASE
        -DCMAKE_INSTALL_LIBDIR=${INSTALL_DIR}/lib
        -DCMAKE_INSTALL_BINDIR=${INSTALL_DIR}/bin
        -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}/)

######################################################################
# Google Test and Google Mock
######################################################################
set(GTEST_CMAKE_ARGS
        ${THIRD_PARTY_CMAKE_ARGS}
        -DINSTALL_GTEST=ON
        -DBUILD_SHARED_LIBS=OFF
        -Dgmock_build_test=OFF
        -Dgtest_build_tests=OFF
        -Dgtest_build_samples=OFF)

externalproject_add(googletest
        URL ${THIRD_PARTY_DIR}/googletest
        SOURCE_DIR ${THIRD_PARTY_DIR}/googletest
        CMAKE_ARGS ${GTEST_CMAKE_ARGS}
        DOWNLOAD_COMMAND ""
        LOG_DOWNLOAD On
        LOG_CONFIGURE On
        LOG_BUILD On
        LOG_INSTALL On)

add_library(GTEST::GTEST STATIC IMPORTED GLOBAL)
externalproject_get_property(googletest INSTALL_DIR)
externalproject_get_property(googletest BINARY_DIR)
set(GTEST_LIBS_DIR ${INSTALL_DIR}/lib PARENT_SCOPE)
set(GTEST_INCLUDE_DIR ${INSTALL_DIR}/include PARENT_SCOPE)
set_target_properties(GTEST::GTEST PROPERTIES IMPORTED_LOCATION ${INSTALL_DIR}/lib/libgtestd.a)


