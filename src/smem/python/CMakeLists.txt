project(pysmem LANGUAGES CXX)

add_compile_options(-fabi-version=0)
add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)

find_package(pybind11 REQUIRED)
pybind11_add_module(pysmem pysmem.cpp)
set_target_properties(pysmem PROPERTIES CXX_STANDARD 17)
set_target_properties(pysmem PROPERTIES OUTPUT_NAME "smem")
target_link_libraries(pysmem PRIVATE smem_shared)

install(TARGETS pysmem
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/smem/lib64
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
)