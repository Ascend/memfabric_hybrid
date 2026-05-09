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

##### 3.2.1 Integration of ZBAL into LLaMA-Factory + DeepSpeed for Fine-Tuning

First, we need to modify the relevant files in LLaMA-Factory and complete the ZBAL initialization.
The file `src/llamafactory/launcher.py` needs to be modified as follows:

```
if __name__ == "__main__":
    # The following are the newly added contents:
    enable_zbal = os.getenv("ENABLE_ZBAL", "0")
    world_size = int(os.getenv("WORLD_SIZE", "1"))
    if enable_zbal == "1":
        local_rank = int(os.environ.get("LOCAL_RANK", 0))
        # init zbal
        from zbal import zbal_init, zbal_set_logger_level
        zbal_set_logger_level(3)
        local_mem = 50 * 1024 * 1024 * 1024  # Adjust the memory size according to actual requirements.
        if not zbal_init(world_size, local_rank, local_rank, local_mem):
            print(f"[ERROR] zbal_init failed on rank {local_rank}.")
        else:
            print(f"[INFO] zbal_init success on rank {local_rank}")
    # zbal init end

    from llamafactory.train.tuner import run_exp  # use absolute import
    run_exp()
```

Next, you need to modify the relevant DeepSpeed(version:0.18.8) reference code. Patch as below:

```
diff --git a/accelerator/npu_accelerator.py b/accelerator/npu_accelerator.py
index 421050d..a284493 100644
--- a/accelerator/npu_accelerator.py
+++ b/accelerator/npu_accelerator.py
@@ -19,7 +19,8 @@ class NPU_Accelerator(DeepSpeedAccelerator):
     def __init__(self):
         super().__init__()
         self._name = 'npu'
-        self._communication_backend_name = 'hccl'
+        #self._communication_backend_name = 'hccl'
+        self._communication_backend_name = 'zbal'
         self._compile_backend = "inductor"
         # dict that holds class name <--> class type mapping i.e.
         # 'AsyncIOBuilder': <class 'op_builder.async_io.AsyncIOBuilder'>
@@ -273,7 +274,8 @@ class NPU_Accelerator(DeepSpeedAccelerator):
         return BuildExtension

     def export_envs(self):
-        return ['ASCEND', 'HCCL', 'LD_LIBRARY', 'PATH']
+        #return ['ASCEND', 'HCCL', 'LD_LIBRARY', 'PATH']
+        return ['ASCEND', 'ZBAL', 'LD_LIBRARY', 'PATH']

     def visible_devices_envs(self):
         return ['ASCEND_RT_VISIBLE_DEVICES']
diff --git a/deepspeed/comm/constants.py b/deepspeed/comm/constants.py
index 50d234c..9ea9e56 100644
--- a/deepspeed/comm/constants.py
+++ b/deepspeed/comm/constants.py
@@ -8,7 +8,8 @@ CCL_BACKEND = 'ccl'
 MPI_BACKEND = 'mpi'
 GLOO_BACKEND = 'gloo'
 SCCL_BACKEND = 'sccl'
-HCCL_BACKEND = 'hccl'
+#HCCL_BACKEND = 'hccl'
+HCCL_BACKEND = 'zbal'

 DEFAULT_AML_MASTER_PORT = "54965"
 DEFAULT_AML_NCCL_SOCKET_IFNAME = "^docker0,lo"
diff --git a/deepspeed/comm/torch.py b/deepspeed/comm/torch.py
index 8e821f2..52dcade 100644
--- a/deepspeed/comm/torch.py
+++ b/deepspeed/comm/torch.py
@@ -117,6 +117,7 @@ class TorchBackend(Backend):
         # The idea is to fake that dist backend is initialized even when
         # it is not so we can run on a single GPU without doing any init_process_group
         self.single_gpu_mode = True
+        backend = "zbal"
         self.init_process_group(backend, timeout, init_method, rank, world_size)
         if self.shm_comm_op != None:
             self.shm_comm_op.initialize(self.get_world_size(), self.get_rank())
diff --git a/deepspeed/runtime/fp16/onebit/adam.py b/deepspeed/runtime/fp16/onebit/adam.py
index fa81757..4b76854 100644
--- a/deepspeed/runtime/fp16/onebit/adam.py
+++ b/deepspeed/runtime/fp16/onebit/adam.py
@@ -97,7 +97,7 @@ class OnebitAdam(torch.optim.Optimizer):
         elif self.comm_backend_name == 'mpi':
             from deepspeed.runtime.comm.mpi import MpiBackend
             self.comm_backend_handle = MpiBackend(cuda_aware)
-        elif self.comm_backend_name == 'hccl':
+        elif self.comm_backend_name == 'hccl' or self.comm_backend_name == 'zbal':
             from deepspeed.runtime.comm.hccl import HcclBackend
             self.using_pipeline = hasattr(self.deepspeed, 'pipeline_enable_backward_allreduce')
             self.comm_backend_handle = HcclBackend(self.deepspeed.mpu)
diff --git a/deepspeed/runtime/fp16/onebit/lamb.py b/deepspeed/runtime/fp16/onebit/lamb.py
index 54f7fd5..1887218 100644
--- a/deepspeed/runtime/fp16/onebit/lamb.py
+++ b/deepspeed/runtime/fp16/onebit/lamb.py
@@ -123,7 +123,7 @@ class OnebitLamb(torch.optim.Optimizer):
         elif self.comm_backend_name == 'mpi':
             from deepspeed.runtime.comm.mpi import MpiBackend
             self.comm_backend_handle = MpiBackend(cuda_aware)
-        elif self.comm_backend_name == 'hccl':
+        elif self.comm_backend_name == 'hccl' or self.comm_backend_name == 'zbal':
             from deepspeed.runtime.comm.hccl import HcclBackend
             self.using_pipeline = hasattr(self.deepspeed, 'pipeline_enable_backward_allreduce')
             self.comm_backend_handle = HcclBackend(self.deepspeed.mpu)
diff --git a/deepspeed/runtime/fp16/onebit/zoadam.py b/deepspeed/runtime/fp16/onebit/zoadam.py
index 70282ec..090cc56 100644
--- a/deepspeed/runtime/fp16/onebit/zoadam.py
+++ b/deepspeed/runtime/fp16/onebit/zoadam.py
@@ -112,7 +112,7 @@ class ZeroOneAdam(torch.optim.Optimizer):
         elif self.comm_backend_name == 'mpi':
             from deepspeed.runtime.comm.mpi import MpiBackend
             self.comm_backend_handle = MpiBackend(cuda_aware)
-        elif self.comm_backend_name == 'hccl':
+        elif self.comm_backend_name == 'hccl' or self.comm_backend_name == 'zbal':
             from deepspeed.runtime.comm.hccl import HcclBackend
             self.using_pipeline = hasattr(self.deepspeed, 'pipeline_enable_backward_allreduce')
             self.comm_backend_handle = HcclBackend(self.deepspeed.mpu)
```

##### 3.2.2 Set the environment variable configuration.

When use zbal in training projects like deepspeed, add the following env to your workspace.

```bash
export ZBAL_OP_DEFUALT_STREAM=1
```