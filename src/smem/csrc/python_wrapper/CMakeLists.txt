project(pysmem LANGUAGES CXX)

add_compile_options(-fabi-version=0)
add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)

find_package(pybind11 REQUIRED)
pybind11_add_module(_pysmem pysmem.cpp)
set_target_properties(_pysmem PROPERTIES CXX_STANDARD 17)
target_link_libraries(_pysmem PRIVATE smem_shared)

install(TARGETS _pysmem
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/smem/lib64
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
)