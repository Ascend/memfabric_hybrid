# 自定义解密库示例

## 概述

此示例演示了如何创建一个自定义解密库，该库可以被memfabric_hybrid项目加载以解密TLS私钥密码。

系统会尝试加载名为`libdecrypt.so`的动态库，并在其中查找名为`DecryptPassword`的函数。

## 文件说明

- `decrypt_example.cpp`: 示例解密库的实现代码
- `CMakeLists.txt`: CMake构建配置文件
- `build.sh`: 构建脚本

## 函数接口

解密函数的签名如下：

```cpp
int DecryptPassword(const char* cipherText, const size_t cipherTextLen, char* plainText, const int32_t plainTextLen);
```

参数说明：

- `cipherText`: 加密的文本（输入）
- `cipherTextLen`: 加密文本的长度（输入）
- `plainText`: 解密文本缓冲区（输出）
- `plainTextLen`: 解密缓冲区大小（输入）

返回值：

- 0: 成功
- 非0: 失败

## 构建说明

1. 确保已安装CMake和编译器
2. 运行构建脚本：
   ```bash
   chmod +x build.sh
   ./build.sh
   ```
3. 构建完成后，将在当前目录生成`libdecrypt.so`文件

## 自定义实现

您可以根据实际需求修改`decrypt_example.cpp`文件中的解密逻辑，例如：

1. 实现真正的密码算法（如AES）
2. 从外部源获取解密密钥
3. 集成硬件安全模块（HSM）
