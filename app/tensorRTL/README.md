# TensorRTL

> TensorRTL - 张量重分布高性能组件，专为大规模AI业务提供张量transfer能力

## ✨ 特性

- 🔧 **高度易用**：模块化设计，为用户提供易用接口，可实现张量在集群内的任意分布
- 🚀 **零冗余映射**：零冗余通信映射，极致减少reshard通信量
- ↔️ **多通信后端**：支持Device、Host、ipc张量传输，支持HCCL后端和Memfabric后端

## 📦 安装

### 环境要求
- Python 3.8+
- PyTorch 2.6.0+
- CANN 8.3.RC1
- Memfabric

### 源码安装
```bash
git clone ssh://git@szv-y.codehub.huawei.com:2222/BeiMing/service_domain/acceleration_library/ascend/TensorRTL.git
cd tensorRTL/src
pip install -e .
```

### **快速开始**
```markdown
### 基础示例
```python
import torch
import tensor_rtl as trtl

# 创建分布式环境
torch.distributed.init_process_group(
        backend='hccl',
        world_size=world_size,
        rank=rank,
        init_method=init_method)

# 创建PTensor
x = torch.randn(1024, 1024)
ptensor =  trtl.PTensor(
                tensor=x,
                dtype=x.dtype,
                ndim=2,
                device_mesh=[0,1,2,3],
                global_size=(4096,),
                shard_dim=0,
                rank=torch.distributed.get_rank(),
                backend='hccl'
            )
# 创建通信执行器
executor = trtl.BatchP2PExecutor()

# 张量从分布在device[0,1,2,3]->device[0,1]
ptensor.transfer_map([0,1])

# 通信执行
executor.execute(ptensor.get_transfer_list())

tensor_list = ptensor.collect_tensor()
print(f"转换后的张量: {tensor_list}")

```

### VeRL适配
```bash
git clone https://github.com/volcengine/verl/tree/release/v0.6.1
cd verl
pip install --no-deps -e .
cp TensorRTL\examples\verl\modify_verl.patch .\verl
git apply modify_verl.patch

export USE_TENSOR_RTL=1
