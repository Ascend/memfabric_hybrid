include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/googletest/googletest/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/googletest/googlemock/include)
link_directories(${PROJECT_3RDPARTY_BIN_DIR}/googletest/lib)

add_subdirectory(3rdparty)
add_subdirectory(ut)