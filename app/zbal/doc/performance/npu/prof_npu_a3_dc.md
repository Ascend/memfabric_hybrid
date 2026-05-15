## Dispatch/Combine communication performance

### Test environment

All testcase running on A3 super pod.

### 1 Dispatch Normal

*Note: Dispatch Normal kernel testcase results are test with single operation call.*

#### 1.1 With quant

|tokens| hidden | topk | experts | ranks | Avg Time(us) Over HCCL| Avg Time(us) Over ZBAL|
|---|---|---|---|---|---|---|
| 1024 | 7168  | 8  | 256   | 8  |  1280  | 1115   |
| 1024 | 7168  | 8  | 256   | 8  |  1767  | 1477   |
| 1024 | 7168  | 8  | 256   | 8  |  2816  | 2226   |
| 1024 | 7168  | 8  | 256   | 8  |  4887  | 3754   |
| 1024 | 7168  | 8  | 256   | 8  |  9377  | 6762   |

#### 1.2 Without quant

|tokens| hidden | topk | experts | ranks | Avg Time(us) Over HCCL| Avg Time(us) Over ZBAL|
|---|---|---|---|---|---|---|
| 1024 | 7168  | 8  | 256   | 8  |  1480  |  1472  |
| 1024 | 7168  | 8  | 256   | 8  |  2246  |  2235  |
| 1024 | 7168  | 8  | 256   | 8  |  3827  |  3739  |
| 1024 | 7168  | 8  | 256   | 8  |  6889  |  6740  |
| 1024 | 7168  | 8  | 256   | 8  |  13215  | 12804   |

### 2 Combine normal

*Note: Dispatch Normal kernel testcase results are test with single operation call.*

|tokens| hidden | topk | experts | ranks | Time(us) Over HCCL| Time(us) Over ZBAL|
|---|---|---|---|---|---|---|
| 1024 | 7168  | 8  | 256   | 8  | 1012  |  723  |
| 1024 | 7168  | 8  | 256   | 8  | 1942  |  1301  |
| 1024 | 7168  | 8  | 256   | 8  | 3773  |  2453  |
| 1024 | 7168  | 8  | 256   | 8  | 7639  |  4704  |
| 1024 | 7168  | 8  | 256   | 8  | 16930 | 9259   |

### 3 Dispatch low latency(With Quant)

|模型|experts|topk|hidden|ranks|batch size|Time(us) Over HCCL| Time(us) Over ZBAL|
|---|---|---|---|---|---|---|---|
| Deepseek3.1  | 256 | 8 | 7168 | 16 | 80   | 99.4 | 80.7 |
| Qwen235B     | 128 | 8 | 4096 | 16 | 144  | 122.0 | 79.8 |

### 4 Combine low latency(Without Quant)

|模型|experts|topk|hidden|ranks|batch size|Time(us) Over HCCL| Time(us) Over ZBAL|
|---|---|---|---|---|---|---|---|
| Deepseek3.1  | 256 | 8 | 7168 | 16 | 80   | 91.8 | 82.9 |
| Qwen235B     | 128 | 8 | 4096 | 16 | 144  | 98.0 | 82.8 |