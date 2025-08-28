include_directories(
        ${PROJECT_UTIL_SRC_BASE}/ptracer/include
        ${PROJECT_UTIL_SRC_BASE}/ptracer
        ${PROJECT_UTIL_SRC_BASE}/ptracer/tracers
)

aux_source_directory(. PTRACER_SRCS)
aux_source_directory(tracers PTRACER_SRCS)

add_library(ptracer_object OBJECT ${PTRACER_SRCS})

target_compile_options(ptracer_object PRIVATE -fPIC)