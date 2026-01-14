> 注：如下接口对外封装了相同含义的Python接口，详细信息可参考`src/smem/csrc/python_wrapper/memfabric_hybrid/pymf_hybrid.cpp`。

[TOC]

# C接口
安装完成run包并source安装路径下的set_env.sh后，会添加memfabric_hybrid安装路径的环境变量MEMFABRIC_HYBRID_HOME_PATH

使用memfabric_hybrid相关接口时，需要include相关头文件(在\${MEMFABRIC_HYBRID_HOME_PATH}/${arch}-${os}/include/smem/host路径下)，并且在**链接时需要添加libmf_smem.so**(在${MEMFABRIC_HYBRID_HOME_PATH}/${arch}-${os}/lib64路径下)依赖

可以通过MEMFABRIC_HYBRID_HOME_PATH环境变量指定头文件和lib库依赖路径，从而完成代码构建

## 公共接口列表

### 1. 服务初始化/退出
#### smem_init
初始化运行环境
```c
int32_t smem_init(uint32_t flags);
```

|参数/返回值 |含义 |
|-|-|
|flags |预留参数 |
|返回值|成功返回0，其他为错误码 |

 #### smem_uninit
退出运行环境
```c
void smem_uninit();
```

### 2. 创建config store对象
#### smem_create_config_store
```c
int32_t smem_create_config_store(const char *storeUrl);
```

|参数/返回值| 含义                                                            |
|-|---------------------------------------------------------------|
|storeUrl| 业务面地址，格式tcp:://ip:port，如tcp://[::1]:5124，tcp://127.0.0.1:5124 |
|返回值| 成功返回0，其他为错误码                                                  |


### 3. 日志设置
#### smem_set_extern_logger
设置自定义日志函数
```c
int32_t smem_set_extern_logger(void (*func)(int level, const char *msg));
```

|参数/返回值|含义|
|-|-|
|func|函数指针|
|level|日志级别，0-debug 1-info 2-warn 3-error|
|msg|日志内容|
|返回值|成功返回0，其他为错误码|

#### smem_set_log_level
设置日志打印级别
```c
int32_t smem_set_log_level(int level);
```

|参数/返回值|含义|
|-|-|
|level|日志级别，0-debug 1-info 2-warn 3-error|
|返回值|成功返回0，其他为错误码|

### 4. 安全证书设置
#### smem_set_conf_store_tls
安装证书设置
```c
int32_t smem_set_conf_store_tls(bool enable, const char *tls_info, const uint32_t tls_info_len);
```

|参数/返回值|含义|
|-|-|
|enable|whether to enable tls|
|tls_info|the format describle in memfabric SECURITYNOTE.md, if disabled tls_info won't be use|
|tls_info_len|length of tls_info, if disabled tls_info_len won't be use|
|返回值|成功返回0，其他为错误码|	

#### smem_set_config_store_tls_key
设置私钥、口令和解密的函数，在开启Tls时，需要调用该接口。
```c
int32_t smem_set_config_store_tls_key(
    const char *tls_pk,
    const uint32_t tls_pk_len,
    const char *tls_pk_pw,
    const uint32_t tls_pk_pw_len,
    const smem_decrypt_handler h
);
```
|参数/返回值| 含义     |
|-|--------|
|tls_pk| 密钥内容   |
|tls_pk_len| 密钥内容长度 |
|tls_pk_pw| 口令内容   |
|tls_pk_pw_len| 口令内容长度 |
|h| 密钥解密函数 |
|返回值| 错误信息   |
```c
typedef int (*smem_decrypt_handler)(const char *cipherText, size_t cipherTextLen, char *plainText, size_t &plainTextLen);
```
|参数/返回值|含义|
|-|-|
|cipherText|密文（加密的用来加密私钥的密码）|
|cipherTextLen|密文的长度|
|plainText|解密后的密码（出参）|
|plainTextLen|解密后的密码长度（出参）|
|返回值|错误信息|

### 5. 错误信息获取/清理
#### smem_get_last_err_msg
获取最后一条错误信息
```c
const char *smem_get_last_err_msg();
```

|参数/返回值|含义|
|-|-|
|返回值|错误信息|

#### smem_get_and_clear_last_err_msg
获取最后一条错误信息并清空所有错误信息
```c
const char *smem_get_and_clear_last_err_msg();
```

|参数/返回值|含义|
|-|-|
|返回值|错误信息|

## BM接口列表

### 1. BM初始化/退出
#### smem_bm_config_init
BM配置初始化

```c
int32_t smem_bm_config_init(smem_bm_config_t *config);
```

|参数/返回值|含义|
|-|-|
|config|初始化参数|
|返回值|成功返回0，其他为错误码|

#### smem_bm_init
初始化BM

```c
int32_t smem_bm_init(const char *storeURL, uint32_t worldSize, uint16_t deviceId, const smem_bm_config_t *config);
```

|参数/返回值|含义|
|-|-|
|storeURL|config store地址，格式tcp://ip:port|
|worldSize|参与初始化BM的rank数量，最大支持1024|
|deviceId|当前rank的deviceId|
|config|BM初始化配置|
|返回值|成功返回0，其他为错误码|

#### smem_bm_uninit
BM退出

```c
void smem_bm_uninit(uint32_t flags);
```
    
|参数/返回值|含义|
|-|-|
|flags|预留参数|

### 2. 创建/销毁BM
#### smem_bm_create
创建BM
 ```c
smem_bm_t smem_bm_create(uint32_t id, uint32_t memberSize,
    smem_bm_data_op_type dataOpType, uint64_t localDRAMSize,
    uint64_t localHBMSize, uint32_t flags);
```

|参数/返回值|含义|
|-|-|
|id|BM id，用户自定义，BM之间取不同值|
|memberSize|创建BM的rank数量（保留参数，后续迭代使用）|
|dataOpType|数据操作类型，取值内容参考smem_bm_data_op_type定义|
|localDRAMSize|创建BM当前rank贡献的DRAM空间大小，单位字节，范围为(0, 2TB]（保留参数，后续迭代使用）|
|localHBMSize|创建BM当前rank贡献的HBM空间大小，单位字节，范围为(0, 64GB]|
|flags|创建标记位，预留|
|返回值|成功返回BM handle，失败返回空指针|


#### smem_bm_create2
创建BM
 ```c
smem_bm_t smem_bm_create2(uint32_t id, const smem_bm_create_option_t *option);
```

| 参数/返回值       | 含义                                                              |
|--------------|-----------------------------------------------------------------|
| id           | BM id，用户自定义，BM之间取不同值                                            |
| option       | 创建BM的配置参数                                                       |
| 返回值          | 成功返回BM handle，失败返回空指针                                           |

📌 **注意**：当dataOpType取值为SMEMB_DATA_OP_HOST_RDMA时，需要依赖libhcom.so，请参考[hcom项目](https://atomgit.com/openeuler/ubs-comm)获取最新的so文件。

#### smem_bm_destroy
销毁BM
```c
void smem_bm_destroy(smem_bm_t handle);
```

|参数/返回值|含义|
|-|-|
|handle|待销毁BM handle|

### 3.加入/退出BM
#### smem_bm_join
加入BM

```c
int32_t smem_bm_join(smem_bm_t handle, uint32_t flags, void **localGvaAddress);
```

|参数/返回值|含义|
|-|-|
|handle|待加入BM handle|    
|flags|预留参数|
|localGvaAddress|当前rank在gva上的地址位置|
|返回值|成功返回0，否则返回错误码|

#### smem_bm_leave
退出BM
```c
int32_t smem_bm_leave(smem_bm_t handle, uint32_t flags);
```

|参数/返回值|含义|
|-|-|
|handle|待退出BM handle|
|flags|预留参数|
|返回值|成功返回0，否则返回错误码|

### 4.拷贝/批量拷贝数据对象
#### smem_bm_copy
拷贝数据对象
```c
int32_t smem_bm_copy(smem_bm_t handle, smem_copy_params *params,  
    smem_bm_copy_type t, uint32_t flags);
```

|参数/返回值|含义|
|-|-|
|handle|BM handle|
|params|拷贝数据的相关参数|
|t|数据拷贝类型，L2G/G2L/G2H/H2G，L=local HBM memory，G=global space，H=Host memory|
|flags|预留参数|
|返回值|成功返回0，失败返回错误码|

#### smem_bm_copy_batch
批量拷贝数据对象
```c
int32_t smem_bm_copy_batch(smem_bm_t handle, smem_batch_copy_params *params, smem_bm_copy_type t, uint32_t flags);
```

|参数/返回值|含义|
|-|-|
|handle|BM handle|
|params|拷贝数据的相关参数|
|t|数据拷贝类型，L2G/G2L/G2H/H2G，L=local HBM memory，G=global space，H=Host memory|
|flags|预留参数|
|返回值|成功返回0，失败返回错误码|

### 5.查询接口
#### smem_bm_get_rank_id
获取当前rank的id
```c
uint32_t smem_bm_get_rank_id(void);
```
    
|参数/返回值|含义|
|-|-|
|返回值|成功返回当前rank id，失败返回u32最大值|

#### smem_bm_get_local_mem_size_by_mem_type
获取创建BM本地贡献的空间大小
```c
uint64_t smem_bm_get_local_mem_size_by_mem_type(smem_bm_t handle, smem_bm_mem_type memType);
```

|参数/返回值|含义|
|-|-|
|handle|BM handle|
|memType|Bmemory type, device or host|
|返回值|本地贡献空间大小，单位byte|

#### smem_bm_ptr_by_mem_type
获取rank id对应在gva上的地址位置
```c
void *smem_bm_ptr_by_mem_type(smem_bm_t handle, smem_bm_mem_type memType, uint16_t peerRankId);
```

|参数/返回值|含义|
|-|-|
|handle|BM handle|
|memType|memory type, SMEM_MEM_TYPE_DEVICE or SMEM_MEM_TYPE_HOST|
|peerRankId|rank id|
|返回值|rank地址对应空间位置指针|

#### smem_bm_get_rank_id_by_gva
根据全局地址获取rankId
```c
uint32_t smem_bm_get_rank_id_by_gva(smem_bm_t handle, void *gva);
```

|参数/返回值|含义|
|-|-|
|handle|BM handle|
|gva|addr|
|返回值|rank id if successful, UINT32_MAX is returned if failed|

### 6. 用户内存register/unregister
#### smem_bm_register_user_mem
smem_bm_register_user_mem（废弃）
```c
int32_t smem_bm_register_user_mem(smem_bm_t handle, uint64_t addr, uint64_t size);
```

|参数/返回值|含义|
|-|-|
|handle|BM handle|
|addr|注册地址的起始地址指针|
|size|注册地址的大小|
|返回值|成功返回0，失败返回错误码|

#### smem_bm_unregister_user_mem
smem_bm_unregister_user_mem
```c
int32_t smem_bm_unregister_user_mem(smem_bm_t handle, uint64_t addr);
```

|参数/返回值|含义|
|-|-|
|handle|BM handle|
|addr|注销地址的起始地址指针|
|返回值|成功返回0，失败返回错误码|
### 7.等待异步操作完成
#### smem_bm_wait
等待异步操作完成
```c
int32_t smem_bm_wait(smem_bm_t handle);
```

|参数/返回值|含义|
|-|-|
|handle|BM handle|
|返回值|成功返回0，失败返回错误码|
	
## SHM接口列表

### 1.SHM初始化/退出
#### smem_shm_config_init
SHM配置初始化
```c
int32_t smem_shm_config_init(smem_shm_config_t *config);
```

|参数/返回值|含义|
|-|-|
|config|初始化配置|
|返回值|成功返回0，失败返回错误码|

#### smem_shm_init
SHM初始化
```c
int32_t smem_shm_init(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId, 
    uint16_t deviceId, smem_shm_config_t *config);
```

|参数/返回值|含义|
|-|-|
|configStoreIpPort|config store的IP和端口，格式tcp://ip:port或者tcp6://[ip]:port|
|worldSize|参与SHM初始化rank数量，最大支持1024|
|rankId|当前rank id|
|deviceId|当前rank的device id|
|config|初始化SHM配置|
|返回值|成功返回0，失败返回错误码|

#### smem_shm_uninit
SHM退出
```c
void smem_shm_uninit(uint32_t flags);
```

|参数/返回值|含义|
|-|-|
|flags|预留参数|

### 2. 创建/销毁SHM
#### smem_shm_create
创建SHM
```c
smem_shm_t smem_shm_create(uint32_t id, uint32_t rankSize, uint32_t rankId, uint64_t symmetricSize,
    smem_shm_data_op_type dataOpType, uint32_t flags, void **gva);
```

|参数/返回值|含义|
|-|-|
|id|SHM对象id，用户指定，与其他SHM对象不重复，范围为[0, 63]|
|rankSize|参与创建SHM的rank数量，最大支持1024|
|rankId|当前rank id|
|symmetricSize|每个rank贡献到创建SHM对象的空间大小，单位字节，范围为[0, 64GB]|
|dataOpType|数据操作类型，参考smem_shm_data_op_type类型定义|
|flags|预留参数|
|gva|出参，gva空间地址|
|返回值|SHM对象handle|

#### smem_shm_destroy
销毁SHM
```c
int32_t smem_shm_destroy(smem_shm_t handle, uint32_t flags);
```

|参数/返回值|含义|
|-|-|
|handle|SHM对象handle|
|flags|预留参数|
|返回值|成功返回0，失败返回错误码|

### 3. 查询接口
#### smem_shm_query_support_data_operation
查询支持的数据操作
```c
uint32_t smem_shm_query_support_data_operation(void);
```

|参数/返回值|含义|
|-|-|
|返回值|参考smem_shm_data_op_type类型定义|

#### smem_shm_get_global_rank
获取rank id
```c
uint32_t smem_shm_get_global_rank(smem_shm_t handle);
```

|参数/返回值|含义|
|-|-|
|handle|SHM对象handle|
|返回值|在SHM里的rank id|

#### smem_shm_get_global_rank_size
获取rank数量
```c
uint32_t smem_shm_get_global_rank_size(smem_shm_t handle);
```

|参数/返回值|含义|
|-|-|
|handle|SHM对象handle|
|返回值|在SHM里的rank个数|

### 4. 设置用户context
#### smem_shm_set_extra_context
设置用户context
```c
int32_t smem_shm_set_extra_context(smem_shm_t handle, const void *context, uint32_t size);
```

|参数/返回值|含义|
|-|-|
|handle|SHM对象handle|
|context|用户context指针|
|size|用户context大小，最大64K，单位字节|
|返回值|成功返回0，失败返回错误码|

### 5.  在SHM对象执行barrier/allgather
#### smem_shm_control_barrier
在SHM对象执行barrier
```c
int32_t smem_shm_control_barrier(smem_shm_t handle);
```

|参数/返回值|含义|
|-|-|
|handle|SHM对象handle|
|返回值|成功返回0，失败返回错误码|

#### smem_shm_control_allgather
在SHM对象执行allgather
```c
int32_t smem_shm_control_allgather(smem_shm_t handle, const char *sendBuf, uint32_t sendSize, 
    char *recvBuf, uint32_t recvSize);
```

|参数/返回值|含义|
|-|-|
|handle|SHM对象handle|
|sendBuf|发送数据buffer|
|sendSize|发送数据大小，单位字节|
|recvBuf|接收数据buffer|
|recvSize|接收数据大小，单位字节|
|返回值|成功返回0，失败返回错误码|

### 6. rank连通检查
#### smem_shm_topology_can_reach
rank连通检查
```c
int32_t smem_shm_topology_can_reach(smem_shm_t handle, uint32_t remoteRank, uint32_t *reachInfo);
```

|参数/返回值|含义|
|-|-|
|handle|SHM对象handle|
|remoteRank|待检查rank id|
|reachInfo|连通信息类型，参考smem_shm_data_op_type定义|
|返回值|成功返回0，失败返回错误码|

### 7. 注册退出回调函数
#### smem_shm_register_exit
注册退出回调函数
```
int32_t smem_shm_register_exit(smem_shm_t handle, void (*exit)(int));
```

|参数/返回值|含义|
|-|-|
|handle|SHM对象handle|
|exit|退出函数|
|返回值|成功返回0，失败返回错误码|

### 8. PE主动退出接口
#### smem_shm_global_exit
PE主动退出接口
```
void smem_shm_global_exit(smem_shm_t handle, int status);
```

|参数/返回值|含义|
|-|-|
|handle|SHM对象handle|
|status|退出状态|

> 注：如下接口对外封装了相同含义的Python接口，详细信息可参考`src/mooncake_adapter/csrc/transfer/pytransfer.cpp`。
## TRANS接口列表
### 1. TRANS初始化/退出
#### smem_trans_config_init
TRANS配置初始化
```c
int32_t smem_trans_config_init(smem_trans_config_t *config);
```
    
|参数/返回值|含义|
|-|-|
|config|初始化参数|
|返回值|成功返回0，其他为错误码|

#### smem_trans_init
TRANS初始化
```c
int32_t smem_trans_init(const smem_trans_config_t *config)
```

|参数/返回值|含义|
|-|-|
|config|TRANS初始化配置|
|返回值|成功返回0，失败返回错误码|

#### smem_trans_uninit
TRANS退出

```c
void smem_trans_uninit(uint32_t flags)
```

|参数/返回值|含义|
|-|--|
|flags|预留参数|

### 2. 创建/销毁TRANS实例
#### smem_trans_create
创建TRANS实例

```c
int32_t smem_trans_t smem_trans_create(const char *storeUrl, const char *uniqueId, const smem_trans_config_t *config)
```

|参数/返回值|含义|
|-|--|
|storeURL|config store地址，格式tcp://ip:port|
|uniqueId|该TRANS实例的唯一标识，格式ip:port|
|config|TRANS初始化配置|
|返回值|成功返回0，其他为错误码|

#### smem_trans_destroy
销毁TRANS实例

```c
void smem_trans_destroy(smem_trans_t handle, uint32_t flags)
```

|参数/返回值|含义|
|-|--|
|handle|TRANS对象handle|
|flags|预留参数|

### 3. 注册/批量注册/注销内存
#### smem_trans_register_mem
注册内存

```c
int32_t smem_trans_register_mem(smem_trans_t handle, void *address, size_t capacity, uint32_t flags)
```

|参数/返回值|含义|
|-|--|
|handle|TRANS对象handle|
|address|注册地址的起始地址指针|
|capacity|注册地址大小|
|flags|预留参数|
|返回值|成功返回0，其他为错误码|

#### smem_trans_batch_register_mem
批量注册内存

```c
int32_t smem_trans_batch_register_mem(smem_trans_t handle, void *addresses[], size_t capacities[], uint32_t count,
                                      uint32_t flags)
```

|参数/返回值| 含义|
|-|----|
|handle|TRANS对象handle|
|addresses[]|批量注册地址的起始地址指针列表|
|capacities[]|批量注册地址大小列表|
|count|批量注册地址数量|
|flags|预留参数|
|返回值|成功返回0，其他为错误码|

#### smem_trans_deregister_mem
注销内存

```c
int32_t smem_trans_deregister_mem(smem_trans_t handle, void *address)
```

|参数/返回值|含义|
|-|-----------|
|handle|TRANS对象handle|
|address|注销地址的起始地址指针|
|返回值|成功返回0，其他为错误码|

### 4. 同步读/写
#### smem_trans_read
同步读接口

```c
int32_t smem_trans_read(smem_trans_t handle, void *localAddr, const char *remoteUniqueId,
                    const void *remoteAddr, size_t dataSize)
```

|参数/返回值|含义|
|-|---------|
|handle|TRANS对象handle|
|localAddr|本地用于接收读取数据的起始地址指针|
|remoteUniqueId|远端TRANS实例对应的标识|
|remoteAddr|远端待读取数据的起始地址指针|
|dataSize|传输数据大小，单位字节|
|返回值|成功返回0，其他为错误码|

#### smem_trans_batch_read
批量同步读接口

```c
int32_t smem_trans_batch_read(smem_trans_t handle, void *localAddrs[], const char *remoteUniqueId,
                          const void *remoteAddrs[], size_t dataSizes[], uint32_t batchSize)
```

|参数/返回值|含义|
|-|--------|
|handle|TRANS对象handle|
|localAddrs[]|本地用于接收读取数据的起始地址指针列表|
|remoteUniqueId|远端TRANS实例对应的标识|
|remoteAddrs[]|批量远端待读取数据的起始地址指针列表|
|dataSizes[]|批量传输数据大小列表，单位字节|
|batchSize|批量读操作的任务数|
|返回值|成功返回0，其他为错误码|	

#### smem_trans_write
同步写接口

```c
int32_t smem_trans_write(smem_trans_t handle, const void *localAddr, const char *remoteUniqueId,
                              void *remoteAddr, size_t dataSize)
```

|参数/返回值|含义|
|-|---------|
|handle|TRANS对象handle|
|localAddr|本地待写数据起始地址指针|
|remoteUniqueId|远端TRANS实例对应的标识|
|remoteAddr|远端存储数据起始地址指针|
|dataSize|传输数据大小，单位字节|
|返回值|成功返回0，其他为错误码|

#### smem_trans_batch_write
批量同步写接口

```c
int32_t smem_trans_batch_write(smem_trans_t handle, const void *localAddrs[], const char *remoteUniqueId,
                                    void *remoteAddrs[], size_t dataSizes[], uint32_t batchSize)
```

|参数/返回值|含义|
|-|--------|
|handle|TRANS对象handle|
|srcAddresses[]|批量本地待写数据起始地址指针列表|
|destUniqueId|远端TRANS实例对应的标识|
|destAddresses[]|批量远端存储数据起始地址指针列表|
|dataSizes[]|批量传输数据大小列表，单位字节|
|batchSize|批量写操作的任务数|
|返回值|成功返回0，其他为错误码|

### 5. 异步读/写提交
#### smem_trans_read_submit
异步读提交接口

```c
int32_t smem_trans_read_submit(smem_trans_t handle, void *localAddr, const char *remoteUniqueId,
                               const void *remoteAddr, size_t dataSize, void *stream)
```

| 参数/返回值       | 含义                   |
|--------------|----------------------|
| handle       | TRANS对象handle        |
| localAddr   | 本地用于接收读取数据的起始地址指针           |
| remoteUniqueId | 远端TRANS实例对应的标识       |
| remoteAddr    | 远端待读取数据的起始地址指针         |
| dataSize     | 传输数据大小，单位字节               |
| stream       | 需要将任务提交到的aclrtStream |
| 返回值          | 成功返回0，其他为错误码         |

#### smem_trans_write_submit
异步写提交接口

```c
int32_t smem_trans_write_submit(smem_trans_t handle, const void *localAddr, const char *remoteUniqueId,
                                void *remoteAddr, size_t dataSize, void *stream)
```

| 参数/返回值       | 含义                   |
|--------------|----------------------|
| handle       | TRANS对象handle        |
| localAddr   |本地待写数据起始地址指针          |
| remoteUniqueId | 远端TRANS实例对应的标识       |
| remoteAddr  | 远端存储数据起始地址指针         |
| dataSize     | 传输数据大小               |
| stream       | 需要将任务提交到的aclrtStream |
| 返回值          | 成功返回0，其他为错误码         |

## 环境变量
|环境变量|含义|
|-|-|
|LD_LIBRARY_PATH|动态链接库搜索路径|
|ASCEND_HOME_PATH|cann包安装路径|
|VERSION|编译whl包版本|