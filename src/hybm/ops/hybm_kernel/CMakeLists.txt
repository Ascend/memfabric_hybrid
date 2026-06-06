set(CMAKE_SKIP_RPATH TRUE)
include(${PROJECT_SOURCE_DIR}/cmake/kernel/hybm_pack_sign.cmake)

set(HYBM_AICPU_PACKAGE_NAME cann-hybm-compat.tar.gz)

add_library(cann_hybm_kernel SHARED
        hybm_batch_transfer.cc
)

target_include_directories(cann_hybm_kernel PRIVATE
        ${PROJECT_HYBM_SRC_BASE}/include
        ${PROJECT_HYBM_SRC_BASE}/csrc/common
        ${PROJECT_HYBM_SRC_BASE}/csrc/driver/version
        ${PROJECT_HYBM_SRC_BASE}/csrc/under_api
        ${PROJECT_UTIL_SRC_BASE}
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libcann_hybm_kernel.json
        COMMAND python3 ${PROJECT_SOURCE_DIR}/script/kernel/parser_ini.py
                ${CMAKE_CURRENT_SOURCE_DIR}/libcann_hybm_kernel.ini
                ${CMAKE_CURRENT_BINARY_DIR}/libcann_hybm_kernel.json
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libcann_hybm_kernel.ini
                ${PROJECT_SOURCE_DIR}/script/kernel/parser_ini.py
)
add_custom_target(hybm_aicpu_kernel_json DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libcann_hybm_kernel.json)
add_dependencies(cann_hybm_kernel hybm_aicpu_kernel_json)

pack_targets_and_files(
        OUTPUT_TARGET pack_aicpu_hybm
        OUTPUT ${HYBM_AICPU_PACKAGE_NAME}
        TARGETS cann_hybm_kernel
)

install(TARGETS cann_hybm_kernel
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/hybm/aicpu_kernel
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libcann_hybm_kernel.json
        DESTINATION ${TARGET_INSTALL_DIR}/hybm/aicpu_kernel
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${HYBM_AICPU_PACKAGE_NAME}
        DESTINATION ${TARGET_INSTALL_DIR}/hybm/aicpu_kernel
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install(PROGRAMS ${PROJECT_SOURCE_DIR}/script/kernel/install.sh
        DESTINATION ${TARGET_INSTALL_DIR}/hybm/aicpu_kernel
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
