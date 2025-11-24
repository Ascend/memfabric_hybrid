file(GLOB SOURCES "*.cpp")
set(PYTHON_EXECUTABLE "python3")
execute_process(
        COMMAND ${PYTHON_EXECUTABLE} -c "import sys; print([s for s in sys.path if 'packages' in s][0])"
        OUTPUT_VARIABLE PYTHON_SYS_PATH
)
string(STRIP ${PYTHON_SYS_PATH} PYTHON_SYS_PATH)

set(PYTHON_PACKAGE_NAME "_pymmc")

find_package(pybind11 REQUIRED)

pybind11_add_module(_pymmc ${SOURCES} pymmc.cpp)

target_link_libraries(_pymmc PUBLIC mmc_shared)

install(TARGETS _pymmc
        LIBRARY DESTINATION ${TARGET_INSTALL_DIR}/memcache/lib64
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
)