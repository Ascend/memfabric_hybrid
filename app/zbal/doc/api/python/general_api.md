## General Python API
[TOC]

### 1 Functions

#### 1.1 zbal_version

##### Description

The version of zbal library, formatted MAJOR_VERSION.MINOR_VERSION.FIX, for example 1.1.0

##### Function definition

```python
def zbal_version()
```

##### Description of parameters and return value

| Parameters/return | In/Out | Description                   |
| ----------------- | ------ | ----------------------------- |
| return            |        | string of version, e.g. 0.1.0 |

##### Alternatives

Get version from global variable as following

```python
__version__
```

#### 1.2 zbal_init

##### Description

Initialize the zbal library before call any operators

##### Function definition

```python
def zbal_init(world_size: int,
              device_id: int,
              rank_id: int,
              device_mem_size: int,
              bootstrap_type: ZBALBootstrapType = ZBALBootstrapType.BOOT_BY_MEMFABRIC,
              start_config_server: bool = False,
              data_op_type: int = 0,
              comm_meta_space_size: int = 1024,
              comm_group_cap: int = 64,
              flags: int = 0,
              ip_port: str = "")
```

##### Description of parameters and return value

| Parameters/return    | In/Out | Description                                          |
| -------------------- | ------ | ---------------------------------------------------- |
| world_size           | in     | size of ranks to init zbal                           |
| device_id            | in     | current device id                                    |
| rank_id              | in     | current rank id                                      |
| device_mem_size      | in     | used device mem per device                           |
| bootstrap_type       | in     | under memory bootstrap type, memfabric support only  |
| start_config_server  | in     | whether to start config server                       |
| data_op_type         | in     | data operator type                                   |
| comm_meta_space_size | in     | collective communication meta space size, unit is KB |
| comm_group_cap       | in     | number of collective communication                   |
| flag                 | in     | reserve flag                                         |
| ip_port              | in     | bootstrap used ip port                               |
| return               | in     | 0 if success else error code                         |

#### 1.3 zbal_uninit

##### Description

Un-initialize zbal library

##### Function definition

```python
def zbal_uninit(flags: int = 0)
```

##### Description of parameters and return value

| Parameters/return | In/Out | Description    |
| ----------------- | ------ | -------------- |
| flags             | in     | optional flags |

### 2. Base Data Types

#### 2.1.1 ZBALBootstrapType (enum)

##### Description

Bootstrap type, i.e. there could be multiple kind of bootstrap, which to build the global virtual memory space.

##### Enum details

| Enum Type         | Description                                   |
| ----------------- | --------------------------------------------- |
| BOOT_BY_MEMFABRIC | build symm memory pool using memfabric_hybrid |

#### 2.2.2 ZBALCommProperty

##### Description

The property of communicator, we get it after communicator created

##### Class details

| Member                       | Description                                             |
| ---------------------------- | ------------------------------------------------------- |
| backendType                  | backend type, see ZBALBackendType                       |
| isWorldGroup                 | if this is the world group, 1 means true, 0 means false |
| groupSize                    | the number of ranks in total                            |
| groupRankId                  | my rank id in the world                                 |
| symmetricMetaGva             | symmetric memory for meta area                          |
| myGVA                        | gva of this rank in world                               |
| myMetaGVA                    | gva of this rank in group                               |
| myMetaGVAForOpParam          | gva for operation param on device in meta area          |
| myMetaGVAForOpExchange       | gva for address exchange in meta area                   |
| sizeOfMetaArea               | device memory size of meta area                         |
| sizeOfMetaForOpParam         | device memory size of operation param in meta area      |
| sizeOfMetaForAddressExchange | device memory size of address exchange area in meta     |
| localDeviceMemSize           | device memory size of this rank                         |
| name                         | name of the comm object                                 |
