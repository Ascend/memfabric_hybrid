# config_store_etcd_test

## 功能说明

`config_store_etcd_test` 用于本地验证 `config_store` 的自动选举、多进程 rendezvous 以及多集群隔离能力。样例保持现有 `smem_bm_init -> smem_bm_create -> smem_bm_join` 流程，但重点是验证控制面收敛，不验证数据读写。样例支持两类 store URL：

- `tcp://HOST:PORT`：本地调试 / 手工指定 server
- `etcd://HOST:PORT` 或 `etcd://HOST:PORT#clusterId`：通过 etcd 做 server 选举；带 `#clusterId` 时隔离到独立集群

样例支持三种 `opType`：

- `0`：`SDMA`
- `1`：`DEVICE_RDMA`
- `2`：`HOST_TCP`

其中 `HOST_TCP` 是当前机器推荐的本地 smoke 路径，可在 `XPU_TYPE=NONE` 下运行。

## 编译

### 依赖

- C++17 或以上
- CMake 3.16+
- `libhcom.so`
- `etcd` 与 `libetcd_client_v3.so` 仅在 `etcd://...` 模式需要

### 当前机器推荐编译命令

```bash
set -e
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TEST=ON -DBUILD_ETCD_BACKEND=ON -DXPU_TYPE=NONE -DBUILD_HCOM=ON -DBUILD_WITH_RDMA=OFF -S . -B ./cmake-build-debug-config-store
cmake --build ./cmake-build-debug-config-store --target config_store_etcd_test -j
```

生成的可执行文件路径：

```bash
./cmake-build-debug-config-store/example/config_store/config_store_etcd_test
```

说明：

- `HOST_TCP` 不依赖 `ASCEND_HOME_PATH` 或 `npu-smi`
- 若需要 `etcd://...` 模式，构建命令必须带 `-DBUILD_ETCD_BACKEND=ON`
- `etcd client` 动态库默认构建到 `./output/etcd/lib64/libetcd_client_v3.so`

## 使用方法

### 命令格式

```bash
config_store_etcd_test <ipPort> <opType> <isA3> <hcomPort>
```

### 参数说明

| 参数 | 类型 | 说明 |
|---|---|---|
| `ipPort` | 字符串 | store 地址，支持 `tcp://HOST:PORT`、`etcd://HOST:PORT` 或 `etcd://HOST:PORT#clusterId` |
| `opType` | 整数 | `0=SDMA`，`1=DEVICE_RDMA`，`2=HOST_TCP` |
| `isA3` | 整数 | `0` 或 `1`；仅 `SDMA/DEVICE_RDMA` 模式生效，`HOST_TCP` 下会被忽略 |
| `hcomPort` | 整数 | 本地 HCOM 端口，样例统一拼接为 `tcp://127.0.0.1/0:<hcomPort>` |

附加说明：

- `worldSize` 固定为 `16`
- HCOM URL 格式为 `tcp://127.0.0.1/0:<hcomPort>`
- `16` 表示该样例固定的最大 rank 容量，不要求必须启动满 `16` 个进程

### 退出方式

程序就绪后会阻塞在标准输入，输入以下任意命令退出：

```text
exit
e
q
```

若标准输入为 EOF，样例在 join 完成后也会正常退出。

## 模式说明

### `HOST_TCP`

- 当前机器推荐路径
- 不需要 NPU / CANN
- `smem_bm_create` 使用 `128MB DRAM + 0 HBM`
- `isA3` 参数不参与行为
- 推荐用于本地 `config_store` / 多集群 smoke 验证

### `SDMA` / `DEVICE_RDMA`

- 继续沿用原有 NPU 探测逻辑
- `isA3=1` 时会做 `deviceId x 2`
- 需要可用的 NPU / CANN 运行环境

## 当前机器运行示例

### 1. 本地 `tcp://` + `HOST_TCP`

先准备环境变量：

```bash
export HCOM_MAX_SLICE_SIZE=32768
export HCOM_RECV_DATA_SIZE=32768
export HYBM_RDMA_SWAP_SPACE_SIZE=32768
```

在多个终端执行相同命令即可；实际参与进程数按本地验证需要决定，但不要超过 `16`：

```bash
./cmake-build-debug-config-store/example/config_store/config_store_etcd_test tcp://127.0.0.1:8573 2 0 10003
```

### 2. 本地 `etcd://` + `HOST_TCP`

先启动本机 etcd：

```bash
etcd --name config-store-local --data-dir /tmp/config-store-etcd --listen-client-urls http://127.0.0.1:2379 --advertise-client-urls http://127.0.0.1:2379 --listen-peer-urls http://127.0.0.1:2380 --initial-advertise-peer-urls http://127.0.0.1:2380 --initial-cluster config-store-local=http://127.0.0.1:2380
```

默认按上文构建出的 build-tree 可执行文件运行时，已带 `RUNPATH` 指向 `./output/etcd/lib64`。若你换了运行目录、做了单独安装，或运行时仍提示找不到 `libetcd_client_v3.so`，再导出：

```bash
export LD_LIBRARY_PATH="$(pwd)/output/etcd/lib64:$LD_LIBRARY_PATH"
```

再在多个终端执行相同命令；实际参与进程数按本地验证需要决定，但不要超过 `16`：

```bash
export HCOM_MAX_SLICE_SIZE=32768
export HCOM_RECV_DATA_SIZE=32768
export HYBM_RDMA_SWAP_SPACE_SIZE=32768
./cmake-build-debug-config-store/example/config_store/config_store_etcd_test etcd://127.0.0.1:2379 2 0 10003
```

若需要在同一 etcd 下顺序验证两个彼此隔离的集群，可分别使用不同的 `#clusterId`。每个集群按需启动多个进程即可：

```bash
./cmake-build-debug-config-store/example/config_store/config_store_etcd_test etcd://127.0.0.1:2379#cluster-a 2 0 10003
./cmake-build-debug-config-store/example/config_store/config_store_etcd_test etcd://127.0.0.1:2379#cluster-b 2 0 10013
```

说明：

- `#clusterId` 用于 etcd 多集群隔离
- `hcomPort` 用于本地 HCOM 通道隔离；若同机并行起多个集群，需为不同集群指定不同端口

## 通过判据

- 返回码为 `0`
- 日志包含 `All ranks joined, ready`
- 退出后日志包含 `Process exited normally`
