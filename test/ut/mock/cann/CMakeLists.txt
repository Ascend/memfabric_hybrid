set(ACL_STUB_FILE
    acl_stub.cpp
)

add_library(acl_object OBJECT ${ACL_STUB_FILE})
target_compile_options(acl_object PRIVATE -fPIC)

add_library(acl_shared SHARED $<TARGET_OBJECTS:acl_object>)
set_target_properties(acl_shared PROPERTIES OUTPUT_NAME "ascendcl")


set(HAL_STUB_FILE
    hal_stub.cpp
)

add_library(hal_object OBJECT ${HAL_STUB_FILE})
target_compile_options(hal_object PRIVATE -fPIC)

add_library(hal_shared SHARED $<TARGET_OBJECTS:hal_object>)
set_target_properties(hal_shared PROPERTIES OUTPUT_NAME "ascend_hal")

install(
        TARGETS acl_shared
        DESTINATION ${PROJECT_HYBM_OUTPUT}/lib64/cann/lib64
        PERMISSIONS OWNER_READ GROUP_READ
)

install(
        TARGETS hal_shared
        DESTINATION ${PROJECT_HYBM_OUTPUT}/lib64/cann/driver/lib64
        PERMISSIONS OWNER_READ GROUP_READ
)

install(
        FILES version.info
        DESTINATION ${PROJECT_HYBM_OUTPUT}/lib64/cann/driver
        PERMISSIONS OWNER_READ GROUP_READ
)