include_directories(
        ${PROJECT_SMEM_SRC_BASE}/include/host
        ${PROJECT_3RDPARTY_BIN_DIR}/acc_links/include/
)

add_subdirectory(3rdparty)
add_subdirectory(ut)
add_subdirectory(ut/mock)