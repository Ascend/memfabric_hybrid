# 数据传输性能工具示例

## 简介

本工具主要用于验证底层的传输性能，可以分别配置SDMA和RDMA。

## 使用方法

### 基本命令格式

```
# transfer_perf {rankSize} {rankId} {deviceId} {useSdma} {testBm} tcp://{Ip}:{port}
或者
# transfer_perf {rankSize} {rankId} {deviceId} {useSdma} {testBm} tcp6://{[Ip]}:{port}

# deviceId=2
./transfer_perf 2 0 2 1 1 tcp://127.0.0.1:12050
(./transfer_perf 2 0 2 1 1 tcp6://[::1]:12050)

# deviceId=2
./transfer_perf 2 1 3 1 1 tcp://127.0.0.1:12050
(./transfer_perf 2 1 3 1 1 tcp6://[::1]:12050)
```

### 参数说明



| 参数名                  | 必选 | 说明                                                   |
| -------------------- | -- | ---------------------------------------------------- |
| rankSize               | 是  | 一共多少个rank                        |
| rankId     | 是  | 当前节点的rankId                              |
| deviceId          | 是  | 当前节点的deviceId                      |
| useSdma             | 是  | 1使用SDMA，0使用RDMA                                      |
| testBm     | 是  | 1测试BigMemory场景，0测试PD传输场景               |
| tcp://{Ip}:{port}          | 是  | 配置存储服务地址，格式：`tcp://ip:port` 或者 `tcp6://[ip]:port`     |

### 运行步骤

修改run.sh中的参数后，可以通过直接运行run.sh获取性能
```
bash run.sh
```

### 实验结果（A3 pod内SDMA传输验证）

#### Device: 0->1 传输性能

```
Test completed: latency 12.5us, block size 32KB, total size 1024KB , throughput 9.70 GB/s
Test completed: latency 12.1us, block size 64KB, total size 2048KB , throughput 19.33 GB/s
Test completed: latency 10.2us, block size 128KB, total size 4096KB , throughput 39.98 GB/s
Test completed: latency 11.3us, block size 256KB, total size 8192KB , throughput 76.26 GB/s
Test completed: latency 13.6us, block size 512KB, total size 16384KB , throughput 151.83 GB/s
Test completed: latency 15.9us, block size 1024KB, total size 32768KB , throughput 179.72 GB/s
Test completed: latency 23.7us, block size 2048KB, total size 65536KB , throughput 179.87 GB/s
Test completed: latency 35.5us, block size 4096KB, total size 131072KB , throughput 184.24 GB/s
Test completed: latency 57.8us, block size 8192KB, total size 262144KB , throughput 183.11 GB/s
Test completed: latency 102.5us, block size 16384KB, total size 524288KB , throughput 186.61 GB/s
```

#### Device: 0->2 传输性能

```
Test completed: latency 15.5us, block size 32KB, total size 1024KB , throughput 9.79 GB/s
Test completed: latency 15.1us, block size 64KB, total size 2048KB , throughput 18.96 GB/s
Test completed: latency 15.6us, block size 128KB, total size 4096KB , throughput 37.72 GB/s
Test completed: latency 15.9us, block size 256KB, total size 8192KB , throughput 71.15 GB/s
Test completed: latency 16.3us, block size 512KB, total size 16384KB , throughput 93.10 GB/s
Test completed: latency 22.5us, block size 1024KB, total size 32768KB , throughput 110.23 GB/s
Test completed: latency 29.5us, block size 2048KB, total size 65536KB , throughput 122.04 GB/s
Test completed: latency 45.8us, block size 4096KB, total size 131072KB , throughput 118.61 GB/s
Test completed: latency 77.2us, block size 8192KB, total size 262144KB , throughput 130.72 GB/s
Test completed: latency 139us, block size 16384KB, total size 524288KB , throughput 133.87 GB/s
```
