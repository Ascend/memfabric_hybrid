if(BUILD_PYTHON STREQUAL "ON")
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
    find_package(pybind11 REQUIRED)
endif()

include_directories(
        ${PROJECT_MMC_SRC_BASE}/include/
        ${PROJECT_MMC_SRC_BASE}/csrc/common
        ${PROJECT_MMC_SRC_BASE}/csrc/config
)

if(BUILD_PYTHON STREQUAL "ON")
    include_directories(
         ${PYTHON_INCLUDE_DIR}
         ${PYTHON_PYBIND11_HOME}/include/
    )
endif()

aux_source_directory(. MMC_LEADER_SRCS)

add_library(mmc_leader_objects OBJECT ${MMC_LEADER_SRCS})
target_compile_options(mmc_leader_objects PRIVATE -fPIC -Wno-float-equal)

add_library(mmc_leader_shared SHARED $<TARGET_OBJECTS:mmc_leader_objects>)
set_target_properties(mmc_leader_shared PROPERTIES OUTPUT_NAME "mmc_leader")
target_link_libraries(mmc_leader_shared PUBLIC pthread)

#install head files
file(GLOB PYTHON_FILES *.py)
install(FILES ${PYTHON_FILES} DESTINATION ${TARGET_INSTALL_DIR}/memcache/bin)

# install library
install(TARGETS mmc_leader_shared
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/memcache/bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
)
