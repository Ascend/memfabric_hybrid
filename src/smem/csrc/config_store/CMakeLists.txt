include_directories(
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_ACCLINKS_SRC_BASE}/include
)
aux_source_directory(. CONFIG_STORE_SRCS)
add_library(config_store_object OBJECT ${CONFIG_STORE_SRCS})
target_compile_options(config_store_object PRIVATE -fPIC)