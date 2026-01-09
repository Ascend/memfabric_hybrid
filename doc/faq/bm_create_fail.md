# BM创建失败
## 问题现象
创建BM时，打开device失败，日志中报以下错误：
```
[HYBM device_rdma_transport_manager.cpp:506 OpenTsd] TsdOpen for (deviceId=1, rankCount=2) failed: 31
[HYBM device_rdma_transport_manager.cpp:474 PrepareOpenDevice] open tsd failed.
[HYBM device_rdma_transport_manager.cpp:79 OpenDevice] PrepareOpenDevice failed.
[HYBM hybm_entity_default.cpp:1076 InitTransManager] Failed to open device, ret: -1
[HYBM hybm_entity_default.cpp:61 Initialize] init transport manager failed
[HYBM hybm_big_mem_entry.cpp:35 hybm_create_entity] initialize entity failed: -1
[SMEM smem_bm_entry.cpp:54 Initialize] create entity failed
[SMEM smem_bm.cpp:176 smem_bm_create] entry init failed, result: -1
```

## 问题根因
此问题是因为环境上Id为1的NPU卡被占用，可以用npu-smi info进行查看。


## 解决方案
换一张空闲的卡或者等此卡空闲了再使用。