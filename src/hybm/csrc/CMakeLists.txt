include_directories(
        ${PROJECT_HYBM_SRC_BASE}/include
        ${PROJECT_HYBM_SRC_BASE}/csrc/common
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation
        ${PROJECT_HYBM_SRC_BASE}/csrc/data_operation/host
        ${PROJECT_HYBM_SRC_BASE}/csrc/entity
        ${PROJECT_HYBM_SRC_BASE}/csrc/mm
        ${PROJECT_HYBM_SRC_BASE}/csrc/under_api
)

## add files into src variables
aux_source_directory(common SRC_FILES_COMMON)
aux_source_directory(data_operation SRC_FILES_DATA_OP)
aux_source_directory(data_operation/host SRC_FILES_DATA_OP)
aux_source_directory(entity SRC_FILES_ENTITY)
aux_source_directory(mm SRC_FILES_MEM_MANAGER)
aux_source_directory(under_api SRC_FILES_CANN)

# set whole source for hybmm src files
set(HYBMM_SRC_FILES
        ${SRC_FILES_COMMON}
        ${SRC_FILES_DATA_OP}
        ${SRC_FILES_ENTITY}
        ${SRC_FILES_CANN}
        ${SRC_FILES_MEM_MANAGER}
        hybm_entry.cpp
        hybm_big_mem_entry.cpp
        hybm_data_op_entry.cpp
)

## build the static and shared library
add_library(hybmm_objects OBJECT ${HYBMM_SRC_FILES})
target_compile_options(hybmm_objects PRIVATE -fPIC)

add_library(hybmm_static STATIC $<TARGET_OBJECTS:hybmm_objects>)
set_target_properties(hybmm_static PROPERTIES OUTPUT_NAME "mf_hybm_core")
target_link_libraries(hybmm_static PUBLIC pthread dl)

add_library(hybmm_shared SHARED $<TARGET_OBJECTS:hybmm_objects>)
set_target_properties(hybmm_shared PROPERTIES OUTPUT_NAME "mf_hybm_core")
target_link_libraries(hybmm_shared PUBLIC pthread dl)

## install header files
file(GLOB HEADER_FILES ${PROJECT_HYBM_SRC_BASE}/include/*.h)
install(
        FILES ${HEADER_FILES}
        DESTINATION ${PROJECT_HYBM_OUTPUT}/include
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

## install library
install(
        TARGETS hybmm_static hybmm_shared
        DESTINATION ${PROJECT_HYBM_OUTPUT}/lib
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
