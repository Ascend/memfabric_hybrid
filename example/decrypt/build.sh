#!/bin/bash

# 创建构建目录
mkdir -p build
cd build

# 运行CMake配置
cmake ..

# 构建项目
make

# 复制生成的库文件到示例目录
cp libdecrypt.so ..

echo "Build completed. The libdecrypt.so library is in the current directory."