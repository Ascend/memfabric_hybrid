# Transfer 性能测试

这是一个类似于C++版本(benchmark/transfer/perf_cpp/transfer_perf.cpp)的Transfer性能测试Python实现。

## 概述

该脚本测试：
- 不同块大小的延迟
- 批量传输的带宽
- 并发传输性能

## 前置条件

运行脚本之前，请确保您具备：
- 已安装MemFabric Hybrid Python包
- 正确的硬件配置（Ascend NPU设备）

## 用法

此脚本需要同时启动两个测试程序：

### 直接启动Python程序

rank 0（发送方/Prefil端）：

```bash
python transfer_performance.py --rank-id 0 --store-url tcp://127.0.0.1:12050 --num-threads 2 --data-op-type sdma --npu-id 0
```

rank 1（接收方/Decode端）：

```bash
python transfer_performance.py --rank-id 1 --store-url tcp://127.0.0.1:12050 --num-threads 2 --data-op-type sdma --npu-id 1
```

参数说明：

| 参数名              | 必选 | 说明                                                 |
|---------------------|------|----------------------------------------------------|
| rank-id             | 是  | 当前节点的rankId                                        |
| store-url           | 是  | 配置存储服务地址，格式：`tcp://ip:port` 或者 `tcp://[ipv6]:port`。configStore的server的监听ip和端口。关于 configStore 配置存储系统的说明，请参考 [config_store_cluster_ha](../../../doc/config_store_cluster_ha.md) |
| num-threads         | 是  | 并发线程数（默认：2）                                    |
| data-op-type        | 是  | 数据操作类型：sdma或rdma（默认：sdma）                    |
| npu-id              | 是  | NPU设备ID（默认：0）                                    |

### 使用自动化脚本 run_transfer_benchmark.sh

为了方便运行完整的性能测试，我们提供了 `run_transfer_benchmark.sh` 自动化脚本，它可以自动启动两个rank的进程。

使用示例：

```bash
# 使用默认参数运行
./run_transfer_benchmark.sh

# 指定自定义参数
./run_transfer_benchmark.sh --store-url tcp://192.168.1.100:12050 --num-threads 4 --data-op-type rdma --npu-id-0 0 --npu-id-1 7
```

参数说明：

| 参数名              | 必选 | 说明                                                 |
|---------------------|------|----------------------------------------------------|
| store-url           | 是  | 配置存储服务地址，格式：`tcp://ip:port` 或者 `tcp://[ipv6]:port`。configStore的server的监听ip和端口。关于 configStore 配置存储系统的说明，请参考 [config_store_cluster_ha](../../../doc/config_store_cluster_ha.md) |
| num-threads         | 是  | 并发线程数（默认：2）                                    |
| data-op-type        | 是  | 数据操作类型：sdma或rdma（默认：sdma）                    |
| npu-id-0            | 是  | NPU设备ID（默认：0）                                    |
| npu-id-1            | 是  | NPU设备ID（默认：7）                                    |

## 功能

- 块大小从32KB到32MB的迭代
- 预热阶段以确保稳定的测量结果
- 多线程的并发传输测试
- 批量传输性能测量
- 吞吐量以GB/s计算

## 输出

脚本将显示：
- 预热完成状态
- 每次测试的延迟测量
- 聚合带宽结果
- 不同块大小的性能指标

## 示例输出

A3 pod内SDMA传输验证，Device: 0->1 传输性能

```
==================================================Trans Test Start==================================================
Test completed: latency 68.03us, block size 32KB, total threads=2, per-thread times=100, aggregated throughput 14.31 GB/s
Test completed: latency 66.97us, block size 64KB, total threads=2, per-thread times=100, aggregated throughput 30.88 GB/s
Test completed: latency 67.85us, block size 128KB, total threads=2, per-thread times=100, aggregated throughput 58.19 GB/s
Test completed: latency 66.12us, block size 256KB, total threads=2, per-thread times=100, aggregated throughput 87.69 GB/s
Test completed: latency 66.33us, block size 512KB, total threads=2, per-thread times=100, aggregated throughput 116.48 GB/s
Test completed: latency 62.75us, block size 1024KB, total threads=2, per-thread times=100, aggregated throughput 139.01 GB/s
Test completed: latency 62.79us, block size 2048KB, total threads=2, per-thread times=100, aggregated throughput 161.57 GB/s
Test completed: latency 67.13us, block size 4096KB, total threads=2, per-thread times=100, aggregated throughput 174.73 GB/s
Test completed: latency 94.17us, block size 8192KB, total threads=2, per-thread times=100, aggregated throughput 178.97 GB/s
Test completed: latency 145.72us, block size 16384KB, total threads=2, per-thread times=100, aggregated throughput 183.52 GB/s
==================================================Test End==================================================
```

A3 pod内SDMA传输验证，Device: 0->2 传输性能

```
==================================================Trans Test Start==================================================
Test completed: latency 68.29us, block size 32KB, total threads=2, per-thread times=100, aggregated throughput 14.58 GB/s
Test completed: latency 69.74us, block size 64KB, total threads=2, per-thread times=100, aggregated throughput 27.82 GB/s
Test completed: latency 70.64us, block size 128KB, total threads=2, per-thread times=100, aggregated throughput 56.11 GB/s
Test completed: latency 67.02us, block size 256KB, total threads=2, per-thread times=100, aggregated throughput 73.52 GB/s
Test completed: latency 70.17us, block size 512KB, total threads=2, per-thread times=100, aggregated throughput 90.83 GB/s
Test completed: latency 66.14us, block size 1024KB, total threads=2, per-thread times=100, aggregated throughput 114.41 GB/s
Test completed: latency 67.88us, block size 2048KB, total threads=2, per-thread times=100, aggregated throughput 123.92 GB/s
Test completed: latency 70.29us, block size 4096KB, total threads=2, per-thread times=100, aggregated throughput 132.92 GB/s
Test completed: latency 123.81us, block size 8192KB, total threads=2, per-thread times=100, aggregated throughput 136.93 GB/s
Test completed: latency 177.56us, block size 16384KB, total threads=2, per-thread times=100, aggregated throughput 137.52 GB/s
==================================================Test End==================================================
```

## 故障排除

- 验证NPU是否正确配置且可访问
- 检查MemFabric Hybrid Python包是否正确安装
- 确认存储URL中的端口可用且未被防火墙阻止