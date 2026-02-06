set(CONFIG_STORE_INCLUDE_DIRS
        ${PROJECT_SMEM_SRC_BASE}/csrc/common
        ${PROJECT_ACCLINKS_SRC_BASE}/include
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/common
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/backend
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/ha
        ${PROJECT_SMEM_SRC_BASE}/csrc/config_store/tcp_store
)

aux_source_directory(. CONFIG_STORE_SRCS)
aux_source_directory(${PROJECT_SMEM_SRC_BASE}/csrc/config_store/common CONFIG_STORE_SRCS)
aux_source_directory(${PROJECT_SMEM_SRC_BASE}/csrc/config_store/backend CONFIG_STORE_SRCS)
aux_source_directory(${PROJECT_SMEM_SRC_BASE}/csrc/config_store/ha CONFIG_STORE_SRCS)
aux_source_directory(${PROJECT_SMEM_SRC_BASE}/csrc/config_store/tcp_store CONFIG_STORE_SRCS)

add_library(config_store_object OBJECT ${CONFIG_STORE_SRCS})

target_include_directories(config_store_object PUBLIC ${CONFIG_STORE_INCLUDE_DIRS})

target_compile_options(config_store_object PRIVATE -fPIC)