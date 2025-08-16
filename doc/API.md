> 注：如下接口对外封装了相同含义的Python接口，详细信息可参考`src/smem/csrc/python_wrapper/pysmem.cpp`。

##### 公共接口列表

1. 初始化运行环境
    ```c
    int32_t smem_init(uint32_t flags);
    ```

    |参数/返回值|含义|
    |-|-|
    |flags|预留参数|
    |返回值|成功返回0，其他为错误码|

1. 创建config store对象
    ```c
    int32_t smem_create_config_store(const char *storeUrl);
    ```

    |参数/返回值|含义|
    |-|-|
    |storeUrl|业务面地址，格式tcp:://ip:port|
    |返回值|成功返回0，其他为错误码|


1. 设置自定义日志函数
    ```c
    int32_t smem_set_extern_logger(void (*func)(int level, const char *msg));
    ```

    |参数/返回值|含义|
    |-|-|
    |func|函数指针|
    |level|日志级别，0-debug 1-info 2-warn 3-error|
    |msg|日志内容|
    |返回值|成功返回0，其他为错误码|

1. 设置日志打印级别
    ```c
    int32_t smem_set_log_level(int level);
    ```

    |参数/返回值|含义|
    |-|-|
    |level|日志级别，0-debug 1-info 2-warn 3-error|
    |返回值|成功返回0，其他为错误码|

1. 退出运行环境
    ```c
    void smem_uninit();
    ```

1. 获取最后一条错误信息
    ```c
    const char *smem_get_last_err_msg();
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|错误信息|

1. 获取最后一条错误信息并清空所有错误信息
    ```c
    const char *smem_get_and_clear_last_err_msg();
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|错误信息|

1. 设置私钥解密的函数，仅在开启Tls，并且使用的是加密私钥时（在配置SMEM_CONF_STORE_TLS_INFO环境变量时，有传入tlsPkPwd参数），需要调用该接口进行设置。
    ```c
    int32_t smem_register_decrypt_handler(const smem_decrypt_handler h);
    ```
    |参数/返回值|含义|
    |-|-|
    |h|密钥解密函数|
    |返回值|错误信息|
    ```c
    typedef int (*smem_decrypt_handler)(const char *cipherText, int *cipherTextLen, char *plainText, int *plainTextLen);
    ```
    |参数/返回值|含义|
    |-|-|
    |cipherText|密文（加密的密钥）|
    |cipherTextLen|密文的长度|
    |plainText|解密后的密钥（出参）|
    |plainTextLen|解密后的密钥长度|
    |返回值|错误信息|

##### BM接口列表

1. BM配置初始化
    ```c
    int32_t smem_bm_config_init(smem_bm_config_t *config);
    ```
    
    |参数/返回值|含义|
    |-|-|
    |config|初始化参数|
    |返回值|成功返回0，其他为错误码|

1. 初始化BM

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

1. BM退出

    ```c
    void smem_bm_uninit(uint32_t flags);
    ```
    
    |参数/返回值|含义|
    |-|-|
    |flags|预留参数|

1. 获取当前rank的id
    ```c
    uint32_t smem_bm_get_rank_id(void);
    ```
    
    |参数/返回值|含义|
    |-|-|
    |返回值|成功返回当前rank id，失败返回u32最大值|

1. 创建BM
    ```c
    smem_bm_t smem_bm_create(uint32_t id, uint32_t memberSize,
        smem_bm_data_op_type dataOpType, uint64_t localDRAMSize,
        uint64_t localHBMSize, uint32_t flags);
    ```

    |参数/返回值|含义|
    |-|-|
    |id|BM id，用户自定义，BM之间取不同值|
    |memberSize|创建BM的rank数量，最大支持1024|
    |dataOpType|数据操作类型，取值内容参考smem_bm_data_op_type定义|
    |localDRAMSize|创建BM当前rank贡献的DRAM空间大小，单位字节，范围为[2MB, 4GB]，且需为2MB的倍数（保留参数，后续迭代使用）|
    |localHBMSize|创建BM当前rank贡献的HBM空间大小，单位字节，范围为[2MB, 4GB]，且需为2MB的倍数|
    |flags|创建标记位，预留|
    |返回值|成功返回BM handle，失败返回空指针|

1. 销毁BM
    ```c
    void smem_bm_destroy(smem_bm_t handle);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|待销毁BM handle|

1. 加入BM

    ```c
    int32_t smem_bm_join(smem_bm_t handle, uint32_t flags, void **localGvaAddress);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|待加入BM handle|    
    |flags|预留参数|
    |localGvaAddress|当前rank在gva上的地址位置|
    |返回值|成功返回0，否则返回错误码|

1. 退出BM
    ```c
    int32_t smem_bm_leave(smem_bm_t handle, uint32_t flags);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|待退出BM handle|
    |flags|预留参数|
    |返回值|成功返回0，否则返回错误码|

1. 获取创建BM本地贡献的空间大小
    ```c
    uint64_t smem_bm_get_local_mem_size(smem_bm_t handle);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|BM handle|
    |返回值|本地贡献空间大小，单位byte|

1. 获取rank id对应在gva上的地址位置
    ```c
    void *smem_bm_ptr(smem_bm_t handle, uint16_t peerRankId);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|BM handle|
    |peerRankId|rank id|
    |返回值|rank地址对应空间位置指针|

1. 拷贝数据对象
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

1. 2D数据拷贝
    ```c
    int32_t smem_bm_copy_2d(smem_bm_t handle, smem_copy_2d_params *params, 
        smem_bm_copy_type t, uint32_t flags);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|BM handle|
    |params|拷贝数据的相关参数|
    |t|数据拷贝类型，L2G/G2L/G2H/H2G，L=local HBM memory，G=global space，H=Host memory|
    |flags|预留参数|
    |返回值|成功返回0，失败返回错误码|

##### SMEM接口列表

1. SMEM配置初始化
    ```c
    int32_t smem_shm_config_init(smem_shm_config_t *config);
    ```

    |参数/返回值|含义|
    |-|-|
    |config|初始化配置|
    |返回值|成功返回0，失败返回错误码|

1. SMEM初始化
    ```c
    int32_t smem_shm_init(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId, 
        uint16_t deviceId, smem_shm_config_t *config);
    ```

    |参数/返回值|含义|
    |-|-|
    |configStoreIpPort|config store的IP和端口，格式tcp://ip:port|
    |worldSize|参与SMEM初始化rank数量，最大支持1024|
    |rankId|当前rank id|
    |deviceId|当前rank的device id|
    |config|初始化SMEM配置|
    |返回值|成功返回0，失败返回错误码|

1. SMEM退出
    ```c
    void smem_shm_uninit(uint32_t flags);
    ```

    |参数/返回值|含义|
    |-|-|
    |flags|预留参数|

1. 查询支持的数据操作
    ```c
    uint32_t smem_shm_query_support_data_operation(void);
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|参考smem_shm_data_op_type类型定义|

1. 创建SMEM
    ```c
    smem_shm_t smem_shm_create(uint32_t id, uint32_t rankSize, uint32_t rankId, uint64_t symmetricSize,
        smem_shm_data_op_type dataOpType, uint32_t flags, void **gva);
    ```

    |参数/返回值|含义|
    |-|-|
    |id|SMEM对象id，用户指定，与其他SMEM对象不重复，范围为[0, 63]|
    |rankSize|参与创建SMEM的rank数量，最大支持1024|
    |rankId|当前rank id|
    |symmetricSize|每个rank贡献到创建SMEM对象的空间大小，单位字节，范围为[2MB, 4GB]，且需为2MB的倍数|
    |dataOpType|数据操作类型，参考smem_shm_data_op_type类型定义|
    |flags|预留参数|
    |gva|出参，gva空间地址|
    |返回值|SMEM对象handle|

1. 销毁SMEM
    ```c
    int32_t smem_shm_destroy(smem_shm_t handle, uint32_t flags);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SMEM对象handle|
    |flags|预留参数|
    |返回值|成功返回0，失败返回错误码|

1. 设置用户context
    ```c
    int32_t smem_shm_set_extra_context(smem_shm_t handle, const void *context, uint32_t size);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SMEM对象handle|
    |context|用户context指针|
    |size|用户context大小，最大64K，单位字节|
    |返回值|成功返回0，失败返回错误码|

1. 获取rank id
    ```c
    uint32_t smem_shm_get_global_rank(smem_shm_t handle);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SMEM对象handle|
    |返回值|在SMEM里的rank id|

1. 获取rank数量
    ```c
    uint32_t smem_shm_get_global_rank_size(smem_shm_t handle);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SMEM对象handle|
    |返回值|在SMEM里的rank个数|

1. 在SMEM对象执行barrier
    ```c
    int32_t smem_shm_control_barrier(smem_shm_t handle);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SMEM对象handle|
    |返回值|成功返回0，失败返回错误码|

1. 在SMEM对象执行allgather
    ```c
    int32_t smem_shm_control_allgather(smem_shm_t handle, const char *sendBuf, uint32_t sendSize, 
        char *recvBuf, uint32_t recvSize);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SMEM对象handle|
    |sendBuf|发送数据buffer|
    |sendSize|发送数据大小，单位字节|
    |recvBuf|接收数据buffer|
    |recvSize|接收数据大小，单位字节|
    |返回值|成功返回0，失败返回错误码|

1. rank连通检查
    ```c
    int32_t smem_shm_topology_can_reach(smem_shm_t handle, uint32_t remoteRank, uint32_t *reachInfo);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SMEM对象handle|
    |remoteRank|待检查rank id|
    |reachInfo|连通信息类型，参考smem_shm_data_op_type定义|
    |返回值|成功返回0，失败返回错误码|

1. 注册退出回调函数
    ```
    int32_t smem_shm_register_exit(smem_shm_t handle, void (*exit)(int));
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SMEM对象handle|
    |exit|退出函数|
    |返回值|成功返回0，失败返回错误码|

1. PE主动退出接口
    ```
    void smem_shm_global_exit(smem_shm_t handle, int status);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SMEM对象handle|
    |status|退出状态|

> 注：如下接口对外封装了相同含义的Python接口，详细信息可参考`src/mooncake_adapter/csrc/transfer/pytransfer.cpp`。
##### TRANS接口列表
1. TRANS配置初始化
    ```c
    int32_t smem_trans_config_init(smem_trans_config_t *config);
    ```
    
    |参数/返回值|含义|
    |-|-|
    |config|初始化参数|
    |返回值|成功返回0，其他为错误码|

1. 创建TRANS实例

    ```c
    int32_t smem_trans_t smem_trans_create(const char *storeUrl, const char *sessionId, const smem_trans_config_t *config)
    ```

    |参数/返回值|含义|
    |-|--|
    |storeURL|config store地址，格式tcp://ip:port|
    |sessionId|该TRANS实例的唯一标识，格式ip:port|
    |config|TRANS初始化配置|
    |返回值|成功返回0，其他为错误码|

1. 销毁TRANS实例

    ```c
    void smem_trans_destroy(smem_trans_t handle, uint32_t flags)
    ```

    |参数/返回值|含义|
    |-|--|
    |handle|TRANS对象handle|
    |flags|预留参数|

1. TRANS退出

    ```c
    void smem_trans_uninit(uint32_t flags)
    ```

    |参数/返回值|含义|
    |-|--|
    |flags|预留参数|

1. 注册内存

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

1. 批量注册内存

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

1. 注销内存

    ```c
    int32_t smem_trans_deregister_mem(smem_trans_t handle, void *address)
    ```

    |参数/返回值|含义|
    |-|-----------|
    |handle|TRANS对象handle|
    |address|注销地址的起始地址指针|
    |返回值|成功返回0，其他为错误码|

1. 同步写接口

    ```c
    int32_t smem_trans_write(smem_trans_t handle, const void *srcAddress, const char *destSession,
                                  void *destAddress, size_t dataSize)
    ```

    |参数/返回值|含义|
    |-|---------|
    |handle|TRANS对象handle|
    |srcAddress|源地址的起始地址指针|
    |destSession|目的TRANS实例对应的标识|
    |destAddress|目的地址的起始地址指针|
    |dataSize|传输数据大小|
    |返回值|成功返回0，其他为错误码|

1. 批量同步写接口

    ```c
    int32_t smem_trans_batch_write(smem_trans_t handle, const void *srcAddresses[], const char *destSession,
                                        void *destAddresses[], size_t dataSizes[], uint32_t batchSize)
    ```

    |参数/返回值|含义|
    |-|--------|
    |handle|TRANS对象handle|
    |srcAddresses[]|批量源地址的起始地址指针列表|
    |destSession|目的TRANS实例对应的标识|
    |destAddresses[]|批量目的地址的起始地址指针列表|
    |dataSizes[]|批量传输数据大小列表|
    |batchSize|批量传输数据数量|
    |返回值|成功返回0，其他为错误码|

##### 环境变量
|环境变量|含义|
|-|-|
|LD_LIBRARY_PATH|动态链接库搜索路径|
|ASCEND_HOME_PATH|cann包安装路径|
|VERSION|编译whl包版本，默认1.0.0|