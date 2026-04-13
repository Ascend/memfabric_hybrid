# UB Transport
UB Transport源代码路径为Mooncake/mooncake-transfer-engine/src/transport/ub_transport。

## 概述
UB Transport是一个基于 [MemFabric](https://gitcode.com/Ascend/memfabric_hybrid) 的NPU数据传输库，
得益于MemFabric强大的跨节点传输能力，支持RH2D、D2RH等OneCopy跨机跨介质数据直接访问能力。

### 软件硬件配套说明
由于UB Transport以 [MemFabric](https://gitcode.com/Ascend/memfabric_hybrid) 作为池化底座，运行需要配套安装MemFabric，
详见 [MemFabric安装部署](https://gitcode.com/Ascend/memfabric_hybrid/blob/master/doc/installation.md)

### 环境变量配置
UB Transport支持通过环境变量设置内部参数

#### 必选配置

| 环境变量         | 说明                                                                                                                              |
|--------------|---------------------------------------------------------------------------------------------------------------------------------|
| MF_STORE_URL | 设置config store url，eg: **export MF_STORE_URL=tcp://127.0.0.1:8570** or etcd mode: **export MF_STORE_URL=etcd://127.0.0.1:8570** |
| MF_DRAM_SIZE | 设置当前节点贡献的DRAM池大小,单位byte（默认等于1GB，0GB <= MF_DRAM_SIZE <= 1TB，内部按照1GB大小对齐），eg: **export MF_DRAM_SIZE=1073741824**                  |
| MF_HBM_SIZE  | 设置当前节点贡献的HBM池大小, 单位byte（默认等于1GB，0GB <= MF_HBM_SIZE <= 1TB，内部按照1GB大小对齐），eg: **export MF_HBM_SIZE=1073741824**                    |
| MF_OP_TYPE   | 设置memfabric传输协议（支持device_rdma、device_sdma，如果不设置MF_RANK_TAG_OP_INFO选项，该值将会成为同tag节点间的传输协议）, eg: **export MF_OP_TYPE=device_rdma** |

#### 可选配置

| 环境变量                |                                                                                                                                                                                        |
|---------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| MF_LOG_LEVEL        | 设置日志级别：0 ~ 4 （debug、info、warning、error、fatal）,eg: **export MF_LOG_LEVEL=1**                                                                                                            |
| MF_MAX_DRAM_SIZE    | 设置集群最大贡献的DRAM池大小（默认等于MF_DRAM_SIZE，如果为分离部署场景则需要设置， 1GB <= MF_DRAM_SIZE <= 1TB，内部按照1GB大小对齐， 需保证所有节点对该值的设置一致）,eg: export MF_MAX_DRAM_SIZE=1073741824                                      |
| MF_MAX_HBM_SIZE     | 设置集群最大贡献的HBM池大小（默认等于MF_HBM_SIZE，如果为分离部署场景则需要设置， 1GB <= MF_HBM_SIZE <= 1TB，内部按照1GB大小对齐， 需保证所有节点对该值的设置一致）,eg: export MF_MAX_HBM_SIZE=1073741824                                          |
| MF_RANK_TAG         | 设置节点tag（默认为空，用于标识当前节点，配合tagOpInfo可以在不同的tag节点构建不同的传输网络，支持字符`(a~zA~z下划线)`), eg: **export MF_RANK_TAG=Tag_A2**                                                                            |
| MF_RANK_TAG_OP_INFO | 设置用户本地介质（默认为空，tag间传输方式说明，应符合tag:dataOpType:tag的形式，dataOpType可选：DEVICE_SDMA, DEVICE_RDMA, HOST_RDMA, HOST_TCP, HOST_URMA）, eg: **export MF_RANK_TAG_OP_INFO=Tag_A2:DEVICE_RDMA:Tag_A3** |
| MF_PD_OP_TYPE       | 设置PD传输协议（支持device_rdma、device_sdma）, eg: **export MF_PD_OP_TYPE=device_rdma**                                                                                                          |
| MF_SMEM_TRANS_ROLE  | 设置PD传输角色（支持Prefill、Decode）,eg: **export MF_SMEM_TRANS_ROLE=Prefill**                                                                                                                   |

### 注意事项（必看）
1.由于 memfabric 初始化时依赖config_store（memfabric内部的元数据交换服务）来进行各节点的元数据同步，所以启动时需要保证MF_STORE_URL的有效性
需要保证 port及prot+1 端口可用（分别用于支持PD传输和内存池化功能）

2.在memfabric中PD传输和内存池化是两个不同功能，接入mooncake后分别通过mooncake_transfer和mooncake_store使能。
即如果需要使用PD传输能力，通过mooncake_transfer接口使用，内存池化功能通过mooncake_store接口使用。

### 编译说明
在成功安装所有依赖后，**设置编译参数-DUSE_UB=ON**，正常编译Mooncake即可。
```cpp
mkdir -p build
cd build
cmake .. -DUSE_UB=ON
make -j
make install
```
### 使用说明
就使用上来说，UB Transport无缝兼容原版mooncake接口，在**初始化时指定协议使用 'ub' 即可**

```python
from mooncake.store import MooncakeDistributedStore
store = MooncakeDistributedStore()
# 无需指定池化内存大小，由环境变量设置
store.setup('127.0.0.1', 'P2PHANDSHAKE', 0, 0, 'ub', '', '127.0.0.1:50051')
# Register
ret = transfer_engine_.batch_register_memory(addrs, sizes)
# D2RH
ret = store.batch_put_from_multi_buffers(keys, addrs, sizes, True)
# RH2D
ret = store.batch_get_into_multi_buffers(keys, addrs, sizes, True)
```
### PD传输使用说明
UB Transport的PD传输能力由mooncake_transfer使能，所以需要通过TransferEngine接口调用

```python
from mooncake.engine import TransferEngine
transfer_engine_ = TransferEngine()
ret = transfer_engine_.initialize('127.0.0.1', "P2PHANDSHAKE", "ub", '')
target_name = '127.0.0.1' + ":" + str(transfer_engine_.get_rpc_port())
# Register
ret = transfer_engine_.batch_register_memory(addrs, sizes)
# WRITE
ret = transfer_engine_.batch_transfer_sync_write(target_name, local_addrs, remote_addrs, sizes)
# READ
ret = transfer_engine_.batch_transfer_sync_read(target_name, local_addrs, remote_addrs, sizes)
```
