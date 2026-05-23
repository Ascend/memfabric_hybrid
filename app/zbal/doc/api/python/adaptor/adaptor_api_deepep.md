## Python APIs for DeepEP

*Note: ZBAL python deepep APIs are compatible with original deepep APIs as a means of choice rather than an addition API group.*

### 1. Compatible APIs

##### 1. Config

Config is a data class to hold deepep configurations.

1. Member variable

    | 变量名称                          | 数据类型 | 含义                                                |
    | -------------------------------- | -------- | -------------------------------------------------- |
    | num_sms                          | int      | the SMs used in high-throughput kernels.(not used) |
    | num_max_nvl_chunked_send_tokens  | int      | not used                                           |
    | num_max_nvl_chunked_recv_tokens  | int      | not used                                           |
    | num_max_rdma_chunked_send_tokens | int      | not used                                           |
    | num_max_rdma_chunked_recv_tokens | int      | not used                                           |

1. Member function

    ```python
    def get_nvl_buffer_size_hint(hidden_bytes, num_ranks)
    # description: compatible function, empty operation, not used

    def get_rdma_buffer_size_hint(hidden_bytes, num_ranks)
    # rdescription: compatible function, empty operation, not used
    ```

##### 2. EventHandle

1. Member function

    ```python
    def current_stream_wait()
    # empty function
    ```

##### 3. Buffer

1. Member function

    ```python
    def is_available()
    # return the buffer available bool state
    ```

    ```python
    def get_num_rdma_ranks()
    # return the number of ranks in internode condition
    ```

    ```python
    def get_rdma_rank()
    # return the rank number in internode condition
    ```

    ```python
    def get_dispatch_layout(topk_idx,                   # the topk id tensor, shape=[num_tokens, topk]
                            num_experts,                # the number of experts
                            previous_event,             # not used
                            async_op,                   # not used
                            allocate_on_comm_stream)    # not used
    #
    # description: get the number of tokens per rank/expert
    #
    # return num_tokens_per_rank      : the number of tokens per rank, shape=[num_ranks]
    # return num_tokens_per_rdma_rank : not used
    # return num_tokens_per_expert    : the number of tokens per expert, shape=[num_experts]
    # return is_token_in_rank         : the flags(0/1) to mark whether the token will send to the rank, shape=[num_tokens, num_ranks]
    # return event                    : not used
    #
    ```

    ```python
    def intranode_dispatch(x,                       # the input tensor, shape=[num_tokens, hidden]
                           x_scales,                # not used
                           topk_idx,                # the topk id tensor, shape=[num_tokens, topk]
                           topk_weights,            # not used
                           num_tokens_per_rank,     # the number of tokens per rank, the same with get_dispatch_layout first out param
                           is_token_in_rank,        # not used
                           num_tokens_per_expert,   # the number of tokens per expert, the same with get_dispatch_layout third out param
                           num_worst_tokens,        # not used
                           config,                  # not used
                           previous_event,          # not used
                           async_op,                # not used
                           allocate_on_comm_stream, # not used
                           use_quant)               # do quant casting flag, boolean type

    #
    # description: Dispatch tokens to dest ranks, used for prefill
    #
    # return expandx_out                     : receive all other rank tokens tensor, shape=[num_recv_tokens, hidden]
    # return dynamic_scales_out              : if enable quant casting, shape=[num_recv_tokens], which means the quant casting scales per token
    # return recv_topk_idx                   : not used
    # return recv_topk_weight                : not used
    # return num_recv_tokens_per_expert_list : the number of tokens received of each local expert, the list size is num_local_experts
    # return put_offset                      : all other rank's first token write to the expert offset, tensor shape=[num_ranks, num_experts]
    # return balance_matrix                  : the token range processed by each rank after balanced, tensor shape=[num_ranks, ranks * 2]
    # return event                           : not used
    ```

    ```python
    def intranode_combine(x,                       # result from intranode_dispatch's expandx_out out param after expert mm computation
                          topk_idx,                # the topk id tensor, shape=[num_tokens, topk]
                          topk_weights,            # the topk weights tensor, shape=[num_tokens, topk]
                          put_offset,              # the same with intranode_dispatch out param put_offset
                          balance_matrix,          # the same with intranode_dispatch out param balance_matrix
                          previous_event,          # not used
                          async_op,                # not used
                          allocate_on_comm_stream) # not used
    #
    # description: Combine (reduce) tokens (addition **without** weights) from different ranks, used for prefill
    #
    # return combined_x        : combined output tensor, shape=[num_tokens, hidden]
    # return recv_topk_weights : not used
    # return event             : not used
    ```

    ```python
    def low_latency_dispatch(x,                                  # the input tensor
                             topk_idx,                           # the topk id tensor
                             cumulative_local_expert_recv_stats, # not used
                             num_max_dispatch_tokens_per_rank,   # the maximum number of tokens to dispatch
                             num_experts,                        # the number of all experts
                             use_fp8,                            # whether to enable FP8 casting
                             round_scale,                        # not used
                             use_ue8m0,                          # not used
                             async_op,                           # not used
                             return_recv_hook)                   # not used
    #
    # description: A low-latency implementation for dispatch, used for decode
    #
    # packed_recv_x        : receive tokens tensor
    # packed_recv_x_scales : the quant casting scales if enable quant casting
    # packed_recv_count    : receive token count
    # expandIdx            : receive token expert index
    # ep_recv_count        : ranks's receive token per expert
    # event                : not used
    # func                 : not used
    ```

    ```python
    def low_latency_combine(x,                                # result from low_latency_dispatch's packed_recv_x after expert computing
                            topk_idx,                         # the topk index tensor
                            topk_weights,                     # the topk weights tensor
                            src_info,                         # the topk index tensor
                            layout_range,                     # the expert send counts
                            num_max_dispatch_tokens_per_rank, # not used
                            num_experts,                      # number experts
                            packed_recv_count,                # not used
                            zero_copy,                        # not used
                            async_op,                         # not used
                            return_recv_hook,                 # not used
                            out)                              # not used
    #
    # description: A low-latency implementation for combine, used for decode
    #
    # return combined_x : combined output tensor
    # return event      : not used
    # return func       : not used
    ```

    ```python
    def clean_low_latency_buffer(num_max_dispatch_tokens_per_rank, # not used
                                 hidden,                           # not used
                                 num_experts)                      # not used
    #
    # description: compatible function, empty operation
    ```

##### 4. get_low_latency_rdma_size_hint

```python
def get_low_latency_rdma_size_hint(num_max_dispatch_tokens_per_rank, hidden, num_ranks, num_experts)
# description: compatible function, empty operation, return num_max_dispatch_tokens_per_rank directly
```


