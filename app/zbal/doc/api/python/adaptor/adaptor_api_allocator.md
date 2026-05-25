## The ZBAL Allocator API

*Note: the allocator python APIs are all under package name* ***allocator***.

#### 1. record_memory_history

|                         |                                                                                               |
| ----------------------- | --------------------------------------------------------------------------------------------- |
| definition              | `record_memory_history(enabled, max_entries)`                                                 |
| description             | begin record memory with history                                                              |
| arguments - enabled     | enabled string, only "all" and "state" are allowed. For compatible "state" is same with "all" |
| arguments - max_entries | record max entry size                                                                         |


#### 2. get_heap_stats

|                    |                                                                   |
| ------------------ | ----------------------------------------------------------------- |
| definition         | `get_heap_stats(device)`                                          |
| description        | get heap usage stats                                              |
| arguments - device | the device id                                                     |
| return             | return (used_size, total_size), return zero if heap is not inited |

#### 3. dump_snapshot

|             |                                       |
| ----------- | ------------------------------------- |
| definition  | `dump_snapshot()`                     |
| description | dump the memory allocator snapshot    |
| return      | the memory alloc result pkl dict file |


#### 4. simulate_init

|                  |                                                          |
| ---------------- | -------------------------------------------------------- |
| definition       | `simulate_init(addr, size)`                              |
| arguments - addr | simulate init addr                                       |
| arguments - size | simulate init size                                       |
| description      | simulate_init on sma/dma heap, no actual memory allocate |

#### 4. is_mix_alloc

|             |                                               |
| ----------- | --------------------------------------------- |
| definition  | `is_mix_alloc()`                              |
| description | check whether allocator is using vmm mix mode |
| return      | boolean result for mix mode                   |