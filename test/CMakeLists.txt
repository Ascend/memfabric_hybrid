include_directories(
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/include/
)

add_subdirectory(3rdparty)
add_subdirectory(stub)

set(ENABLE_FUZZ $ENV{ENABLE_FUZZ})
if (ENABLE_FUZZ STREQUAL "ON")
    message(STATUS "ENABLE_FUZZ is ${ENABLE_FUZZ}")
    if (BUILD_ACC_LINK STREQUAL "ON")
        add_subdirectory(acc_link_fuzz)
    else()
        add_subdirectory(fuzz)
        add_subdirectory(ut/mock)
    endif()
else ()
    if (BUILD_ACC_LINK STREQUAL "ON")
        add_subdirectory(llt)
    else()
        add_subdirectory(ut)
        add_subdirectory(ut/mock)
    endif()
endif()