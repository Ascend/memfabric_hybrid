include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/googletest/googletest/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/googletest/googlemock/include)

include_directories(
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_HYBM_SRC_BASE}/include
        ${PROJECT_ACCLINKS_SRC_BASE}/include
)
add_subdirectory(3rdparty)
link_directories(${PROJECT_3RDPARTY_BIN_DIR}/googletest/lib)

add_subdirectory(ut)
add_subdirectory(ut/mock)
#
#set(ENABLE_FUZZ $ENV{ENABLE_FUZZ})
#if (ENABLE_FUZZ STREQUAL "ON")
#    message(STATUS "Fuzz enabled.")
#    add_subdirectory(fuzz)
#endif ()