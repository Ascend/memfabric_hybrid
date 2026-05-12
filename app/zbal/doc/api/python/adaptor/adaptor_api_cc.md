## Python API for communications

*Note: ZBAL python adaptor commmunication APIs are compatible with pytorch distributed communication APIs like allgather/allreduce and so on as a means of choice rather than an addition API group.*

### 1. Compatible APIs

The following Python APIs are supported by ZBAL from now, the equivalent form of C APIs is [here](../../raw_api/raw_api_communication.md).

##### 1. allgather

```python
torch.distributed.all_gather(tensor_list, tensor, group=None, async_op=False)
```
Gathers tensors from the whole group in a list.

| Parameters/return | In/Out | Description                   |
|-------------------|--------|-------------------------------|
| tensor_list       |   Out | Output list       |
|tensor | In | Tensor to be broadcast from current process |
|group|In|optional, The process group to work on|
|async_op|In|optional, Whether this op should be an async op, un-supported|

```python
torch.distributed.all_gather_into_tensor(output_tensor, input_tensor, group=None, async_op=False)
```
Gather tensors from all ranks and put them in a single output tensor.

| Parameters/return | In/Out | Description                   |
|-------------------|--------|-------------------------------|
| output_tensor       |   Out | Output tensor to accommodate tensor elements from all ranks       |
|input_tensor | In | Tensor to be broadcast from current process |
|group|In|optional, The process group to work on|
|async_op|In|optional, Whether this op should be an async op, un-supported|

#### 2. allreduce
```python
torch.distributed.all_reduce(tensor, op=<RedOpType.SUM: 0>, group=None, async_op=False)
```

Reduces the tensor data across all machines in a way that all get the final result.

| Parameters/return | In/Out | Description                   |
|-------------------|--------|-------------------------------|
| tensor       |   In/Out | Input and output of the collective. The function operates in-place.       |
|op | In | Optional,  One of the values from torch.distributed.ReduceOp enum. Specifies an operation used for element-wise reductions. |
|group|In|optional, The process group to work on|
|async_op|In|optional, Whether this op should be an async op, un-supported|

#### 3. alltoall
Split input tensor and then scatter the split list to all processes in a group.

```python
torch.distributed.all_to_all_single(output, input, output_split_sizes=None, input_split_sizes=None, group=None, async_op=False)
```

| Parameters/return | In/Out | Description                   |
|-------------------|--------|-------------------------------|
| output       |   Out | Gathered concatenated output tensor.      |
|input | In | Input tensor to scatter. |
|output_split_sizes|In|optional, Output split sizes for dim 0 if specified None or empty, dim 0 of output tensor must divide equally by world_size.|
|input_split_sizes|In|Input split sizes for dim 0 if specified None or empty, dim 0 of input tensor must divide equally by world_size.|
|group|In|optional, The process group to work on|
|async_op|In|optional, Whether this op should be an async op, un-supported|

#### 4. barrier
Synchronize all processes.

```python
torch.distributed.barrier(group=None, async_op=False, device_ids=None)[source]
```

| Parameters/return | In/Out | Description                   |
|-------------------|--------|-------------------------------|
|group|In|optional, The process group to work on|
|async_op|In|optional, Whether this op should be an async op, un-supported|
|device_ids|In|optional, List of device/GPU ids. Only one id is expected.|

#### 5. broadcast
Broadcasts the tensor to the whole group.

```python
torch.distributed.broadcast(tensor, src=None, group=None, async_op=False, group_src=None)
```

| Parameters/return | In/Out | Description                   |
|-------------------|--------|-------------------------------|
|tensor|In|Data to be sent if src is the rank of current process, and tensor to be used to save received data otherwise.|
|src|In|Source rank on global process group (regardless of group argument).|
|group|In|optional, The process group to work on|
|async_op|In|optional, Whether this op should be an async op, un-supported|
|group_src|In|Source rank on group. Must specify one of group_src and src but not both.|

#### 6. reduce_scatter

Reduces, then scatters a tensor to all ranks in a group.

```python
torch.distributed.reduce_scatter_tensor(output, input, op=<RedOpType.SUM: 0>, group=None, async_op=False)
```

| Parameters/return | In/Out | Description                   |
|-------------------|--------|-------------------------------|
| output       |   Out | Output tensor. It should have the same size across all ranks.       |
|input|In|Input tensor to be reduced and scattered. |
|op | In | Optional,  One of the values from torch.distributed.ReduceOp enum. Specifies an operation used for element-wise reductions. |
|group|In|optional, The process group to work on|
|async_op|In|optional, Whether this op should be an async op, un-supported|

#### 7. scatter

Scatters a list of tensors to all processes in a group.

```python
torch.distributed.scatter(tensor, scatter_list=None, src=None, group=None, async_op=False, group_src=None)
```

| Parameters/return | In/Out | Description                   |
|-------------------|--------|-------------------------------|
| tensor       |   Out | Output tensor.       |
|scatter_list |In|List of tensors to scatter (default is None, must be specified on the source rank)|
|src|In|Source rank on global process group (regardless of group argument). (If both src and group_src are None, default is global rank 0)|
|group|In|optional, The process group to work on|
|async_op|In|optional, Whether this op should be an async op, un-supported|
|group_src|In|Source rank on group. Must specify one of group_src and src but not both.|