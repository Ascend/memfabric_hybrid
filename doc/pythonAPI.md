# Python接口
使用Python接口前需要安装memfabric_hybrid的whl包，有两种安装方式，可参考[安装指南](./installation.md)
whl包安装完成后，即可在python中通过**import memfabric_hybrid**导入memfabric的python包，然后调用python接口

python接口为c接口的封装，功能一致，具体介绍可以在python中使用help函数获取，参考如下
```python
import memfabric_hybrid as mf  #导入memfabric_hybrid
help(mf)   #查看memfabric_hybrid基础函数介绍
help(mf.bm)    #查看big memory接口介绍
help(mf.shm)   #查看share memory接口介绍
```

[TOC]

## 公共接口
### 1. 初始化/退出函数
#### initialize
初始化运行环境
```python
def initialize(flags = 0) -> int
```

|参数/返回值|含义|
|-|-|
|flags|int类型，预留参数|
|返回值|成功返回0，其他为错误码|

退出运行环境
#### uninitialize
```python
def uninitialize()
```

### 2. 创建config store对象
#### create_config_store
创建config store对象
```python
def create_config_store(store_url: str) -> int
```

|参数/返回值|含义|
|-|-|
|store_url|业务面地址，格式支持 `tcp://ip:port`、`etcd://ip:port`、`etcd://ip:port#instanceId`（etcd 多集群隔离）|
|返回值|成功返回0，其他为错误码|

### 3. 日志设置
#### set_log_level
设置日志打印级别
```python
def set_log_level(level)
```

|参数/返回值|含义|
|-|-|
|level|int类型，日志级别，0-debug 1-info 2-warn 3-error|
|返回值|成功返回0，其他为错误码|

#### set_extern_logger
设置自定义日志函数
```python
def set_extern_logger(log_fn:Callable[[int, str], None]) -> int
```

|参数/返回值|含义|
|-|-|
|log_fn|函数指针|
|level|日志级别，0-debug 1-info 2-warn 3-error|
|message|日志内容|
|返回值|成功返回0，其他为错误码|

### 4. 安全证书设置
#### set_conf_store_tls_key
注册Python解密处理程序
```python
def set_conf_store_tls_key(tls_pk, tls_pk_pw, py_decrypt_func:Callable[[str], str]) -> int
```

|参数/返回值|含义|
|-|-|
|tls_pk|私钥|
|tls_pk_pw|私钥口令|
|py_decrypt_func|口令解密函数|
|返回值|成功返回0，其他为错误码|

#### set_conf_store_tls
设置配置存储的TLS信息
```python
def set_conf_store_tls(enable, tls_info) -> int
```

|参数/返回值|含义|
|-|-|
|enable(boolean)|是否启用配置存储的TLS|
|tls_info(str)|TLS配置字符串|
|返回值|成功时返回零,出错时返回非零值|

### 5. 错误信息获取/清理
#### get_last_err_msg
获取最后一条错误信息
```python
def get_last_err_msg() -> str
```

|参数/返回值|含义|
|-|-|
|返回值|错误信息|

#### get_and_clear_last_err_msg
获取最后一条错误信息并清空所有错误信息
```python
def get_and_clear_last_err_msg() -> str
```

|参数/返回值|含义|
|-|-|
|返回值|错误信息|

## BM接口
### 1. BM初始化/退出
#### initialize
初始化运行环境
```python
def initialize(store_url, world_size, device_id, config) -> int
```

|参数/返回值|含义|
|-|-|
|store_url|config store地址，格式支持 `tcp://ip:port`、`etcd://ip:port`、`etcd://ip:port#instanceId`（etcd 多集群隔离）|
|world_size|参与SMEM初始化rank数量，最大支持1024|
|device_id|当前rank的device id|
|config|初始化SMEM配置|
|返回值|成功返回0，其他为错误码|

#### uninitialize
退出运行环境
```python
def uninitialize(flags = 0) -> None
```

|参数/返回值|含义|
|-|-|
|flags|int类型，预留参数|

### 2. 创建BM
#### create
创建BM
```python
def create(id, local_dram_size, local_hbm_size = 0, data_op_type = SMEMB_DATA_OP_SDMA, flags = 0) -> BigMemory
```

|参数/返回值|含义|
|-|-|
|id|SMEM对象id，用户指定，与其他SMEM对象不重复，范围为[0, 63]|
|local_dram_size|本地dram内存大小|
|local_hbm_size|本地hbm内存大小|
|data_op_type|数据操作类型，参考smem_bm_data_op_type类型定义|
|flags|预留参数|
|返回值|BigMemory对象|

#### create2
创建BM（支持设置本地内存上限）
```python
def create2(id, local_dram_size, max_dram_size, local_hbm_size = 0, max_hbm_size = 0, data_op_type = SMEMB_DATA_OP_SDMA, enable_56bits_gva = False, flags = 0, shm_fd = -1) -> BigMemory
```

|参数/返回值|含义|
|-|-|
|id|SMEM对象id，用户指定，与其他SMEM对象不重复，范围为[0, 63]|
|local_dram_size|本地dram内存大小|
|max_dram_size|本地dram内存最大大小|
|local_hbm_size|本地hbm内存大小|
|max_hbm_size|本地hbm内存最大大小|
|data_op_type|数据操作类型，参考smem_bm_data_op_type类型定义|
|enable_56bits_gva|是否显式启用 56 位 GVA，bool 类型，默认 False。|
|flags|预留参数|
|shm_fd|共享内存文件描述符，默认-1（不使用）|
|返回值|BigMemory对象|

### 3. 获取当前rank的id
#### bm_rank_id
获取当前rank的id
```python
def bm_rank_id() -> int
```

|参数/返回值|含义|
|-|-|
|返回值|成功返回当前rank id，失败返回u32最大值|

### 4. 常用类型
#### BmCopyType枚举类
```python
class BmCopyType(Enum):
    L2G
    G2L
    G2H
    H2G
    L2GH
    GH2L
    GH2H
    H2GH
    G2G
    AUTO
```

|属性|含义|
|-|-|
|L2G|将数据从本地HBM复制到全局空间|
|G2L|将数据从全局空间复制到本地HBM|
|G2H|将数据从全局空间复制到主机内存|
|H2G|将数据从主机内存复制到全局空间|
|L2GH|将数据从本地HBM复制到全局主机空间|
|GH2L|将数据从全局主机空间复制到本地HBM|
|GH2H|将数据从全局主机空间复制到主机内存|
|H2GH|将数据从主机内存复制到全局主机空间|
|G2G|将数据从全局空间复制到全局空间|

#### BmMemType枚举类
```python
class BmMemType(Enum):
    LOCAL_DEVICE
    LOCAL_HOST
```

| 属性 | 含义 |
|-----|------|
|LOCAL_DEVICE|本地设备侧内存|
|LOCAL_HOST|本地主机侧内存|

#### BmConfig类
```python
class BmConfig:
    def __init__(self) -> None
```

|构造函数/属性|含义|
|-|-|
|构造函数|BM配置初始化|
|init_timeout属性|init函数的超时时间，默认120秒（最小值为1，最大值为SMEM_BM_TIMEOUT_MAX）|
|create_timeout属性|create函数的超时时间，默认120秒（最小值为1，最大值为SMEM_BM_TIMEOUT_MAX）|
|operation_timeout属性|控制操作超时，默认120秒（最小值为1，最大值为SMEM_BM_TIMEOUT_MAX）|
|start_store属性|是否启动配置库，默认为true|
|start_store_only属性|仅启动配置存储|
|dynamic_world_size属性|成员不能动态连接|
|unified_address_space属性|SVM统一地址|
|auto_ranking属性|自动分配排名ID，默认为true（由 smem_bm_config_init 设置）|
|rank_id属性|用户指定的RankId，对autoRanking有效为False|
|flags属性|预留参数|

#### BmGroupEvent枚举类
```python
class BmGroupEvent(Enum):
    JOIN_EVENT
    LEAVE_EVENT
```

| 属性 | 含义 |
|-----|------|
|JOIN_EVENT|有节点加入BM组|
|LEAVE_EVENT|有节点退出BM组|

#### BmDataOpType枚举类
```python
class BmDataOpType(Enum):
    SDMA
    HOST_RDMA
    HOST_URMA
    HOST_TCP
    DEVICE_RDMA
    HOST_SHM
```

#### BigMemory类
```python
class BigMemory:
    def join(flags = 0) -> int:
    def leave(flags = 0) -> int:
    def extend_local_mem(mem_type = SMEM_MEM_TYPE_HOST, size) -> int:
    def local_mem_size(mem_type = SMEM_MEM_TYPE_DEVICE) -> int:
    def peer_rank_ptr(peer_rank, mem_type = SMEM_MEM_TYPE_DEVICE) -> int:
    def gva_to_va(gva, mem_type = SMEM_MEM_TYPE_LOCAL_HOST) -> int:
    def destroy() -> None:
    def register(addr, size) -> int:
    def unregister(addr) -> int:
    def copy_data(src_ptr, dst_ptr, size, type, flags = 0) -> int:
    def copy_data_batch(src_addrs, dst_addrs, sizes, count, type, flags) -> int:
    def set_group_event_handler(cb) -> int:
    def get_rank_id_by_gva(gva) -> int:
    def copy_data_batch_partial_succeed(src_addrs, dst_addrs, sizes, count, type, flags, result) -> int:
    def wait() -> int:

```

| 属性/方法                        | 含义                                              |
|------------------------------|-------------------------------------------------|
| join方法                       | 加入BM                                            |
| join参数flags                  | 预置参数                                            |
| leave方法                      | 退出BM                                            |
| leave参数flags                 | 预置参数                                            |
| extend_local_mem方法        | 扩展本地内存空间                                        |
| extend_local_mem参数memType | 内存类型，支持 SMEM_MEM_TYPE_HOST、SMEM_MEM_TYPE_DEVICE |
| extend_local_mem参数size    | 扩展内存大小                                          |
| local_mem_size方法             | 获取创建BM本地贡献的空间大小                                 |
| local_mem_size参数mem_type     | 本地贡献空间的内存类型                                     |
| local_mem_size返回值            | 本地贡献空间大小，单位byte                                 |
| peer_rank_ptr方法              | 获取rank id对应的贡献空间在gva上的地址位置                      |
| peer_rank_ptr参数peer_rank     | 指定的rank id                                      |
| peer_rank_ptr参数mem_type      | 指定的rank id的贡献空间的内存类型                            |
| gva_to_va方法                  | 将GVA地址转换为当前进程可访问的VA地址                           |
| gva_to_va参数gva               | 待转换的GVA地址                                       |
| gva_to_va参数mem_type          | 内存类型(BmMemType)                                 |
| gva_to_va返回值                 | 转换后的VA地址，失败返回0                                  |
| destroy方法                    | 销毁BM                                            |
| register方法                   | 注册内存到BM                                         |
| register参数addr               | 注册地址的起始地址指针                                     |
| register参数size               | 注册地址的大小                                         |
| unregister方法                 | 从BM中注销内存                                        |
| unregister参数addr             | 注销地址的起始地址指针                                     |
| copy_data方法                  | 拷贝数据对象                                          |
| copy_data参数src_ptr(int)      | source gva of data                              |
| copy_data参数dst_ptr(int)      | destination gva of data                         |
| copy_data参数size(int)         | size of data to be copied                       |
| copy_data参数type(BmCopyType)  | copy type, L2G, G2L, G2H, H2G                   |
| copy_data参数flags(int)        | optional flags                                  |
| set_group_event_handler方法     | 注册组事件回调函数                                   |
| set_group_event_handler参数cb   | 回调函数，签名 cb(rank_id: int, event: BmGroupEvent) |
| get_rank_id_by_gva方法          | 根据GVA获取rank ID                                |
| get_rank_id_by_gva参数gva       | 全局虚拟地址                                       |
| copy_data_batch_partial_succeed方法 | 批量拷贝数据，允许部分失败                          |
| copy_data_batch_partial_succeed参数result | 出参，记录哪些操作成功/失败                  |
| wait方法                        | 等待异步操作完成                                    |

## SHM接口
### 1. 初始化/退出接口
#### initialize
初始化运行环境
```python
def initialize(store_url, world_size, rank_id, device_id, config) -> int
```

|参数/返回值|含义|
|-|-|
|store_url|config store地址，格式支持 `tcp://ip:port`、`etcd://ip:port`、`etcd://ip:port#instanceId`（etcd 多集群隔离）|
|world_size|参与SMEM初始化rank数量，最大支持1024|
|rank_id|当前rank id|
|device_id|当前rank的device id|
|config|初始化SMEM配置|
|返回值|成功返回0，其他为错误码|

#### uninitialize
退出运行环境
```python
def uninitialize(flags = 0) -> None
```

|参数/返回值|含义|
|-|-|
|flags|int类型，预留参数|

### 2. 创建SHM
#### create
创建SHM
```python
def create(id, rank_size, rank_id, local_mem_size, data_op_type = SMEMS_DATA_OP_MTE, flags = 0) -> ShareMemory
```

|参数/返回值|含义|
|-|-|
|id|SMEM对象id，用户指定，与其他SMEM对象不重复，范围为[0, 63]|
|rank_size|参与创建SMEM的rank数量，最大支持1024|
|rank_id|当前rank id|
|local_mem_size|每个rank贡献到创建SMEM对象的空间大小，单位字节，范围为[2MB, 4GB]，且需为2MB的倍数|
|data_op_type|数据操作类型，参考smem_shm_data_op_type类型定义|
|flags|预留参数|
|返回值|ShareMemory对象（失败抛RuntimeError异常）|

### 3. 常用类型
#### ShmConfig类
```python
class ShmConfig:
    def __init__(self) -> None
```

|构造函数/属性|含义|
|-|-|
|构造函数|SMEM配置初始化|
|init_timeout|init函数的超时时间，默认120秒（最小值为1，最大值为SMEM_BM_TIMEOUT_MAX）|
|create_timeout|create函数的超时时间，默认120秒（最小值为1，最大值为SMEM_BM_TIMEOUT_MAX）|
|operation_timeout|控制操作的超时时间|
|start_store|是否启动配置存储|
|flags|预留参数|

#### ShareMemory类
```python
class ShareMemory:
    def set_context(context) -> int:
    def destroy(flags:int = 0) -> int:
    def query_support_data_operation() -> int:
    def barrier() -> None:
    def all_gather(local_data) -> bytes:
    def topology_can_reach(remote_rank, reach_info) -> int
```

|属性/方法|含义|
|-|-|
|set_context方法|设置共享内存对象的用户额外上下文|
|set_context参数context|额外上下文数据|
|destroy方法|销毁内存句柄|
|destroy参数flags|预留参数|
|query_support_data_operation方法|查询支持的数据操作|
|barrier|在内存对象上执行barrier操作|
|all_gather方法|在内存对象上执行allgather操作|
|all_gather参数local_data|输入的数据，bytes类型|
|topology_can_reach方法|查询到远程排名的可达性|
|topology_can_reach属性remote_rank|int类型，目标rankid|
|topology_can_reach属性reach_info|int类型，可达性信息|
|local_rank(只读属性)|获取内存对象的本地排名|
|rank_size(只读属性)|获取内存对象的秩大小|
|gva(只读属性)|获取全局虚拟地址|

#### ShmDataOpType枚举
```python
class ShmDataOpType(Enum):
    MTE
    SDMA
    AIV_SDMA
    RDMA
```

## TRANSFER接口
### 1. 常用类型
#### TransferEngine类
```python
class TransferEngine:
    def __init__(self):
    def initialize(store_url: str, session_id: str, role: str, device_id: int, data_op_type = TransDataOpType.SDMA) -> int:
    def get_rpc_port() -> str:
    def transfer_sync_write(dest_session: str, buffer, peer_buffer, length, flags = 0) -> int:
    def batch_transfer_sync_write(dest_session: str, buffers, peer_buffers, lengths, flags = 0) -> int:
    def transfer_async_write_submit(dest_session: str, buffer, peer_buffer, length, stream, flags = 0) -> int:
    def transfer_async_read_submit(dest_session: str, buffer, peer_buffer, length, stream, flags = 0) -> int:
    def register_memory(buffer_addr, capacity) -> int:
    def unregister_memory(buffer_addr) -> int:
    def batch_register_memory(buffer_addrs, capacities) -> int:
    def transfer_sync_read(dest_session: str, buffer, peer_buffer, length, flags = 0) -> int:
    def batch_transfer_sync_read(dest_session: str, buffers, peer_buffers, lengths, flags = 0) -> int:
    def batch_transfer_async_write_submit(dest_session: str, buffers, peer_buffers, lengths, stream, flags = 0) -> int:
    def batch_transfer_async_read_submit(dest_session: str, buffers, peer_buffers, lengths, stream, flags = 0) -> int:
    def batch_transfer_write_with_quant(dest_session: str, buffers, peer_buffers, lengths, scale_buffers, offset_buffers, unit_num, input_type = 0, stream = 0, flags = 0) -> int:
    def trans_malloc(capacity) -> int:
    def trans_free(buffer_addr) -> int:
    def destroy() -> None:
    def unInitialize() -> None:
```

|属性|含义|
|-|-|
|initialize方法|TRANS配置初始化，成功返回0，其他为错误码|
|initialize参数store_url|config store地址，格式支持 `tcp://ip:port`、`etcd://ip:port`、`etcd://ip:port#instanceId`（etcd 多集群隔离）|
|initialize参数session_id|该TRANS实例的唯一标识，格式ip:port|
|initialize参数role|当前进程的角色|
|initialize参数device_id|当前设备的唯一标识|
|initialize参数data_op_type|数据传输操作类型，默认 TransDataOpType.SDMA|
|get_rpc_port方法|获取可用的rpc端口+pid|
|transfer_sync_write方法|同步写接口,成功返回0，其他为错误码|
|transfer_sync_write参数dest_session|目的TRANS实例对应的标识|
|transfer_sync_write参数buffer|源地址的起始地址指针|
|transfer_sync_write参数peer_buffer|目的地址的起始地址指针|
|transfer_sync_write参数length|传输数据大小|
|transfer_sync_write参数flags|标记位，默认0|
|transfer_async_write_submit方法|异步写任务提交接口,相比于transfer_async_write增加了入参stream,成功返回0,其他为错误码|
|transfer_async_write_submit参数stream|需要提交到的acl.rt.stream|
|batch_transfer_sync_write方法|批量同步写接口，成功返回0，其他为错误码|
|batch_transfer_sync_write参数dest_session|目的TRANS实例对应的标识|
|batch_transfer_sync_write参数buffer|源地址的起始地址指针列表|
|batch_transfer_sync_write参数peer_buffers|目的地址的起始地址指针列表|
|batch_transfer_sync_write参数lengths|传输数据大小列表|
|batch_transfer_sync_write参数flags|标记位，默认0|
|register_memory方法|注册内存，成功返回0，其他为错误码|
|register_memory参数buffer_addr|注册地址的起始地址指针|
|register_memory参数capacity|注册地址大小|
|unregister_memory方法|注销内存，成功返回0，其他为错误码|
|unregister_memory参数buffer_addr|注销地址的起始地址指针|
|batch_register_memory方法|批量注册内存，成功返回0，其他为错误码|
|batch_register_memory参数buffer_addrs|批量注册地址的起始地址指针列表|
|batch_register_memory参数capacities|批量注册地址大小列表|
|destroy方法|销毁TRANS实例|
|unInitialize方法|TRANS退出|
|transfer_sync_read方法|同步读接口，成功返回0，其他为错误码|
|transfer_sync_read参数dest_session|目的TRANS实例对应的标识|
|transfer_sync_read参数buffer|本地用于接收读取数据的起始地址指针|
|transfer_sync_read参数peer_buffer|远端待读取数据的起始地址指针|
|transfer_sync_read参数length|传输数据大小|
|transfer_sync_read参数flags|标记位，默认0|
|batch_transfer_sync_read方法|批量同步读接口|
|batch_transfer_sync_read参数dest_session|目的TRANS实例对应的标识|
|batch_transfer_sync_read参数buffers|本地接收数据指针列表|
|batch_transfer_sync_read参数peer_buffers|远端数据指针列表|
|batch_transfer_sync_read参数lengths|传输数据大小列表|
|batch_transfer_sync_read参数flags|标记位，默认0|
|batch_transfer_async_write_submit方法|批量异步写提交接口|
|batch_transfer_async_write_submit参数dest_session|目的TRANS实例对应的标识|
|batch_transfer_async_write_submit参数buffers|源地址指针列表|
|batch_transfer_async_write_submit参数peer_buffers|目的地址指针列表|
|batch_transfer_async_write_submit参数lengths|传输数据大小列表|
|batch_transfer_async_write_submit参数stream|需要提交到的acl.rt.stream|
|batch_transfer_async_write_submit参数flags|标记位，默认0|
|batch_transfer_async_read_submit方法|批量异步读提交接口|
|batch_transfer_async_read_submit参数dest_session|目的TRANS实例对应的标识|
|batch_transfer_async_read_submit参数buffers|本地接收数据指针列表|
|batch_transfer_async_read_submit参数peer_buffers|远端数据指针列表|
|batch_transfer_async_read_submit参数lengths|传输数据大小列表|
|batch_transfer_async_read_submit参数stream|需要提交到的acl.rt.stream|
|batch_transfer_async_read_submit参数flags|标记位，默认0|
|batch_transfer_write_with_quant方法|批量随路量化写接口|
|batch_transfer_write_with_quant参数dest_session|目的TRANS实例对应的标识|
|batch_transfer_write_with_quant参数buffers|源地址指针列表|
|batch_transfer_write_with_quant参数peer_buffers|目的地址指针列表|
|batch_transfer_write_with_quant参数lengths|传输数据大小列表|
|batch_transfer_write_with_quant参数scale_buffers|量化scale指针列表|
|batch_transfer_write_with_quant参数offset_buffers|量化offset指针列表|
|batch_transfer_write_with_quant参数unit_num|量化单元数量|
|batch_transfer_write_with_quant参数input_type|输入数据类型，默认0|
|batch_transfer_write_with_quant参数stream|需要提交到的acl.rt.stream，默认0|
|batch_transfer_write_with_quant参数flags|标记位，默认0|
|trans_malloc方法|在TRANS引擎中分配内存|
|trans_malloc参数capacity|分配内存大小|
|trans_free方法|释放TRANS引擎分配的内存|
|trans_free参数buffer_addr|待释放的内存地址|

#### TransDataOpType枚举类
```python
class TransDataOpType(Enum):
    SDMA
    DEVICE_RDMA
```

#### TransferOpcode枚举类
```python
class TransferOpcode(Enum):
    Read
    Write
```
