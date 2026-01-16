# Python接口
安装完成run包并source安装路径下的set_env.sh后，即可在python中通过**import memfabric_hybrid**导入memfabric的python包，然后调用python接口

**Note: 后续支持pip安装**

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

### 2. 日志设置
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

### 3. 安全证书设置
#### set_conf_store_tls_key
注册Python解密处理程序
```python
def set_conf_store_tls_key(tls_pk, tls_pk_pw, py_decrypt_func:Callable[str, str], str) -> int
```

|参数/返回值|含义|
|-|-|
|tls_pk|私钥|
|tls_pk_pw|私钥口令|
|py_decrypt_func|口令解密函数|
|返回值|错误信息|

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

### 4. 错误信息获取/清理
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
|store_url|config store的IP和端口，格式tcp://ip:port|
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
def create(id, local_dram_size, local_hbm_size, data_op_type = SMEMS_DATA_OP_MTE, flags = 0) -> int
```

|参数/返回值|含义|
|-|-|
|id|SMEM对象id，用户指定，与其他SMEM对象不重复，范围为[0, 63]|
|local_dram_size|本地dram内存大小|
|local_hbm_size|本地hbm内存大小|
|data_op_type|数据操作类型|
|flags|预留参数|
|返回值|SMEM对象handle|

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
|auto_ranking属性|自动分配排名ID，默认为false|
|rank_id属性|用户指定的RankId，对autoRanking有效为False|
|flags属性|预留参数|

#### BmDataOpType枚举类
```python
class BmDataOpType(Enum):
    SDMA
    ROCE
```

#### BigMemory类
```python
class BigMemory:
    def join(flags = 0) -> int:
    def leave(flags = 0) -> int:
    def local_mem_size(mem_type = SMEM_MEM_TYPE_DEVICE) -> int:
    def peer_rank_ptr(peer_rank, mem_type = SMEM_MEM_TYPE_DEVICE) -> int:
    def destroy() -> None:
    def register(addr, size) -> int:
    def unregister(addr) -> int:
    def copy_data(src_ptr, dst_ptr, size, type, flags) -> int:
    def copy_data_batch(src_addrs, dst_addrs, sizes, count, type, flags) -> int:

```

|属性/方法|含义|  
|-|-|
|join方法|加入BM|
|join参数flags|预置参数|
|leave方法|退出BM|
|leave参数flags|预置参数|
|local_mem_size方法|获取创建BM本地贡献的空间大小|
|local_mem_size参数mem_type|本地贡献空间的内存类型|
|local_mem_size返回值|本地贡献空间大小，单位byte|
|peer_rank_ptr方法|获取rank id对应的贡献空间在gva上的地址位置|
|peer_rank_ptr参数peer_rank|指定的rank id|
|peer_rank_ptr参数mem_type|指定的rank id的贡献空间的内存类型|
|destroy方法|销毁BM|
|register方法|注册内存到BM|
|register参数addr|注册地址的起始地址指针|
|register参数size|注册地址的大小|
|unregister方法|从BM中注销内存|
|unregister参数addr|注销地址的起始地址指针|
|copy_data方法|拷贝数据对象|
|copy_data参数src_ptr(int)|source gva of data|
|copy_data参数dst_ptr(int)|destination gva of data|
|copy_data参数size(int)|size of data to be copied|
|copy_data参数type(BmCopyType)|copy type, L2G, G2L, G2H, H2G|
|copy_data参数flags(int)|optional flags|

## SMEM接口
### 1. 初始化/退出接口
#### initialize
初始化运行环境
```python
def initialize(store_url, world_size, rank_id, device_id, config) -> int
```

|参数/返回值|含义|
|-|-|
|store_url|config store的IP和端口，格式tcp://ip:port|
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

### 2. 创建SMEM
#### create
创建SMEM
```python
def create(id, rank_size, rank_id, local_mem_size, data_op_type = SMEMS_DATA_OP_MTE, flags = 0) -> int
```

|参数/返回值|含义|
|-|-|
|id|SMEM对象id，用户指定，与其他SMEM对象不重复，范围为[0, 63]|
|rank_size|参与创建SMEM的rank数量，最大支持1024|
|rank_id|当前rank id|
|local_mem_size|每个rank贡献到创建SMEM对象的空间大小，单位字节，范围为[2MB, 4GB]，且需为2MB的倍数|
|data_op_type|数据操作类型，参考smem_shm_data_op_type类型定义|
|flags|预留参数|
|返回值|SMEM对象handle|

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
    ROCE
```

## TRANSFER接口
### 1. 创建config store对象
#### create_config_store
创建config store对象
```python
def create_config_store(store_url: str) -> int
```

|参数/返回值|含义|
|-|-|
|store_url|业务面地址，格式tcp:://ip:port|
|返回值|成功返回0，其他为错误码|

### 2. 常用类型
#### TransferEngine类
```python
class TransferEngine:
    def __init__(self):
    def initialize(store_url: str, unique_id: str, role: str, device_id: int) -> int:
    def get_rpc_port() -> int:
    def transfer_sync_write(destflag: str, buffer, peer_buffer_address, length) -> int:
    def batch_transfer_sync_write(destflag: str, buffers, peer_buffer_addresses, lengths) -> int:
    def transfer_async_write_submit(destflag: str, buffer, peer_buffer_address, length, stream) -> int:
    def transfer_async_read_submit(destflag: str, buffer, peer_buffer_address, length, stream) -> int:
    def register_memory(buffer_addr, capacity) -> int:
    def unregister_memory(buffer_addr) -> int:
    def batch_register_memory(buffer_addrs, capacities) -> int:
    def destroy() -> None:
    def unInitialize() -> None:
```

|属性|含义|
|-|-|
|initialize方法|TRANS配置初始化，成功返回0，其他为错误码|
|initialize参数store_url|config store地址，格式tcp://ip:port|
|initialize参数unique_id|该TRANS实例的唯一标识，格式ip:port|
|initialize参数role|当前进程的角色|
|initialize参数device_id|当前设备的唯一标识|
|get_rpc_port方法|获取可用的rpc端口|
|transfer_sync_write方法|同步写接口,成功返回0，其他为错误码|
|transfer_sync_write参数destflag|目的TRANS实例对应的标识|
|transfer_sync_write参数buffer|源地址的起始地址指针|
|transfer_sync_write参数peer_buffer_address|目的地址的起始地址指针|
|transfer_sync_write参数length|传输数据大小|
|transfer_async_write_submit方法|异步写任务提交接口,相比于transfer_async_write增加了入参stream,成功返回0,其他为错误码|
|transfer_async_write_submit参数stream|需要提交到的acl.rt.stream|
|batch_transfer_sync_write方法|批量同步写接口，成功返回0，其他为错误码|
|batch_transfer_sync_write参数destflag|目的TRANS实例对应的标识|
|batch_transfer_sync_write参数buffer|源地址的起始地址指针列表|
|batch_transfer_sync_write参数peer_buffer_address|目的地址的起始地址指针列表|
|batch_transfer_sync_write参数length|传输数据大小列表|
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

#### TransferOpcode枚举类
```python
class TransferOpcode(Enum):
    READ
    WRITE
```