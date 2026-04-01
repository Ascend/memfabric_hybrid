# 02_register

## 场景
将本地地址注册到内存池，在批量拷贝路径中减少地址管理开销并提升拷贝效率。

## 目标
验证本地地址注册后，`copy_data_batch` 在固定负载下的性能表现。

## 使用能力
- 基础生命周期接口
- `BigMemory.register(addr, size)` / `BigMemory.unregister(addr)`
- `copy_data_batch`（主路径）

## 规模建议
- world_size=1
- local_dram_size=4GB
- 待注册的内存大小：4GB
- 总拷贝数据量：4GB
- 每个数据块大小：4MB
- 批量任务个数：16 / 128 / 512

## 必要条件
至少 1 张可用NPU。

## 验收标准
- 在 16 / 128 / 512 三组批量任务下均可稳定完成 4GB 拷贝
- 记录三组参数对应的时延与吞吐数据，与[01_copy_data_batch](../01_copy_data_batch/README.md)对比
