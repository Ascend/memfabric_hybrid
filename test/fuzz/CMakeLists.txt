set(ROOT_DIR ${CMAKE_SOURCE_DIR})
set(FUZZ_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 检查SecodeFuzz
set(SECODEFUZZ_INSTALL_DIR "${ROOT_DIR}/test/3rdparty/secodefuzz")
if (NOT EXISTS ${SECODEFUZZ_INSTALL_DIR})
    message(FATAL_ERROR "SECODEFUZZ_INSTALL_DIR(${SECODEFUZZ_INSTALL_DIR}) is invalid, Please git clone secodefuzz first.")
endif()
include_directories(${SECODEFUZZ_INSTALL_DIR}/include)
link_directories(${SECODEFUZZ_INSTALL_DIR}/lib)

file(GLOB_RECURSE FUZZ_GLOBAL_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

message(STATUS "fuzz build")

add_subdirectory(memcache)