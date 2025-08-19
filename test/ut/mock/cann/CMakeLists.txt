include_directories(
        ${PROJECT_HYBM_SRC_BASE}/csrc/common
        ${PROJECT_HYBM_SRC_BASE}/csrc/under_api
)

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

set(TSD_STUB_FILE
    tsd_stub.cpp
)

add_library(tsd_object OBJECT ${TSD_STUB_FILE})
target_compile_options(tsd_object PRIVATE -fPIC)

add_library(tsd_shared SHARED $<TARGET_OBJECTS:tsd_object>)
set_target_properties(tsd_shared PROPERTIES OUTPUT_NAME "tsdclient")

set(RA_STUB_FILE
    ra_stub.cpp
)

add_library(ra_object OBJECT ${RA_STUB_FILE})
target_compile_options(ra_object PRIVATE -fPIC)

add_library(ra_shared SHARED $<TARGET_OBJECTS:ra_object>)
set_target_properties(ra_shared PROPERTIES OUTPUT_NAME "ra")

set(HCOM_STUB_FILE
    hcom_stub.cpp
)

add_library(hcom_object OBJECT ${HCOM_STUB_FILE})
target_compile_options(hcom_object PRIVATE -fPIC)

add_library(hcom_shared SHARED $<TARGET_OBJECTS:hcom_object>)
set_target_properties(hcom_shared PROPERTIES OUTPUT_NAME "hcom")

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
        TARGETS tsd_shared
        DESTINATION ${PROJECT_HYBM_OUTPUT}/lib64/cann/driver/lib64
        PERMISSIONS OWNER_READ GROUP_READ
)

install(
        TARGETS ra_shared
        DESTINATION ${PROJECT_HYBM_OUTPUT}/lib64/cann/driver/lib64
        PERMISSIONS OWNER_READ GROUP_READ
)

install(
        TARGETS hcom_shared
        DESTINATION ${PROJECT_HYBM_OUTPUT}/lib64/cann/driver/lib64
        PERMISSIONS OWNER_READ GROUP_READ
)

install(
        FILES version.info
        DESTINATION ${PROJECT_HYBM_OUTPUT}/lib64/cann/driver
        PERMISSIONS OWNER_READ GROUP_READ
)