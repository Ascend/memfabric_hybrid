cmake_minimum_required(VERSION 3.10)

# 项目名称
project(decrypt_example)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)

# 创建共享库
add_library(decrypt SHARED decrypt_example.cpp)

# 设置输出名称为libdecrypt.so
set_target_properties(decrypt PROPERTIES
    OUTPUT_NAME "decrypt"
    PREFIX "lib"
    SUFFIX ".so"
)

# 安装规则
install(TARGETS decrypt
    LIBRARY DESTINATION lib
)