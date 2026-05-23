# 03_device_sdma

## 场景
在 [02_register](../02_register/README.md) 的基础上，将协议切换为 `device_sdma`，在相同负载下感受拷贝性能跃迁。

## 目标
验证切换到 `device_sdma` 协议后，`copy_data_batch` 在固定负载下的性能表现。

## 使用能力
- 基础生命周期接口
- `create2` 中 data_op_type 选择 `bm.BmDataOpType.SDMA`
- `copy_data_batch`（主路径）
- **注意**：SDMA 协议无需地址注册（register/unregister），源码中显式跳过注册步骤

## 规模建议
- world_size=1
- local_dram_size=4GB
- 总拷贝数据量：4GB
- 每个数据块大小：4MB
- 批量任务个数：16 / 128 / 512

## 必要条件
A3单机至少 1 张可用NPU。

## 验收标准
- 在 16 / 128 / 512 三组批量任务下均可稳定完成 4GB 拷贝
- 记录三组参数对应的时延与吞吐数据，与[02_register](../02_register/README.md)对比
