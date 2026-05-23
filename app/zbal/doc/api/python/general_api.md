## General Python API

### 1. Base Data Types

#### 1. enums

|||
|---|---|
|enum - `ZBALBootstrapType`| bootstrap under resource type|
|member - BOOT_BY_MEMFABRIC|build symm memory pool using memfabric_hybrid|


|||
|---|---|
|enum - `ZBALBackendType`| init zbal backend type, only support NPU now|
|member - ZBAL_ASCEND_NPU|init zbal backend with NPU resources|


#### 2. classes

1. the `ZBALBootstrapOption` class.

    |||
    |---|---|
    |class - `ZBALBootstrapOption`|zbal boostrap options |
    |member - flags| resreved, un-used now|
    |member - btType| bootstrap type, see ZBALBootstrapType |
    |member - worldSize| zbal working process size|
    |member - rankId| current rank id in global nodes view|
    |member - deviceId| current rank id in local node view|
    |member - startConfigServer|optional, if start config store server, 1 means start, 0 means not start  |
    |member - deviceMemorySize|memory size will allocate  |
    |member - dataOperationType|optional, data operation type |
    |member - commMetaSpaceSize|optional, single meta size in KB, default 1MB, min: 512KB, max: 4MB |
    |member - commGroupCap|optional, max count of comm Group, default 128, min: 1, max: 256  |
|member - ipPort|optional, config store server ip:port |


1. the `ZBALCommProperty` class.

    |||
    |---|---|
    |class - `ZBALCommProperty`|zbal comm properties |
    |member - backendType | backend type, see ZBALBackendType|
    |member - isWorldGroup| if this is the world group, 1 means true, 0 means false |
    |member - groupSize| the number of ranks in total |
    |member - groupRankId| my rank id in the world |
    |member - symmetricMetaGva| symmetric memory for meta area |
    |member - myGVA| gva of this rank in world  |
    |member - myMetaGVA| gva of this rank in group |
    |member - myMetaGVAForOpParam|gva for operation param on device in meta area  |
    |member - myMetaGVAForOpExchange| gva for address exchange in meta area |
    |member - sizeOfMetaArea| device memory size of meta area |
    |member - sizeOfMetaForOpParam|device memory size of operation param in meta area  |
    |member - sizeOfMetaForAddressExchange| device memory size of address exchange area in meta |
    |member - localDeviceMemSize| device memory size of this rank  |
    |member - name| name of the comm object |

1. the `Options` class.

    |||
    |---|---|
    |class - `Options`|zbal init comm options |
    |member - op_timeout| the timeout for op running |
    |member - is_high_priority_stream| whether running stream is high priority, un-used|
    |member - global_ranks_in_group| the rank list in current comm group  |
    |member - group_id| the group id, un-used |


1. the `ProcessGroupZBAL` class.

    |||
    |---|---|
    |class - `ProcessGroupZBAL`|zbal process group |
    |func - constructor | receive store/rank_id/rank_size/ZOptions as input arguments|
    |func - get_zbal_comm_name() | return the zbal comm instance name|
    |func - get_hccl_comm_name() | a compatible function for hccl, the same with get_zbal_comm_name() |
    |func - init_zbal_comm_meta() | init the zbal comm resource if comm is not ready |

