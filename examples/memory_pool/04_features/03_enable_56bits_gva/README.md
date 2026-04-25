# 03_enable_56bits_gva

## 场景
当前卡侧 dva 默认按 48 位编址，最大仅支持 128TB 的地址范围映射。针对分离部署场景，有的节点贡献超大内存，
有的节点不贡献内存：以 maxSize=1TB、worldSize=1024 为例，需要映射的总地址范围理论上达到 1PB，超过 128TB 直接报错；
但整个集群实际贡献内存远小于这个理论值。针对这种分离部署的场景可以通过开启 56 位 GVA 特性，
只映射实际贡献内存的节点到卡侧，GVA 表达范围可以超过 128TB（实际可用容量上限仍为 128TB），上层用户使用无需修改。
普通场景下，GVA 就是 DVA，传输引擎直接使用 GVA 即可传输。开启 56 位 GVA 后，GVA 不等于 DVA，需要进行一次查表转换。

A3 device_sdma 单机单卡最小闭环：开启 56 位 GVA 功能后验证读写功能是否正常。

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
cd examples/memory_pool/04_features/03_enable_56bits_gva
python3 03_enable_56bits_gva.py
```

成功时打印 `56-bit GVA test ok`

## Q&A
