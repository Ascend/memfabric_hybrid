# MemFabric_Hybrid Etcd Store Backend 测试工具

本项目是一个用于测试 `SmemEtcdStoreBackend` 组件并发性能和功能的演示程序。它模拟了多线程环境下，从 Etcd 中批量读取（PrefixGet）配置数据的场景，用于验证后端存储接口在高并发下的稳定性和正确性。

---

## 项目概述

该程序主要执行以下逻辑：

1. **初始化**：连接指定的 Etcd 服务端。
2. **数据预埋**：写入一组模拟的分布式系统配置数据（如 Leader 信息、World Size 等）。
3. **并发测试**：启动多个工作线程，循环执行 `PrefixGet` 操作，读取指定前缀的配置。
4. **统计输出**：汇总并输出成功的读取次数。

---

## ️ 环境准备

**重要提示**：在运行本程序之前，请确保您的本地环境中**已经启动 Etcd 服务**。如果服务未启动，程序将因无法连接而报错。

---

## ️ 构建与依赖

### 依赖库

本项目依赖于以下核心库，请确保在构建环境中已安装：

- **smem**: 华为开源的共享内存/存储库（包含 `smem.h`）。
- **Etcd C++ Client**: 用于与 Etcd 服务通信。
- **CMake**: 用于项目构建管理。
- **C++17+ 编译器**: 代码使用了 `<atomic>`, `<thread>`, `<filesystem>` 等现代 C++ 特性。

### 编译步骤

使用 CMake 进行标准构建：

```
mkdir build
cd build
cmake ..
make -j
```

---

## 使用指南

### 命令格式

```
./etcd_backend_test [Etcd地址] [线程数量]
```

### 参数说明

| 参数位置 | 参数名 | 说明 | 默认值 | 必填 |
| ------ |------ |------ |------ |------ |
| 1 | `<etcd-address>` | Etcd 服务的连接地址 | `etcd://127.0.0.1:2379` | 否 |
| 2 | `<thread-count>` | 启动的工作线程数量 | `4` | 否 |

### 使用示例

1. **使用默认配置运行 (4线程)**：

```
./etcd_backend_test
```

2. **指定 Etcd 地址和线程数**：

```
./etcd_backend_test etcd://192.168.1.100:2379 8
```

### 预期输出

程序运行时会输出类似以下的日志：

```
=== SmemEtcdStoreBackend Multi-Thread PrefixGet Test ===
ETCD Address: etcd://127.0.0.1:2379
Thread Count: 4

[Step 1] Writing test data...
   PUT: /memfabric_hybrid/config_store/meta/leader = rank_0
   ...

[Step 2] Starting 4 threads for PrefixGet test...
main.cpp:45 [INFO] [Thread-0] SUCCESS! Found 2 items.
main.cpp:45 [INFO] [Thread-1] SUCCESS! Found 2 items.
...

=== Test Finished ===
Total successful PrefixGet operations: 20
```

---