## 特性简介
### 1) Big Memory(BM)
面向大内存使用场景，提供大容量的HBM+DRAM池化空间，提供全局统一的虚拟地址编址和多路径的内存访问能力，支持动态扩缩容

    * 支持创建统一内存空间存储数据
    * 支持同步数据拷贝，包含L2G, G2L, G2H, H2G, G2G
    * 自动分配rank (初始化时用户可以不指定rank,由BM内部自动生成,具体参见smem_bm_init接口)
    * 动态join和leave (参见smem_bm_join和smem_bm_leave接口)
    * 接口支持的语言: c, python

Note:
```angular2html
L2G: copy data from local HBM space to global memory space
G2L: copy data from global memory space to local HBM space
H2G: copy data from local DRAM memory to global memory space
G2H: copy data from global memory space to local DRAM memory
G2G: copy data from global memory space to global memory space
```

安装MemFabric后，BM接口头文件位于
```${MEMFABRIC_HYBRID_HOME_PATH}/${arch}-${os}/include/smem/host/smem_bm.h```

##### 使用简介
详情参考[bm_example](../example/bm/BmCpp/README.md)  
简单概述如下
```
c接口使用方式:
1. 调用smem_bm_init接口初始化
2. 调用smem_bm_create接口创建实例
3. 调用smem_bm_ptr_by_mem_type接口获取gva地址，然后调用smem_bm_copy接口访问对应地址的内存

python接口使用方式:
1. 调用mf_smem.bm.initialize接口初始化
2. 调用mf_smem.bm.create接口创建实例获得bm_handle
3. 调用bm_handle.peer_rank_ptr接口获取gva地址，然后调用bm_handle.copy_data接口访问对应地址的内存
```

### 2) Transfer(TRANS)
面向大模型推理的数据传输场景，支持对用户注册的HBM内存进行高性能D2D传输

    * 支持用户注册内存，并对该内存进行跨server共享
    * 提供对用户注册的内存进行数据拷贝的接口，并提供读写语义
    * 接口支持的语言: c, python

安装MemFabric后，BM接口头文件位于
```${MEMFABRIC_HYBRID_HOME_PATH}/${arch}-${os}/include/smem/host/smem_trans.h```

##### 使用简介
详情参考[trans_example](../example/trans/demo/README.md)  
简单概述如下
```
c接口使用方式:
1. 调用smem_trans_init接口初始化
2. 调用smem_trans_create接口创建实例
3. 调用smem_trans_register_mem接口将内存注册给memfabric，然后调用smem_trans_write或smem_trans_read接口进行传输

python接口使用方式:
1. 调用memfabric_hybrid.TransferEngine接口创建engine
2. 调用engine.initialize接口初始化
3. 调用engine.register_memory接口将内存注册给memfabric，然后调用engine.transfer_sync_write接口进行传输
```

### 3) Share Memory(SHM)
面向算子场景，提供全局共享内存，支持AICORE的细粒度的读写操作

    * 支持创建统一内存空间
    * 支持用户传入自定义数据在卡侧访问(参见smem_shm_set_extra_context和smem_shm_get_extra_context_addr接口)
    * 支持host侧全局barrier和allgather(参见smem_shm_control_barrier和smem_shm_control_allgather接口)
    * 支持超节点内，卡侧通过MTE直接访问统一内存
    * 卡侧提供基础copy接口
    * 接口支持的语言: c, python

安装MemFabric后，SHM接口头文件位于
```
${MEMFABRIC_HYBRID_HOME_PATH}/${arch}-${os}/include/smem/host/smem_shm.h
和
${MEMFABRIC_HYBRID_HOME_PATH}/${arch}-${os}/include/smem/device/smem_shm_aicore_base_api.h
```

##### 使用简介
详情参考[shm_example](../example/shm/ShiftPutGet/README.md)  
简单概述如下
```
c接口使用方式:
1. 调用smem_shm_init接口初始化
2. 调用smem_shm_create接口创建实例并获得gva地址
3. 编写算子直接访问或通过smem_shm_aicore_base_api.h的接口访问gva地址
```