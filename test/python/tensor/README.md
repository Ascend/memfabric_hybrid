# Tensor测试脚本

## Tensor拷贝一致性测试脚本

用于测试Tensor与NPU上分配的内存之间拷贝的一致性。

```bash
usage: copy_consistency_test.py [-h] [--device DEVICE] [--repeat REPEAT] [--parallel PARALLEL] [--size SIZE] [--sync-create] [--same-stream]

options:
  -h, --help           显示帮助文本
  --device DEVICE      指定执行测试的NPU设备序号 (default: 0)
  --repeat REPEAT      指定测试重复的次数 (default: 1000)
  --parallel PARALLEL  指定测试并行的进程数 (default: 4)
  --size SIZE          指定测试拷贝的数据大小，支持无单位字节数或者这些单位：B, MB, GB (default: 2097152)
  --sync-create        指定是否在创建Tensor后增加一次同步流操作 (default: False)
  --same-stream        指定是否在拷贝时使用与创建Tensor相同的流 (default: False)
```

测试流程：

1. 根据并行数量启动多个进程，每个进程执行下列测试逻辑
2. 使用acl接口初始化并设置当前device
3. 如果开启 `--same-stream`，则取torch_npu当前stream给后续copy操作使用，否则创建一个新的stream
4. 进入重复测试流程：
    1. 使用acl接口申请内存，称为MemoryA
    2. 使用torch_npu创建2个tensor（随机浮点数），分别称为TensorA和TensorB
    3. 如果开启 `--sync-create`，则对torch_npu当前stream进行同步流操作
    4. 使用acl的异步拷贝接口将TensorA位置的数据拷贝至MemoryA并在同一个流上执行同步流操作
    5. 使用acl的异步拷贝接口将MemoryA位置的数据拷贝至TensorB并在同一个流上执行同步流操作
    6. 对比TensorA的和与TensorB的和是否一致
5. 打印测试结果
6. 释放资源并结束测试

使用示例：

```bash
# 在2号NPU上分别执行 --same-stream 和 --sync-create 测试，进程数为4（默认），重复 10000 次测试，测试数据为 1MB
python3 copy_consistency_test.py --device 2 --size 1MB --repeat 10000 --same-stream
python3 copy_consistency_test.py --device 2 --size 1MB --repeat 10000 --sync-create
```
