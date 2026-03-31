# 01_single_card_dram_pool

## 场景
单机单卡最小闭环：通过 create2 创建 DEVICE_RDMA 的一定容量的 DRAM 池，完成一次写入与读回校验。

## 目标
验证内存池基础生命周期与最小数据通路是否工作正常。

## 使用能力
- `initialize(store_url, world_size, device_id, config)`
- `create2(id, local_dram_size, max_dram_size, local_hbm_size=0, max_hbm_size=0, ...)`
- `BigMemory.join()` / `BigMemory.leave()` / `BigMemory.destroy()`
- `BigMemory.peer_rank_ptr(peer_rank, mem_type)`（本 rank）
- `BigMemory.copy_data(src_ptr, dst_ptr, size, type, flags)`（H2G/G2H）
- `BigMemory.copy_data_batch(src_ptr, dst_ptr, size, type, flags)`（H2G/G2H）

## 规模建议
- world_size=1
- local_dram_size=1GB, max_dram_size=1GB
- 数据块：4KB、64KB、1MB

## 必要条件
单机至少 1 张可用设备卡，且进程可访问 config store( tcp://ip:port ）

## 验收标准
- 初始化、创建、join、copy、leave、destroy 全流程返回成功
- 写入后读回数据一致
- 无错误码残留（`get_last_err_msg` 为空）
