set(HYBM_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/hybm/include")

if (BUILD_ACC_LINK STREQUAL "ON")
    add_subdirectory(net)
else()
    add_subdirectory(smem/csrc)
    add_subdirectory(hybm/csrc)
    add_subdirectory(mooncake_adapter/csrc)
endif()