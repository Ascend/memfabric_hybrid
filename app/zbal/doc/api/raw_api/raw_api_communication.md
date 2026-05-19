## C API for communications

[TOC]

## 1 Communicator management

### 1.1 Create communicator

#### Functionality description

Create a communicator after bootstrapped

#### Function definition

```c
int32_t zbal_comm_create(zbal_comm_options_t *options, zbal_comm_t *comm)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                            |
|-------------------|--------|----------------------------------------|
| options           | in     | options for communicator to be created |
| comm              | out    | communicator created                   |
| return            |        | 0 if successful                        |

### 1.2 Get the properties of a communicator

#### Functionality description

Get the properties of a communicator

#### Function definition

```c
int32_t zbal_comm_get_property(zbal_comm_t comm, zbal_comm_property_t *property)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                        |
|-------------------|--------|------------------------------------|
| comm              | in     | communicator handle                |
| property          | out    | properties of communicator created |
| return            |        | 0 if successful                    |

### 1.3 Get the world communicator

#### Functionality description

Get the world communicator

#### Function definition

```c
zbal_comm_t zbal_comm_get_global()
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                   |
|-------------------|--------|-------------------------------|
| return            |        | pointer of world communicator |

### 1.4 Get the communicator by name

#### Functionality description

Get the communicator by the name. Each communicator has unique name, user can get communicator handle by name.

#### Function definition

```c
zbal_comm_t zbal_comm_get_by_name(const char *name)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                     |
|-------------------|--------|---------------------------------|
| name              | in     | unique name of the communicator |
| return            |        | pointer of world communicator   |

### 1.5 Destroy a communicator

#### Functionality description

Destroy a communicator

#### Function definition

```c
int32_t zbal_comm_destroy(zbal_comm_t comm, uint32_t flags)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                |
|-------------------|--------|----------------------------|
| comm              | in     | handle of the communicator |
| flags             | in     | optional flags             |
| return            |        | 0 if successful            |

### 1.6 Destroy all communicators

#### Functionality description

Destroy all communicators

#### Function definition

```c
void zbal_comm_destroy_all(uint32_t flags)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description     |
|-------------------|--------|-----------------|
| flags             | in     | optional flags  |
| return            |        | 0 if successful |

## 2 Dispatch/Combine Normal

### 2.1 Dispatch Layout

#### Functionality description

Calculate the layout required for communication

#### Function definition

```c
int32_t zbal_dispatch_normal_layout(const zbal_tensor_info_t *topkIndex,
                                    int64_t tokens,
                                    int64_t expertNum,
                                    int64_t topkNum,
                                    const zbal_tensor_info_t *tokensPerRank,
                                    const zbal_tensor_info_t *tokensPerExpert,
                                    const zbal_tensor_info_t *isTokenInRank,
                                    const zbal_tensor_info_t *sendTokensIndex,
                                    const zbal_tensor_info_t *notifySendData,
                                    zbal_comm_t comm,
                                    aclrtStream stream,
                                    int64_t flags)
```
#### Description of parameters and return value

| Parameters/return | In/Out | Description     |
|-------------------|--------|-----------------|
| topkIndex            |in| the topk id tensor, shape=[num_tokens, topk]|
| tokens               |in| the number of tokens|
| expertNum            |in| the number of experts|
| topkNum              |in| the topk value|
| tokensPerRank        |in/out| the number of tokens to be sent to each rank statistics by local tokens, shape=[num_ranks]|
| tokensPerExpert      |in/out| the number of tokens to be sent to each expert statistics by local tokens, shape=[num_experts]|
| isTokenInRank        |in/out| whether the token to be sent to a rank flag(0/1), shape=[num_tokens, num_ranks]|
| sendTokensIndex      |in/out| local token send to target expert index, shape=[num_tokens, topk]|
| notifySendData       |in/out| temp buffer for the operation to caculate, shape=[50 * num_experts]|
| comm                 |in| zbal communication handle|
| stream               |in| compute stream|
| flags                |in| optional flags, reserved or extend|
| return            |        | 0 if successful |


### 2.2 Dispatch Notify

#### Functionality description

Calculate the number of tokens sent to each rank and other info

#### Function definition

```c
int32_t zbal_dispatch_normal_notify(const zbal_tensor_info_t *sendTokensPerExpert,
                                    int64_t sendCount,
                                    int64_t topKNum,
                                    const zbal_tensor_info_t *recvBuff,
                                    const zbal_tensor_info_t *totalRecvTokens,
                                    const zbal_tensor_info_t *recvTokensPerExpert,
                                    const zbal_tensor_info_t *pushTargetOffset,
                                    const zbal_tensor_info_t *balanceMatrix,
                                    zbal_comm_t comm,
                                    aclrtStream stream,
                                    int64_t flags);
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description     |
|-------------------|--------|-----------------|
|sendTokensPerExpert  |in| the same with zbal_dispatch_normal_layout 'tokensPerExpert' param|
|sendCount            |in| the number of experts |
|topKNum              |in| the topk value|
|recvBuff             |in| allgather ranks 'tokensPerExpert', shape=[num_ranks, num_experts]|
|totalRecvTokens      |in/out| local rank total recv token number, shape=[1]|
|recvTokensPerExpert  |in/out| the number of tokens received by each local expert, shape=[num_local_experts]|
|pushTargetOffset     |in/out| all other rank's first token write to the expert offset, tensor shape=[num_ranks, num_experts] |
|balanceMatrix        |in/out| the token range processed by each rank after balanced, tensor shape=[num_ranks, ranks * 2]|
|comm                 |in| zbal communication handle|
|stream               |in| stream|
|flags                |in| optional flags, reserved or extend|
| return            |        | 0 if successful |

### 2.3 Dispatch Normal

#### Functionality description

Dispatch operation

#### Function definition

```c
int32_t zbal_dispatch_normal(const zbal_tensor_info_t *srcTokens,
                             const zbal_tensor_info_t *topkIndex,
                             const zbal_tensor_info_t *sendTokensIndex,
                             const zbal_tensor_info_t *pushTargetOffset,
                             const zbal_tensor_info_t *balanceMatrix,
                             int64_t expertNum,
                             zbal_quant_mode_t quantMode,
                             const zbal_tensor_info_t *destTokens,
                             const zbal_tensor_info_t *destScale,
                             zbal_comm_t comm,
                             aclrtStream stream,
                             int64_t flags);
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description     |
|-------------------|--------|-----------------|
|srcTokens            |in| the input tensor, shape=[num_tokens, hidden]|
|topkIndex            |in| the topk id tensor, shape=[num_tokens, topk]|
|sendTokensIndex      |in| the same with zbal_dispatch_normal_layout out param 'sendTokensIndex'|
|pushTargetOffset     |in| the same with zbal_dispatch_normal_notify out param 'pushTargetOffset'|
|balanceMatrix        |in| the same with zbal_dispatch_normal_notify out param 'balanceMatrix'|
|expertNum            |in| the number of experts|
|quantMode            |in| quant mode, see struct 'zbal_quant_model_t'|
|destTokens           |in/out| receive all other rank tokens tensor, shape=[num_recv_tokens, hidden]|
|destScale            |in/out| if enable quant casting, shape=[num_recv_tokens], which means the quant casting scales per token|
|comm                 |in| zbal communication handle|
|stream               |in| stream|
|flags                |in| optional flags, reserved or extend|
| return            |        | 0 if successful |

### 2.4 Combine Normal

#### Functionality description

Combine operation

#### Function definition

```c
int32_t zbal_combine_normal(const zbal_tensor_info_t *srcTokens,
                            const zbal_tensor_info_t *srcTokensPerEp,
                            const zbal_tensor_info_t *topKWeight,
                            const zbal_tensor_info_t *topkIndex,
                            const zbal_tensor_info_t *sendTokensIndex,
                            const zbal_tensor_info_t *balanceMatrix,
                            uint16_t expertNum,
                            const zbal_tensor_info_t *destTokens,
                            zbal_comm_t comm,
                            aclrtStream stream,
                            int64_t flags);
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description     |
|-------------------|--------|-----------------|
|srcTokens            |in| result from zbal_dispatch_normal's destTokens out param after expert mm computation|
|srcTokensPerEp       |in| the same with zbal_dispatch_normal_notify out param 'pushTargetOffset'|
|topKWeight           |in| the topk weights tensor, shape=[num_tokens, topk]|
|topkIndex            |in| the topk id tensor, shape=[num_tokens, topk]|
|sendTokensIndex      |in| the same with zbal_dispatch_normal_layout out param 'sendTokensIndex'|
|balanceMatrix        |in| the same with zbal_dispatch_normal_notify out param 'balanceMatrix'|
|expertNum            |in| the number of experts|
|destTokens           |in/out| the result combined tensor, the shape is same with zbal_dispatch_normal in param 'srcTokens'|
|comm                 |in| zbal communication handle|
|stream               |in| stream|
|flags                |in| optional flags, reserved or extend|
| return            |        | 0 if successful |



## 3 Dispatch/Combine Low Latency

### 3.1 dispatch low latency

#### Functionality description

Dispatch operation for low latency

#### Function definition

```c
int32_t zbal_dispatch_low_latency(const zbal_tensor_info_t *x,
                                  const zbal_tensor_info_t *expertIds,
                                  int64_t moeExpertNum,
                                  int64_t sharedExpertNum,
                                  int64_t sharedExpertRankNum,
                                  int64_t quantMode,
                                  int64_t globalBs,
                                  int64_t magicVal,
                                  int64_t expertTokenNumsType,
                                  const zbal_tensor_info_t *expandXOut,
                                  const zbal_tensor_info_t *dynamicScalesOut,
                                  const zbal_tensor_info_t *expandIdxOut,
                                  const zbal_tensor_info_t *expertTokenNumsOut,
                                  const zbal_tensor_info_t *epRecvCountsOut,
                                  const zbal_tensor_info_t *putOffset,
                                  const zbal_tensor_info_t *putOffsetStatus,
                                  zbal_comm_t comm,
                                  aclrtStream stream,
                                  int64_t flags)
```

#### Description of parameters and return value

| Parameters/return     | In/Out | Description     |
|-------------------    |--------|-----------------|
| x                     | in |   tensor info of source tokens to be dispatched    |
|  expertIds            | in |   the token's expert ids    |
| moeExpertNum          | in |   the expert number          |
|  sharedExpertNum      | in |   the share expert number       |
| sharedExpertRankNum   | in |   the share expert rank number  |
| quantMode             | in |   quant mode                    |
| globalBs              | in |   global batch size           |
| magicVal              | in |   magic value           |
| expertTokenNumsType   | in |   expert token number type   |
| expandXOut            | in |   the output tensor          |
| dynamicScalesOut      | in |   the output quant scales        |
| expandIdxOut          | in |   the output index           |
| expertTokenNumsOut    | in |   the expert output token number |
| epRecvCountsOut       | in |   ep rank receive token count   |
| putOffset             | in |   the token put offset to rank expert |
| putOffsetStatus       | in |   put offset flag            |
|  comm                 | in |   the communicator handle           |
| stream                | in |   stream to run             |
|  flags                | in |   reserved flags          |
| return                |        | 0 if successful |


### 3.2 combine low latency

#### Functionality description

Combine operation for low latency

#### Function definition

```c
int32_t zbal_combine_low_latency(const zbal_tensor_info_t *expandX,
                                 const zbal_tensor_info_t *expertIds,
                                 const zbal_tensor_info_t *expertIdx,
                                 const zbal_tensor_info_t *epSendCounts,
                                 const zbal_tensor_info_t *expertScales,
                                 const zbal_tensor_info_t *xOut,
                                 int64_t moeExpertNum,
                                 zbal_comm_t comm,
                                 aclrtStream stream,
                                 int64_t flags)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description     |
|-------------------|--------|-----------------|
| expandX           |  in    |  the combine input tensor   |
| expertIds         |  in    |  the expert ids list        |
| expertIdx         |  in    |  the expert index list      |
| epSendCounts      |  in    |  ep rank send count         |
| expertScales      |  in    |  the expert of token scales  |
| xOut              |  in    |  the combine output tensor   |
| moeExpertNum      |  in    |  expert number               |
| comm              |  in    |  the communicator handle     |
| stream            |  in    |  the stream to run        |
| flags             |  in    |  flags, reserved        |
| return            |        | 0 if successful |

## 4 AllGather

#### Functionality description

AllGather operation

#### Function definition

```c
int32_t zbal_all_gather(const void *sendBuff,
                        void *recvBuff,
                        size_t send_count,
                        zbal_datatype_t dataType,
                        zbal_comm_t comm,
                        aclrtStream stream)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                 |
|-------------------|--------|-----------------------------|
| send_buff         | in     | pointer of source data      |
| recv_buff         | out    | pointer of destination data |
| send_count        | in     | count of the source data    |
| data_type         | in     | type of the data            |
| comm              | in     | handle of the communicator  |
| stream            | in     | stream                      |
| return            |        | 0 if successful             |

## 5 AllReduce

#### Functionality description

AllReduce operation

#### Function definition

```c
int32_t zbal_all_reduce(const void *send_buff,
                        void *recv_buff,
                        void *buffer,
                        size_t count,
                        size_t buf_cnt,
                        zbal_datatype_t data_type,
                        zbal_reduce_op_t op,
                        zbal_comm_t comm,
                        aclrtStream stream)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                 |
|-------------------|--------|-----------------------------|
| send_buff         | in     | pointer of source data      |
| recv_buff         | out    | pointer of destination data |
| buffer            | in     |                             |
| count             | in     | count of the data           |
| buf_cnt           | in     |                             |
| data_type         | in     | type of the data            |
| op                | in     | type of reduce operation    |
| comm              | in     | handle of the communicator  |
| stream            | in     | stream                      |
| return            |        | 0 if successful             |

## 6 ReduceScatter

#### Functionality description

ReduceScatter operation

#### Function definition

```c
int32_t zbal_reduce_scatter(const void *sendBuff,
                            void *recvBuff,
                            size_t recv_count,
                            zbal_datatype_t dataType,
                            zbal_reduce_op_t op,
                            zbal_comm_t comm,
                            aclrtStream stream)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                 |
|-------------------|--------|-----------------------------|
| send_buff         | in     | pointer of source data      |
| recv_buff         | out    | pointer of destination data |
| count             | in     | count of the data           |
| data_type         | in     | type of the data            |
| op                | in     | type of reduce operation    |
| comm              | in     | handle of the communicator  |
| stream            | in     | stream                      |
| return            |        | 0 if successful             |

## 7 Braodcast

#### Functionality description

Braodcast operation

#### Function definition

```c
int32_t zbal_broadcast(const void *buf,
                       uint64_t data_count,
                       zbal_datatype_t dataType,
                       uint16_t root,
                       zbal_comm_t comm,
                       aclrtStream stream)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                     |
|-------------------|--------|---------------------------------|
| buf               | in     | pointer of source data          |
| data_count        | in     | count of the data               |
| dataType          | in     | type of the data                |
| root              | in     | the root rank in the operator   |
| comm              | in     | handle of the communicator      |
| stream            | in     | stream                          |
| return            |        | 0 if successful                 |

## 8 Scatter

#### Functionality description

Scatter operation

#### Function definition

```c
int32_t zbal_scatter(const void *sendBuf,
                     void *recvBuf,
                     uint64_t data_count,
                     zbal_datatype_t dataType,
                     uint16_t root,
                     zbal_comm_t comm,
                     aclrtStream stream)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                     |
|-------------------|--------|---------------------------------|
| sendBuff          | in     | pointer of source data      |
| recvBuff          | out    | pointer of destination data |
| data_count        | in     | count of the data               |
| dataType          | in     | type of the data                |
| root              | in     | the root rank in the operator   |
| comm              | in     | handle of the communicator      |
| stream            | in     | stream                          |
| return            |        | 0 if successful                 |

## 9 AlltoAllv

#### Functionality description

Alltoall operation and allows input/output splits parameter

#### Function definition

```c
int32_t zbal_all_to_all_v(const void *sendBuff,
                          void *recvBuff,
                          void *sendCumSum,
                          void *recvSplitCounts,
                          void *elements,
                          zbal_datatype_t dataType,
                          zbal_comm_t comm,
                          aclrtStream stream)

| Parameters/return | In/Out | Description                     |
|-------------------|--------|---------------------------------|
| sendBuff          | in     | pointer of source data      |
| recvBuff          | out    | pointer of destination data |
| sendCumSum        | in     | count of the data               |
| recvSplitCounts   | in     | output splits in elemtents      |
| elements          | in     | input and output total elements |
| dataType          | in     | the data type                   |
| comm              | in     | handle of the communicator      |
| stream            | in     | stream                          |
| return            |        | 0 if successful                 |
```