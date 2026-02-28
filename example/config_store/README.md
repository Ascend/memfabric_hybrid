# config_store_etcd_test

## 功能说明

`config_store_etcd_test` 是基于华为 **MemFabric_Hybrid** 框架的 **config_store 测试工具**
。主要测试config_store的自动选举和故障恢复能力，验证其在多节点环境下的稳定性和可靠性。

config_store 是集群节点间共享配置/元数据的存储组件。本工具验证其在两种 server 选举模式下的初始化流程与多节点 rendezvous
能力：

| 模式            | 说明                                            |
|---------------|-----------------------------------------------|
| **etcd 自动选举** | 通过 etcd 集群自动选出 config_store server，适合生产/多节点场景 |
| **TCP 手动指定**  | 显式指定某一进程作为 server，适合本地调试与点对点验证                |

### 工作流程

1. **设备自动探测**：通过 `npu-smi` 查询当前节点空闲的 NPU 设备，支持 A3 架构物理 die 编号映射（device ID × 2）。
2. **config_store 初始化**：根据传入的 URL 模式（`etcd://` 或 `tcp://`）完成 server 选举与多节点 rendezvous。
3. **就绪等待**：所有 rank 握手完成后进入等待状态，保持资源存活，供外部脚本协调测试。
4. **资源清理**：收到退出指令后销毁 handle 并释放所有资源。

---

## 编译

### 依赖

- C++17 或以上
- 昇腾 CANN 工具链（提供 `acl.h`、`smem.h`、`smem_bm.h`）
- CMake 3.16+
- etcd（仅 etcd 模式需要，确保 etcd 服务可访问）
- libetcd_client_v3.so（参考相关文档编译，然后export LD_LIBRARY_PATH=youpath:$LD_LIBRARY_PATH）

### 编译步骤

在当前目录执行如下命令即可

  ```bash
  mkdir build
  cmake . -B build
  make -C build
  ```

或打包安装时同源码一起编译

  ```bash
bash script/build_and_pack_run.sh --build_mode RELEASE --build_python ON --xpu_type NPU --build_test ON
  ```

---

## 使用方法

### 命令格式

```
config_store_etcd_test <rankSize> <ipPort> <opType> <isA3>
```

### 参数说明

| 参数         | 类型  | 说明                                               |
|------------|-----|--------------------------------------------------|
| `rankSize` | 整数  | 参与通信的总 rank 数                                    |
| `ipPort`   | 字符串 | server 地址，`etcd://HOST:PORT` 或 `tcp://HOST:PORT` |
| `opType`   | 整数  | 传输模式：`0` = SDMA，`1` = DEVICE_RDMA                |
| `isA3`     | 整数  | 是否为 A3 架构：`0` = 否，`1` = 是（device ID 自动 ×2）       |

### 退出方式

程序就绪后进入等待状态，在标准输入键入以下任意命令退出：

```
exit
e
q
```

---

## 两种模式说明

### 模式一：etcd 自动选举 server

所有节点使用相同的 etcd 地址启动，etcd 负责从参与节点中自动选举出 config_store server。

```
节点 A ──┐
节点 B ──┼──► etcd://192.168.1.100:12335 ──► 自动选举 server ──► 全部就绪
节点 C ──┘
```

- 各节点启动顺序无严格要求，etcd 保证选举一致性。
- etcd 服务必须在所有节点启动前可访问。

### 模式二：TCP 手动指定 server

显式指定某一节点的 IP 作为 server，其余节点作为 client 连接该地址。

```
节点 A (server) ◄──┐
节点 B         ────┼──► tcp://192.168.1.100:12335 ──► 全部就绪
节点 C         ────┘
```

- 所有节点填写**同一个** IP（即指定 server 的节点 IP）。
- server 节点须先于或同时于其他节点启动。

---

## 常用命令

### etcd 模式：4 节点，A3 架构，SDMA

```bash
# 每个节点上分别执行（相同命令）
./config_store_etcd_test 4 etcd://192.168.1.100:12335 0 1
```

### TCP 模式：4 节点，A3 架构，SDMA，指定 server IP

```bash
# 每个节点上分别执行（IP 填 server 节点地址）
./config_store_etcd_test 4 tcp://192.168.1.100:12335 0 1
```
