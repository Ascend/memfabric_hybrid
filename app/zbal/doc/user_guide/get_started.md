## Get Started

### 1. Get wheel package

The zbal wheel pacakge has been uploaded to pip repo, install zbal as following command.

```bash
pip install zbal-ascend
```

If there is a network issue on you workspace, compiling to get wheel package is also a way to it, see details [here](../../README.md#quickstart).

### 2. Import and init

Add initializing code as following to your project.

```python
from zbal import zbal_init, zbal_uninit, zbal_set_logger_level

zbal_set_logger_level(3)    # 0-debug, 1-info, 2-warn, 3-error, 4-fault

world_size = 16
local_rank = xx
global_rank = xx
local_mem = 1024 * 1024 * 1024

if not zbal_init(world_size, local_rank, global_rank, local_mem):
    return

group = dist.init_process_group("zbal", rank=global_rank, world_size=world_size)


# zbal_uninit()
```

### 3. Other tips

#### 3.1 For Un-support collective communicator operators

Some of the collective communicator opereatos are un-supported from now, or if the operators performs bad compare to hccl operators in your situation, use the following way to repalce zbal operators with hccl operators. Multi operators separated by comma.

```bash
export ZBAL_HCCL_OP="allgather,allreduce"
```

#### 3.2 For training

When use zbal in training projects like deepspeed, add the following env to your workspace.

```bash
export ZBAL_OP_DEFUALT_STREAM=1
```