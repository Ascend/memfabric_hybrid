# 01_register_reuse_performance

## 场景
在主流批量拷贝链路前，先对长期复用的缓冲区执行注册（register），再在业务周期内复用，降低高频内存管理开销。

## 目标
通过“预注册 + 复用”方式提升稳定吞吐，减少性能抖动。

## 使用能力（仅内存池）
- `BigMemory.register(addr, size)` / `BigMemory.unregister(addr)`
- `copy_data_batch`（主路径）
- 基础生命周期接口（`initialize/create2/join/leave`）

## 规模建议
- world_size=2~8
- 预注册内存池：1GB / 4GB / 8GB
- 批量任务：64 / 256 / 512

## 必要条件
待注册内存需满足接口约束（如 device_rdma 场景下 DRAM buffer 首地址 4K 对齐），且业务负载具备“内存可复用”特征。

## 验收标准
- 相比未预注册基线，吞吐有稳定提升或抖动明显下降
- register/unregister 生命周期无泄漏、无错误码堆积
- 在目标并发下保持稳定运行
