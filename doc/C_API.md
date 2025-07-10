##### 公共接口列表

1. 初始化运行环境
    ```c
    int32_t smem_init(uint32_t flags);
    ```

    |参数/返回值|含义|
    |-|-|
    |flags|预留参数|
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

1. 设置ssl参数，在打开TLS时设置
    ```c
    int32_t smem_set_ssl_option(const smem_tls_option *tlsOption);
    ```

    |参数/返回值|含义|
    |-|-|
    |tlsOption|tls相关参数|
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
    |storeURL|初始化参数|
    |worldSize|参与初始化BM的rank数量|
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
    |memberSize|创建BM的rank数量|
    |dataOpType|数据操作类型|
    |localDRAMSize|创建BM当前rank贡献的DRAM空间大小|
    |localHBMSize|创建BM当前rank贡献的HBM空间大小|
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
    int32_t smem_bm_copy(smem_bm_t handle, const void *src, void *dest, uint64_t size, 
        smem_bm_copy_type t, uint32_t flags);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|BM handle|
    |src|源gva地址|
    |dest|目的gva地址|
    |size|拷贝数据大小|
    |t|数据拷贝类型，L2G/G2L/G2H/H2G，L=local HBM memory，G=global space，H=Host memory|
    |flags|预留参数|
    |返回值|成功返回0，失败返回错误码|

1. 2D数据拷贝
    ```c
    int32_t smem_bm_copy_2d(smem_bm_t handle, const void *src, uint64_t spitch,
        void *dest, uint64_t dpitch, uint64_t width, uint64_t heigth,
        smem_bm_copy_type t, uint32_t flags)
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|BM handle|
    |src|源gva地址|
    |spitch|源数据拷贝间距|
    |dest|目的gva地址|
    |dpitch|目的数据拷贝间距|
    |width|拷贝数据宽度|
    |height|拷贝数据高度|
    |t|数据拷贝类型，L2G/G2L/G2H/H2G，L=local HBM memory，G=global space，H=Host memory|
    |flags|预留参数|
    |返回值|成功返回0，失败返回错误码|

##### SHM接口列表

1. SHM配置初始化
    ```c
    int32_t smem_shm_config_init(smem_shm_config_t *config);
    ```

    |参数/返回值|含义|
    |-|-|
    |config|初始化配置|
    |返回值|成功返回0，失败返回错误码|

1. SHM初始化
    ```c
    int32_t smem_shm_init(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId, 
        uint16_t deviceId, smem_shm_config_t *config);
    ```

    |参数/返回值|含义|
    |-|-|
    |configStoreIpPort|config store的IP和端口|
    |worldSize|参与SHM初始化rank数量|
    |rankId|当前rank id|
    |deviceId|当前rank的device id|
    |config|初始化SHM配置|
    |返回值|成功返回0，失败返回错误码|

1. SHM退出
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

1. 创建SHM
    ```c
    smem_shm_t smem_shm_create(uint32_t id, uint32_t rankSize, uint32_t rankId, uint64_t symmetricSize,
        smem_shm_data_op_type dataOpType, uint32_t flags, void **gva);
    ```

    |参数/返回值|含义|
    |-|-|
    |id|SHM对象id，用户指定，与其他SHM对象不重复|
    |rankSize|参与创建SHM的rank数量|
    |rankId|当前rank id|
    |symmetricSize|每个rank贡献到创建SHM对象的空间大小|
    |dataOpType|数据操作类型，参考smem_shm_data_op_type类型定义|
    |flags|预留参数|
    |gva|出参，gva空间地址|
    |返回值|SHM对象handle|

1. 销毁SHM
    ```c
    int32_t smem_shm_destroy(smem_shm_t handle, uint32_t flags);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SHM对象handle|
    |flags|预留参数|
    |返回值|成功返回0，失败返回错误码|

1. 设置用户context
    ```c
    int32_t smem_shm_set_extra_context(smem_shm_t handle, const void *context, uint32_t size);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SHM对象handle|
    |context|用户context指针|
    |size|用户context大小，最大64K|
    |返回值|成功返回0，失败返回错误码|

1. 获取rank id
    ```c
    uint32_t smem_shm_get_global_rank(smem_shm_t handle);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SHM对象handle|
    |返回值|在SHM里的rank id|

1. 获取rank数量
    ```c
    uint32_t smem_shm_get_global_rank_size(smem_shm_t handle);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SHM对象handle|
    |返回值|在SHM里的rank个数|

1. 在SHM对象执行barrier
    ```c
    int32_t smem_shm_control_barrier(smem_shm_t handle);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SHM对象handle|
    |返回值|成功返回0，失败返回错误码|

1. 在SHM对象执行allgather
    ```c
    int32_t smem_shm_control_allgather(smem_shm_t handle, const char *sendBuf, uint32_t sendSize, 
        char *recvBuf, uint32_t recvSize);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SHM对象handle|
    |sendBuf|发送数据buffer|
    |sendSize|发送数据大小|
    |recvBuf|接收数据buffer|
    |recvSize|接收数据大小|
    |返回值|成功返回0，失败返回错误码|

1. rank连通检查
    ```c
    int32_t smem_shm_topology_can_reach(smem_shm_t handle, uint32_t remoteRank, uint32_t *reachInfo);
    ```

    |参数/返回值|含义|
    |-|-|
    |handle|SHM对象handle|
    |remoteRank|待检查rank id|
    |reachInfo|连通信息|
    |返回值|成功返回0，失败返回错误码|