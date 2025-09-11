# Memcache API

## C接口列表
C语言接口功能齐全，包含metaservice/localservice服务启停接口，客户端初始化和反初始化接口，数据操作接口和日志设置接口。

### 1. 服务启动停止接口

#### mmcs_meta_service_start
```c
mmc_meta_service_t mmcs_meta_service_start(mmc_meta_service_config_t *config);
```
**功能**: 启动分布式内存缓存的元数据服务，该服务是全局的。

**参数**:
- `config`: 元数据服务配置信息

**返回值**:
- `mmc_meta_service_t`: 元数据服务句柄（成功），NULL（失败）

#### mmcs_meta_service_stop
```c
void mmcs_meta_service_stop(mmc_meta_service_t handle);
```
**功能**: 停止分布式内存缓存的元数据服务。

**参数**:
- `handle`: 元数据服务句柄

#### mmcs_local_service_start
```c
mmc_local_service_t mmcs_local_service_start(mmc_local_service_config_t *config);
```
**功能**: 启动分布式内存缓存的本地服务，负责本地内存对象的管理。

**参数**:
- `config`: 本地服务配置信息

**返回值**:
- `mmc_local_service_t`: 本地服务句柄（成功），NULL（失败）

#### mmcs_local_service_stop
```c
void mmcs_local_service_stop(mmc_local_service_t handle);
```
**功能**: 停止分布式内存缓存的本地服务。

**参数**:
- `handle`: 本地服务句柄

### 2. 客户端初始化和反初始化

#### mmcc_init
```c
int32_t mmcc_init(mmc_client_config_t *config);
```
**功能**: 初始化分布式内存缓存客户端，这是一个单例模式。

**参数**:
- `config`: 客户端配置信息

**返回值**:
- `0`: 成功
- 其他: 失败

#### mmcc_uninit
```c
void mmcc_uninit();
```
**功能**: 反初始化客户端，释放相关资源。

### 3. 数据操作接口

#### mmcc_put
```c
int32_t mmcc_put(const char *key, mmc_buffer *buf, mmc_put_options options, uint32_t flags);
```
**功能**: 将指定 key 的数据放入分布式内存缓存中。支持同步和异步操作。

**参数**:
- `key`: 数据的键，长度小于256字节
- `buf`: 要放入的数据缓冲区
- `options`: 放置操作的选项
- `flags`: 可选标志，保留字段

**返回值**:
- `0`: 成功
- 其他: 失败

#### mmcc_get
```c
int32_t mmcc_get(const char *key, mmc_buffer *buf, uint32_t flags);
```
**功能**: 从分布式内存缓存中获取指定 key 的数据。支持同步和异步操作。

**参数**:
- `key`: 数据的键，长度小于256字节
- `buf`: 存储获取数据的缓冲区
- `flags`: 可选标志，保留字段

**返回值**:
- `0`: 成功
- 其他: 失败

#### mmcc_query
```c
int32_t mmcc_query(const char *key, mmc_data_info *info, uint32_t flags);
```
**功能**: 查询分布式内存缓存中指定 key 的数据信息。支持同步和异步操作。

**参数**:
- `key`: 数据的键，长度小于256字节
- `info`: 存储数据信息的结构体
- `flags`: 可选标志，保留字段

**返回值**:
- `0`: 成功
- 其他: 失败

#### mmcc_remove
```c
int32_t mmcc_remove(const char *key, uint32_t flags);
```
**功能**: 从分布式内存缓存中删除指定 key 的对象。支持同步和异步操作。

**参数**:
- `key`: 数据的键，长度小于256字节
- `flags`: 可选标志，保留字段

**返回值**:
- `0`: 成功
- 其他: 失败

#### mmcc_exist
```c
int32_t mmcc_exist(const char *key, uint32_t flags);
```
**功能**: 判断指定 key 是否存在于分布式内存缓存中。

**参数**:
- `key`: 数据的键，长度小于256字节
- `flags`: 可选标志，保留字段

**返回值**:
- `0`: 成功
- 其他: 失败

#### mmcc_get_location
```c
mmc_location_t mmcc_get_location(const char *key, uint32_t flags);
```
**功能**: 获取对象的位置信息。此操作仅支持同步模式。

**参数**:
- `key`: 数据的键，长度小于256字节
- `flags`: 可选标志，保留字段

**返回值**:
- `mmc_location_t`: 对象的位置信息（如果存在）

#### mmcc_batch_query
```c
int32_t mmcc_batch_query(const char **keys, size_t keys_count, mmc_data_info *info, uint32_t flags);
```
**功能**: 批量查询指定 keys 的 blob 信息。

**参数**:
- `keys`: 数据键数组，每个键长度小于256字节
- `keys_count`: 键的数量
- `info`: 输出的键的 Blob 信息
- `flags`: 操作标志

**返回值**:
- `0`: 成功
- 其他: 失败

#### mmcc_batch_remove
```c
int32_t mmcc_batch_remove(const char **keys, uint32_t keys_count, int32_t *remove_results, uint32_t flags);
```
**功能**: 从 BM 中批量删除多个键。

**参数**:
- `keys`: 要从 BM 中删除的键列表
- `keys_count`: 键的数量
- `remove_results`: 每个删除操作的结果
- `flags`: 操作标志

**返回值**:
- `0`: 成功
- 正值: 发生错误
- 负值: 其他错误

#### mmcc_batch_exist
```c
int32_t mmcc_batch_exist(const char **keys, uint32_t keys_count, int32_t *exist_results, uint32_t flags);
```
**功能**: 判断多个键是否存在于 BM 中。

**参数**:
- `keys`: 数据键数组，每个键长度小于256字节
- `keys_count`: 键的数量
- `exist_results`: BM 中键的存在状态列表
- `flags`: 可选标志，保留字段

**返回值**:
- `0`: 成功
- 其他: 失败

#### mmcc_batch_put
```c
int32_t mmcc_batch_put(const char **keys, uint32_t keys_count, const mmc_buffer *bufs,
                       mmc_put_options& options, uint32_t flags);
```
**功能**: 批量将多个数据对象放入分布式内存缓存中。支持同步和异步操作。

**参数**:
- `keys`: 数据对象的键数组
- `keys_count`: 数组中键的数量
- `bufs`: 要放入的数据缓冲区数组
- `options`: 批量放置操作的选项
- `flags`: 可选标志，保留字段

**返回值**:
- `0`: 成功
- 其他: 失败

#### mmcc_batch_get
```c
int32_t mmcc_batch_get(const char **keys, uint32_t keys_count, mmc_buffer *bufs, uint32_t flags);
```
**功能**: 批量从分布式内存缓存中获取多个数据对象。支持同步和异步操作。

**参数**:
- `keys`: 数据对象的键数组
- `keys_count`: 数组中键的数量
- `bufs`: 存储检索数据的数据缓冲区数组
- `flags`: 可选标志，保留字段

**返回值**:
- `0`: 成功
- 其他: 失败

#### mmcc_wait
```c
int32_t mmcc_wait(int32_t waitHandle, int32_t timeoutSec);
```
**功能**: 等待异步操作完成。

**参数**:
- `waitHandle`: 由数据操作（如 mmcc_put、mmcc_get、mmcc_remove）创建的句柄
- `timeoutSec`: 等待超时时间（秒）

**返回值**:
- `0`: 成功
- `1`: 超时
- 正值: 发生错误

### 4. 日志设置接口

#### mmc_set_extern_logger
```c
int32_t mmc_set_extern_logger(void (*func)(int level, const char *msg));
```
**功能**: 设置外部日志函数，用户可以设置自定义的日志记录函数。在自定义的日志记录函数中，用户可以使用统一的日志工具，这样日志消息就可以写入与调用者相同的日志文件中。如果未设置，acc_links日志消息将被打印到标准输出。

日志级别说明：
- `0`: DEBUG
- `1`: INFO
- `2`: WARN
- `3`: ERROR

**参数**:
- `func`: 外部日志记录函数

**返回值**:
- `0`: 成功
- 其他: 失败

#### mmc_set_log_level
```c
int32_t mmc_set_log_level(int level);
```
**功能**: 设置日志打印级别。

**参数**:
- `level`: 日志级别，0:debug 1:info 2:warn 3:error

**返回值**:
- `0`: 成功
- 其他: 失败

## Python 接口列表
说明：Python接口只封装了客户端，需要配置metaservice独立进程使用

### DistributedObjectStore 类

DistributedObjectStore是分布式对象存储的Python接口封装类，提供了完整的分布式内存缓存操作接口。

#### \_\_init\_\_
```python
store = DistributedObjectStore()
```
**功能**: 创建DistributedObjectStore实例

#### init
```python
result = store.init(device_id)
```
**功能**: 初始化分布式内存缓存客户端

**参数**:
- `device_id`: 使用HBM时的NPU卡用户ID（支持ASCEND_RT_VISIBLE_DEVICES映射）

**返回值**:
- `0`: 成功
- 其他: 失败

#### close
```python
store.close()
```
**功能**: 关闭并清理分布式内存缓存客户端

#### put
```python
result = store.put(key, data)
```
**功能**: 将指定key的数据写入分布式内存缓存中

**参数**:
- `key`: 数据的键，字符串类型
- `data`: 要存储的字节数据

**返回值**:
- `0`: 成功
- 其他: 失败

#### put_from
```python
result = store.put_from(key, buffer_ptr, size, direct=SMEMB_COPY_H2G)
```
**功能**: 从预分配的缓冲区中写入数据

**参数**:
- `key`: 数据的键
- `buffer_ptr`: 缓冲区指针
- `size`: 数据大小
- `direct`: 数据拷贝方向，可选值：
  - `SMEMB_COPY_H2G`: 从主机内存到全局内存（默认）
  - `SMEMB_COPY_L2G`: 从卡上内存到全局内存

**返回值**:
- `0`: 成功
- 其他: 失败

#### batch_put_from
```python
result = store.batch_put_from(keys, buffer_ptrs, sizes, direct=SMEMB_COPY_H2G)
```
**功能**: 从预分配的缓冲区中批量写入数据

**参数**:
- `keys`: 键列表
- `buffer_ptrs`: 缓冲区指针列表
- `sizes`: 数据大小列表
- `direct`: 数据拷贝方向，可选值：
  - `SMEMB_COPY_H2G`: 从主机内存到全局内存（默认）
  - `SMEMB_COPY_L2G`: 从卡上内存到全局内存

**返回值**:
- 结果列表，每个元素表示对应写入操作的结果
  - `0`: 成功
  - 其他: 错误

#### put_from_layers
```python
result = store.put_from_layers(key, buffer_ptrs, sizes, direct=SMEMB_COPY_H2G)
```
**功能**: 从多个预分配的缓冲区中写入数据（分层数据）

**参数**:
- `key`: 数据的键
- `buffer_ptrs`: 缓冲区指针列表，每个指针对应一个数据层
- `sizes`: 数据大小列表，每个元素对应一个数据层的大小
- `direct`: 数据拷贝方向，可选值：
  - `SMEMB_COPY_H2G`: 从主机内存到全局内存（默认）
  - `SMEMB_COPY_L2G`: 从卡上内存到全局内存

**返回值**:
- `0`: 成功
- 其他: 失败

#### batch_put_from_layers
```python
result = store.batch_put_from_layers(keys, buffer_ptrs_list, sizes_list, direct=SMEMB_COPY_H2G)
```
**功能**: 从多个预分配的缓冲区中批量写入分层数据

**参数**:
- `keys`: 数据键列表，每个键对应一个分层数据对象
- `buffer_ptrs_list`: 缓冲区指针二维列表，外层列表对应每个键，内层列表对应每个键的各个数据层指针
- `sizes_list`: 数据大小二维列表，外层列表对应每个键，内层列表对应每个键的各个数据层大小
- `direct`: 数据拷贝方向，可选值：
  - `SMEMB_COPY_H2G`: 从主机内存到全局内存（默认）
  - `SMEMB_COPY_L2G`: 从卡上内存到全局内存

**返回值**:
- 结果列表，每个元素表示对应写入操作的结果
  - `0`: 成功
  - 其他: 错误

#### get
```python
data = store.get(key)
```
**功能**: 获取指定key的数据

**参数**:
- `key`: 数据的键

**返回值**:
- 成功时返回数据字节串
- 失败时返回空字节串

#### get_into
```python
result = store.get_into(key, buffer_ptr, size, direct=SMEMB_COPY_G2H)
```
**功能**: 将数据直接获取到预分配的缓冲区中

**参数**:
- `key`: 数据的键
- `buffer_ptr`: 目标缓冲区指针
- `size`: 缓冲区大小
- `direct`: 数据拷贝方向，可选值：
  - `SMEMB_COPY_G2H`: 从全局内存到主机内存（默认）
  - `SMEMB_COPY_G2L`: 从全局内存到卡上内存

**返回值**:
- `0`: 成功
- 其他: 失败

#### batch_get_into
```python
results = store.batch_get_into(keys, buffer_ptrs, sizes, direct=SMEMB_COPY_G2H)
```
**功能**: 批量将数据获取到预分配的缓冲区中

**参数**:
- `keys`: 键列表
- `buffer_ptrs`: 缓冲区指针列表
- `sizes`: 缓冲区大小列表
- `direct`: 数据拷贝方向，可选值：
  - `SMEMB_COPY_G2H`: 从全局内存到主机内存（默认）
  - `SMEMB_COPY_G2L`: 从全局内存到卡上内存

**返回值**:
- 结果列表，每个元素表示对应操作的结果
  - `0`: 成功
  - 其他: 错误

#### get_into_layers
```python
result = store.get_into_layers(key, buffer_ptrs, sizes, direct=SMEMB_COPY_G2H)
```
**功能**: 将数据分层获取到预分配的缓冲区中

**参数**:
- `key`: 数据的键
- `buffer_ptrs`: 目标缓冲区指针列表，每个指针对应一个数据层
- `sizes`: 缓冲区大小列表，每个元素对应一个数据层的大小
- `direct`: 数据拷贝方向，可选值：
  - `SMEMB_COPY_G2H`: 从全局内存到主机内存（默认）
  - `SMEMB_COPY_G2L`: 从全局内存到卡上内存

**返回值**:
- `0`: 成功
- 其他: 失败

#### batch_get_into_layers
```python
results = store.batch_get_into_layers(keys, buffer_ptrs_list, sizes_list, direct=SMEMB_COPY_G2H)
```
**功能**: 批量将分层数据获取到预分配的缓冲区中

**参数**:
- `keys`: 数据键列表，每个键对应一个分层数据对象
- `buffer_ptrs_list`: 缓冲区指针二维列表，外层列表对应每个键，内层列表对应每个键的各个目标数据层指针
- `sizes_list`: 缓冲区大小二维列表，外层列表对应每个键，内层列表对应每个键的各个数据层大小
- `direct`: 数据拷贝方向，可选值：
  - `SMEMB_COPY_G2H`: 从全局内存到主机内存（默认）
  - `SMEMB_COPY_G2L`: 从全局内存到卡上内存

**返回值**:
- 结果列表，每个元素表示对应操作的结果
  - `0`: 成功
  - 其他: 错误

#### remove
```python
result = store.remove(key)
```
**功能**: 删除指定key的数据

**参数**:
- `key`: 要删除的数据键

**返回值**:
- `0`: 成功
- 其他: 失败

#### remove_batch
```python
results = store.remove_batch(keys)
```
**功能**: 批量删除数据

**参数**:
- `keys`: 要删除的键列表

**返回值**:
- 结果列表，每个元素表示对应删除操作的结果
  - `0`: 成功
  - 其他: 错误

#### is_exist
```python
result = store.is_exist(key)
```
**功能**: 检查指定key是否存在

**参数**:
- `key`: 要检查的键

**返回值**:
- `1`: 存在
- `0`: 不存在
- 其他: 错误

#### batch_is_exist
```python
results = store.batch_is_exist(keys)
```
**功能**: 批量检查键是否存在

**参数**:
- `keys`: 要检查的键列表

**返回值**:
- 结果列表，每个元素表示对应键的存在状态：
  - `1`: 存在
  - `0`: 不存在
  - 其他: 错误

### MmcCopyDirect 枚举类型

用于指定数据拷贝方向的枚举类型：
- `SMEMB_COPY_H2G`: 从主机内存到全局内存
- `SMEMB_COPY_L2G`: 从卡上内存到全局内存
- `SMEMB_COPY_G2H`: 从全局内存到主机内存
- `SMEMB_COPY_G2L`: 从全局内存到卡上内存

## 数据结构

### mmc_client_config_t
客户端配置结构体，包含以下字段：
- `discoveryURL`: 发现服务URL
- `rankId`: Rank ID
- `timeOut`: 超时时间
- `logLevel`: 日志级别
- `logFunc`: 外部日志函数
- `tlsConfig`: TLS配置

### mmc_meta_service_config_t
元数据服务配置结构体，包含以下字段：
- `discoveryURL`: 发现服务URL
- `logLevel`: 日志级别
- `evictThresholdHigh`: 高水位驱逐阈值
- `evictThresholdLow`: 低水位驱逐阈值
- `tlsConfig`: TLS配置

### mmc_local_service_config_t
本地服务配置结构体，包含以下字段：
- `discoveryURL`: 发现服务URL
- `deviceId`: 设备ID
- `rankId`: BM全局统一编号
- `worldSize`: 世界大小
- `bmIpPort`: BM IP端口
- `bmHcomUrl`: BM HCOM URL
- `createId`: 创建ID
- `dataOpType`: 数据操作类型
- `localDRAMSize`: 本地DRAM大小
- `localHBMSize`: 本地HBM大小
- `flags`: 标志
- `tlsConfig`: TLS配置
- `logLevel`: 日志级别
- `logFunc`: 外部日志函数

### mmc_buffer
内存缓冲区结构体，包含内存地址、类型和维度信息。

### mmc_put_options
放置操作选项，包含媒体类型和亲和策略。

### mmc_data_info
数据信息结构体，包含大小、保护标志、blob数量和有效性标志。

### mmc_location_t
位置信息结构体，表示数据在内存中的位置。

### tls_config
TLS配置结构体，包含以下字段：
- `tlsEnable`: TLS启用标志
- `caPath`: 根证书文件路径
- `crlPath`: 证书吊销列表文件路径
- `certPath`: 证书文件路径
- `keyPath`: 私钥文件路径
- `keyPassPath`: 私钥加密密码（口令）文件路径
- `packagePath`: openssl动态库路径
- `decrypterLibPath`: 自定义口令解密算法动态库路径

## 错误码

| 值     | 说明        |
|-------|-----------|
| 0     | 操作成功      |
| -1    | 一般错误      |
| -3000 | 参数无效      |
| -3001 | 内存分配失败    |
| -3002 | 对象创建失败    |
| -3003 | 服务未启动     |
| -3004 | 操作超时      |
| -3005 | 重复调用      |
| -3006 | 对象已存在     |
| -3007 | 对象不存在     |
| -3008 | 未初始化      |
| -3009 | 网络序列号重复   |
| -3010 | 网络序列号未找到  |
| -3011 | 已通知       |
| -3013 | 超出容量限制    |
| -3014 | 连接未找到     |
| -3015 | 网络请求句柄未找到 |
| -3016 | 内存不足      |
| -3017 | 未连接到元数据服务 |
| -3018 | 未连接到本地服务  |
| -3019 | 客户端未初始化   |
| -3101 | 状态不匹配     |
| -3102 | 键不匹配      |
| -3103 | 返回值不匹配    |
| -3104 | 租约未到期     |

## 注意事项

- 所有键的长度必须小于256字节
- 支持同步和异步两种操作模式
- 批量操作可以提高处理效率