# mf_adapter数据传输工具示例

## 简介

本工具基于 `mf_adapter` 库实现了两个节点（Prefill 节点与 Decode 节点）之间的高效数据传输功能，适用于基于昇腾 NPU 的分布式计算场景。通过内存注册与直接内存访问，实现了跨节点数据的快速传输，避免了传统 IO 操作的性能开销。

## 功能说明

工具包含两种工作角色：

*   **Prefill 角色**：负责生成数据并将数据传输到目标节点

*   **Decode 角色**：负责接收并处理 Prefill 节点传输的数据

两者通过配置存储服务（store\_url）进行协调，通过 RPC 端口进行通信，支持大规模张量数据（torch.Tensor）的高效传输。


## 使用方法

### 基本命令格式



```
python test\_transfer\_engine.py \[参数]
```

### 参数说明



| 参数名                  | 必选 | 说明                                                   |
| -------------------- | -- | ---------------------------------------------------- |
| --role               | 是  | 工作角色，可选值：`Prefill` 或 `Decode`                        |
| --src-session-id     | 否  | 当前节点的会话 ID，格式：`ip:port`                              |
| --store-url          | 是  | 配置存储服务地址，格式：`tcp://ip:port`                          |
| --npu-id             | 否  | NPU 设备 ID，默认值：0                                      |
| --dst-session-id     | 否  | 目标节点会话 ID（仅 Prefill 角色需要），格式：`ip:port`               |
| --log-level          | 否  | 日志级别：0 (debug)、1 (info)、2 (warn)、3 (error)，默认值：0     |

### 运行步骤

#### 1. 启动 Decode 节点


```
python test_transfer_engine.py \
    --role Decode \
    --src-session-id "192.168.1.100:50051" \
    --store-url tcp://192.168.1.100:8000 \
    --npu-id 0 \
    --log-level 1
```

#### 2. 启动 Prefill 节点


```
python test_transfer_engine.py \
    --role Prefill \
    --src-session-id "192.168.1.100:50052" \
    --store-url tcp://192.168.1.1000:8000 \
    --npu-id 1 \
    --dst-session-id "192.168.1.100:50051" \
    --log-level 1
```

## 工作流程说明


1.  **初始化阶段**：

*   两个节点分别启动并通过`engine.initialize()`初始化 TransferEngine

*   Decode 节点与 Prefill 节点通过`store_url`进行配置同步，两个节点必须使用相同的`store_url`配置以确保能够相互发现

*   双方通过各自的`src-session-id`和目标节点的`dst-session-id`建立通信通道，Prefill 节点的`dst-session-id`必须与 Decode 节点的`src-session-id`完全一致（包括端口号）

2.  **数据传输阶段**：

*   Decode 节点创建接收缓冲区（大小为`(10, 50, 40, 20, 60)`的 float16 张量）并注册内存地址

*   Prefill 节点生成随机张量数据，注册内存后通过`transfer_sync_write`方法将数据传输到 Decode 节点

*   数据通过直接内存访问方式传输，避免数据拷贝

## 故障排查

1.  **初始化失败**：

*   检查 store\_url 是否可访问（可使用`telnet ip port`验证端口连通性），需使用所有节点均可访问的 TCP 地址，建议使用固定端口（如`tcp://``192.168.1.1:23456`）

*   确认`src-session-id`格式正确且未被占用

*   验证 NPU 设备是否正常工作（可通过`npu-smi info`命令检查）

2.  **数据传输失败**：

*   检查 Prefill 节点的`dst-session-id`是否与 Decode 节点的`src-session-id`完全一致

*   确认两个节点的内存注册成功（日志中包含`register success`）

*   查看 debug 级日志（`--log-level 0`）获取详细错误信息
