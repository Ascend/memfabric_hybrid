## The ZBAL Allocator API

*Note: the zbal memory allocator is a implemention of pytorch pluggable allocator and satisfies the API specification.*

#### 1. zbal_sma_init

|                     |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| ------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| definition          | `int32_t zbal_sma_init(zbal_allocator_options_t *options, int32_t flags)`                                                                                                                                                                                                                                                                                                                                                                                                       |
| description         | Initialize the allocator with options, this is allocator is secondary memory allocator, the original memory is already allocated from Device, for example from mem fabric on Ascend. The allocator can be plugged into torch, any address allocated from this allocator can be accessed by other device directly, then we don't need to have COMM buffer to store the data temporarily. This action must be called before pluggable allocator automatic use zbal_pluggable_init |
| arguments - options | init allocator options                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| arguments - flags   | init flags, reserved                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| return              | success return 0 or else error code                                                                                                                                                                                                                                                                                                                                                                                                                                             |

#### 2. zbal_sma_uninit

|                   |                                       |
| ----------------- | ------------------------------------- |
| definition        | `void zbal_sma_uninit(int32_t flags)` |
| description       | un-initialize the allocator           |
| arguments - flags | un-init flags, reserved               |

#### 3. zbal_pluggable_init

|                          |                                                             |
| ------------------------ | ----------------------------------------------------------- |
| definition               | `void zbal_pluggable_init(int32_t device_count)`            |
| description              | Initialize API of official torch pluggable memory allocator |
| arguments - device_count | number of devices                                           |

#### 4. zbal_pluggable_malloc

|                    |                                                                                |
| ------------------ | ------------------------------------------------------------------------------ |
| definition         | `void *zbal_pluggable_malloc(size_t size, int32_t device, aclrtStream stream)` |
| description        | Allocate memory API of official torch pluggable memory allocator               |
| arguments - size   | size of memory to be allocated                                                 |
| arguments - device | device id                                                                      |
| arguments - stream | current stream                                                                 |

#### 5. zbal_pluggable_free

|                    |                                                                                        |
| ------------------ | -------------------------------------------------------------------------------------- |
| definition         | `void zbal_pluggable_free(void *ptr, size_t size, int32_t device, aclrtStream stream)` |
| description        | Free memory API of official torch pluggable memory allocator                           |
| arguments - ptr    | pointer allocated by zbal_torch_malloc                                                 |
| arguments - size   | size of memory                                                                         |
| arguments - device | device id                                                                              |
| arguments - stream | current stream                                                                         |

#### 6. zbal_pluggable_empty_cache

|                         |                                                              |
| ----------------------- | ------------------------------------------------------------ |
| definition              | `void zbal_pluggable_empty_cache(bool check_error)`          |
| description             | Empty cache API of official torch pluggable memory allocator |
| arguments - check_error | whether check on error allocate                              |

#### 7. zbal_simulate_init

|                  |                                                       |
| ---------------- | ----------------------------------------------------- |
| definition       | `void zbal_simulate_init(int64_t addr, int64_t size)` |
| description      | Simulate sma alloc for testing with out npu purpose   |
| arguments - addr | mock mem heap addr(will not use in simulate)          |
| arguments - size | mock mem heap size                                    |