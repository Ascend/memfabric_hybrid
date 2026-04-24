# 03_second_mapping

## 场景
当前卡侧dva最大仅支持128TB的地址范围映射，针对分离部署场景，有的节点贡献超大内存，有的节点不贡献内存，如果不开启二次映射，以maxSize=1TB，
worldSize=1024为例，需要映射的总的地址范围需要1PB，大于128TB导致报错，但实际上整个集群实际贡献内存远小于这个值。
针对这种分离部署的场景可以通过开启二次映射特性，只映射实际贡献内存的节点到卡侧，GVA可超过128TB，上层用户使用无需修改。
普通场景下，GVA就是DVA，传输引擎直接使用GVA即可传输。开启二次映射后，GVA不等于DVA，需要进行一次查表转换。
A3 device_sdma单机单卡最小闭环：开启二次映射功能后验证读写功能是否正常。

## 目标
验证内存池基础生命周期与最小数据通路是否工作正常。

## 使用能力
- `initialize(store_url, world_size, device_id, config)`
- `create2(id, local_dram_size, max_dram_size, local_hbm_size=0, max_hbm_size=0, ...)`
- `extend_local_mem(...)`
- `BigMemory.join()`
- `BigMemory.peer_rank_ptr(peer_rank, mem_type)`（本 rank）
- `BigMemory.copy_data(...)`（H2G / G2H）
- `BigMemory.leave()` / `BigMemory.destroy()`

## 规模建议
- world_size=8
- local_dram_size=1GB, max_dram_size=8GB
- 数据块：1KB

## 必要条件
单机至少 1 张可用NPU，且进程可访问 config store( tcp://ip:port ）

## 验收标准
- 初始化、创建、join、copy、leave、destroy 全流程返回成功
- 写入后读回数据一致
- 无错误码残留（`get_last_err_msg` 为空）

## 运行（Python）

依赖：已安装 **`memfabric_hybrid`**

```bash
cd examples/memory_pool/04_feature/03_second_mapping
python3 03_second_mapping.py
```

成功时打印 `second mapping test ok`

## Q&A
