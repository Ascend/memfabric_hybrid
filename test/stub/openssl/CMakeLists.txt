include_directories(
    ${PROJECT_SOURCE_DIR}/src/net/csrc/under_api/openssl
)

set(SSL_STUB_FILE
    ssl_stub.cpp
)

add_library(ssl_object OBJECT ${SSL_STUB_FILE})
target_compile_options(ssl_object PRIVATE -fPIC)

add_library(ssl_shared SHARED $<TARGET_OBJECTS:ssl_object>)
set_target_properties(ssl_shared PROPERTIES OUTPUT_NAME "ssl")


set(CRYPTO_STUB_FILE
    crypto_stub.cpp
)

add_library(crypto_object OBJECT ${CRYPTO_STUB_FILE})
target_compile_options(crypto_object PRIVATE -fPIC)

add_library(crypto_shared SHARED $<TARGET_OBJECTS:crypto_object>)
set_target_properties(crypto_shared PROPERTIES OUTPUT_NAME "crypto")

install(
    TARGETS ssl_shared crypto_shared
    DESTINATION ${PROJECT_OUTPUT_PATH}/3rdparty/openssl/lib
    PERMISSIONS OWNER_READ GROUP_READ
)
