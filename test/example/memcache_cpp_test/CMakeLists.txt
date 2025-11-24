# 递归查找所有源文件
file(GLOB_RECURSE ALL_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

# 过滤源文件：排除文件名XX 或路径包含XX的文件
set(SOURCES "")
foreach (source ${ALL_SOURCES})
    # 获取文件名和路径
    get_filename_component(FILENAME ${source} NAME)
    get_filename_component(SOURCE_PATH ${source} PATH)

    # 检查过滤条件
    string(FIND "${FILENAME}" "mock" FILENAME_MATCH)
    string(FIND "${SOURCE_PATH}" "/src/hybm/" PATH_MATCH)
    string(FIND "${SOURCE_PATH}" "/src/driver/" PATH_MATCH2)

    # 如果都不匹配，则添加到源文件列表
    if (FILENAME_MATCH EQUAL -1 AND PATH_MATCH EQUAL -1 AND PATH_MATCH2 EQUAL -1)
        list(APPEND SOURCES ${source})
    endif ()
endforeach ()

# 递归查找所有头文件目录
file(GLOB_RECURSE ALL_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hxx"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hh"
        "${PROJECT_SOURCE_DIR}/src/memcache/include/*.h"
        "${PROJECT_SOURCE_DIR}/src/memcache/csrc/common/*.h"
)

# 提取头文件目录并去重，同时过滤掉路径包含XXX的目录
set(INCLUDE_DIRS "")
foreach (header ${ALL_HEADERS})
    # 获取头文件所在目录
    get_filename_component(HEADER_DIR ${header} PATH)

    # 检查是否包含"/aaa/"
    string(FIND "${HEADER_DIR}" "/src/hybm/" PATH_MATCH)

    # 如果路径不包含"/aaa/"，则添加到包含目录
    if (PATH_MATCH EQUAL -1)
        list(APPEND INCLUDE_DIRS ${HEADER_DIR})
    endif ()
endforeach ()

# 去重包含目录
list(REMOVE_DUPLICATES INCLUDE_DIRS)

# 添加包含目录
include_directories(${INCLUDE_DIRS})

# 统计信息
list(LENGTH SOURCES SOURCES_COUNT)
list(LENGTH INCLUDE_DIRS INCLUDE_DIRS_COUNT)
message(STATUS "Project configured with ${SOURCES_COUNT} source files")
message(STATUS "Project configured with ${INCLUDE_DIRS_COUNT} include directories")

# 创建可执行文件
add_executable(memcache_cpp_test ${SOURCES})
target_link_libraries(memcache_cpp_test PUBLIC mmc_shared)
# 查找 Python 开发库
find_package(Python3 REQUIRED COMPONENTS Development)
target_link_libraries(memcache_cpp_test PUBLIC
        Python3::Python  # 添加 Python 库
        pthread
        dl
)

# 设置目标属性
target_include_directories(memcache_cpp_test PUBLIC ${INCLUDE_DIRS})
target_include_directories(memcache_cpp_test PUBLIC ${PROJECT_SOURCE_DIR}/src/hybm_ubsm/csrc/include)

# 可选：添加编译特性
target_compile_features(memcache_cpp_test PUBLIC cxx_std_17)

# 可选：设置编译器特定选项
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(memcache_cpp_test PRIVATE -W)
endif ()

# 显示过滤后的源文件和包含目录（调试用，正式使用可注释掉）
#-DCMAKE_VERBOSE_MAKEFILE=ON
if (CMAKE_VERBOSE_MAKEFILE)
    message(STATUS "Source files:")
    foreach (source ${SOURCES})
        message(STATUS "  ${source}")
    endforeach ()

    message(STATUS "Include directories:")
    foreach (dir ${INCLUDE_DIRS})
        message(STATUS "  ${dir}")
    endforeach ()
endif ()

install(TARGETS memcache_cpp_test
        RUNTIME DESTINATION ${TARGET_INSTALL_DIR}/memcache/bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)