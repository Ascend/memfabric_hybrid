aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/csrc HYBM_USER_SOURCES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/csrc/common HYBM_USER_SOURCES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/csrc/manager HYBM_USER_SOURCES)

add_library(hybm_gvm_object OBJECT ${HYBM_USER_SOURCES})
target_compile_options(hybm_gvm_object PRIVATE -fPIC)

target_include_directories(hybm_gvm_object PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_include_directories(hybm_gvm_object PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(hybm_gvm_object PUBLIC ${PROJECT_UTIL_SRC_BASE}/)
target_include_directories(hybm_gvm_object PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/csrc/include)
target_include_directories(hybm_gvm_object PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/csrc/manager)

add_library(hybm_gvm_static STATIC $<TARGET_OBJECTS:hybm_gvm_object>)
set_target_properties(hybm_gvm_static PROPERTIES OUTPUT_NAME "hybm_gvm")

add_library(hybm_gvm_shared SHARED $<TARGET_OBJECTS:hybm_gvm_object>)
set_target_properties(hybm_gvm_shared PROPERTIES OUTPUT_NAME "hybm_gvm")

## install header files
install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${PROJECT_DRIVER_OUTPUT}/include
        FILE_PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
        DIRECTORY_PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE
        FILES_MATCHING PATTERN "*.h"
)

# install library
install(
        TARGETS hybm_gvm_static hybm_gvm_shared
        DESTINATION ${PROJECT_DRIVER_OUTPUT}/lib64
        PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE
)
