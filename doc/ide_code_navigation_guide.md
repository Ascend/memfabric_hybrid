# IDE 代码跳转配置指南

## 问题背景

项目编译通过，但 IDE 无法正确跳转到定义/声明。这是因为 IDE 的 IntelliSense 需要 `compile_commands.json` 文件来获取编译选项（include 路径、宏定义等）。

## 解决方案

### 步骤 1：修改构建脚本（可选但推荐）

在 `script/build.sh` 中添加 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` 选项，让 CMake 自动生成 `compile_commands.json`。

找到以下两处 cmake 调用，添加该选项：

**位置 1**（约第 159 行）：
```bash
cmake \
    -G "$GENERATOR"  \
    -DCMAKE_BUILD_TYPE="${BUILD_MODE}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \    # 添加这一行
    -DBUILD_UT="${BUILD_UT}" \
    ...
```

**位置 2**（约第 325 行）：
```bash
cmake -G "$GENERATOR" -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -S . -B build/
```

> **注意**：如果无法修改 `build.sh`，可以在编译时手动添加该选项，或者直接使用方法 2。

### 步骤 2：编译项目

使用正常的编译命令编译项目：

```bash
bash script/build_and_pack_run.sh --build_mode RELEASE --build_python ON --xpu_type NPU --build_test OFF --build_hcom OFF
```

编译完成后，`build/` 目录下会生成 `compile_commands.json` 文件。

### 步骤 3：创建符号链接

在项目根目录下执行：

```bash
ln -s build/compile_commands.json compile_commands.json
```

这样 IDE 就能在项目根目录找到 `compile_commands.json` 文件。

### 步骤 4：重启 IDE

重新加载 IDE 窗口：

- **VS Code**: `Ctrl+Shift+P` → 输入 "Reload Window" → 回车
- **其他 IDE**: 重启 IDE

## 验证

重启后，尝试跳转到定义（通常 `F12` 或 `Ctrl+点击`），应该能正确跳转了。

## 注意事项

1. **重新编译后无需重新创建符号链接**：符号链接会自动指向新生成的 `compile_commands.json`

2. **如果仍然无法跳转**：
   - 确认 `compile_commands.json` 文件存在且内容不为空
   - 检查 VS Code 是否安装了 C/C++ 扩展
   - 尝试在 VS Code 中手动指定配置文件路径（见下方）

3. **手动指定配置文件路径**（可选）：

   创建 `.vscode/c_cpp_properties.json`：
   ```json
   {
       "configurations": [
           {
               "name": "Linux",
               "compileCommands": "${workspaceFolder}/build/compile_commands.json"
           }
       ]
   }
   ```

## 原理说明

- **编译通过**：编译器直接使用 CMakeLists.txt 中的配置
- **IDE 无法跳转**：IDE 的 IntelliSense 需要 `compile_commands.json` 来知道每个文件的编译选项

`compile_commands.json` 是 CMake 通过 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` 选项生成的 JSON 文件，包含了项目中每个源文件的完整编译命令。
