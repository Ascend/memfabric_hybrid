# 故障注入（FaultInjectionPoint）实现与使用说明

## 1. 背景

本地这版故障注入已经不是旧的 `MF_FAILPOINTS` 环境变量方案，而是重构成了一个通用的 FaultInjectionPoint 框架。

新方案的几个核心变化如下：

- 从“按环境变量解析 + 按 deviceId 匹配”改成了“进程内注册 + 配置文件激活 + 文件变更自动重载”
- 从“只有 fail/sleep”扩展成了统一的 `callback` / `pause` / `reset` / `abort` 四种动作
- 从“命中点和动作强耦合”改成了“业务代码只埋点，激活动作由外部配置决定”
- 支持先写配置、后注册故障点；注册完成后会自动继承之前的文件配置

当前实现主要分布在以下文件：

- `src/util/csrc/mf_failpoint.h`
- `src/util/csrc/mf_failpoint.cpp`
- `src/util/csrc/mf_fault_injection_point_registry.h`
- `src/util/csrc/mf_fault_injection_point_registry.cpp`
- `src/hybm/csrc/entity/hybm_entity_default.cpp`
- `src/smem/csrc/smem_bm/smem_bm.cpp`
- `src/smem/csrc/smem_bm/smem_bm_entry.cpp`
- `test/ut/testcase/utils/mf_failpoint_test.cpp`

## 2. 总体设计

### 2.1 角色划分

当前框架可以分成四层：

1. `FaultInjectionPointManager`

负责故障点的生命周期管理，包括：

- `Init` / `Exit`
- `Register` / `Unregister`
- `Activate` / `Deactivate` / `DeactivateAll`
- `Reload`
- `Begin`

2. `FIP_START` / `FIP_END`

这是业务代码里真正使用的埋点宏。业务代码只需要把可能注入故障的位置包起来，运行时是否触发故障由 `FaultInjectionPointManager` 决定。

3. `FaultInjectionPointRegistrar`

这是一个“默认故障点注册器”，用于把仓库内置故障点批量注册到 `FaultInjectionPointManager`。

4. 配置文件 + 自动重载

用于在进程运行期间动态激活、更新、清除故障注入配置。

### 2.2 当前内置故障点

当前代码里已经接入的内置故障点有两个：

| 故障点名 | 位置 | 包裹的真实操作 | 默认 callback 行为 |
|---|---|---|---|
| `ALLOC_LOCAL_MEMORY` | `src/hybm/csrc/entity/hybm_entity_default.cpp` | `segment->AllocLocalMemory(...)` | 把返回值改成 `-1`，并跳过原代码块 |
| `MMAP` | `src/smem/csrc/smem_bm/smem_bm_entry.cpp` | `hybm_mmap(entity_, 0)` | 把返回值改成 `-1`，并跳过原代码块 |

`smem_bm_init()` 中会调用 `FaultInjectionPointRegistrar::Register()`，`smem_bm_uninit()` 中会调用 `FaultInjectionPointRegistrar::Unregister()`，因此当前默认注册入口是在 BM 初始化/反初始化流程里。

## 3. 运行机制

### 3.1 编译期开关

FaultInjectionPoint 功能由 `MF_ENABLE_TRACEPOINT` 控制。

- 在 CMake 构建里，`DEBUG` / `Debug` / `ASAN` 会打开这个宏
- 在 Bazel 构建里，`dbg` 模式会打开这个宏
- 如果没有定义这个宏，`FIP_START` / `FIP_END` 会退化成普通代码块，`FaultInjectionPointManager` 也基本是 no-op

这意味着：

- Release 版本默认不会真正执行故障注入
- 要做故障注入验证，应使用 debug/asan 构建

### 3.2 初始化与退出

`FaultInjectionPointManager::Init()` 做两件事：

- 维护引用计数，支持多次初始化/退出
- 建立当前进程 FaultInjectionPoint 状态

安装成功后，配置文件路径固定为：

`/tmp/mf_failpoints_<pid>.conf`

其中 `<pid>` 是目标进程的进程号。

`FaultInjectionPointManager::Exit()` 在引用计数归零时会：

- 清空已注册故障点
- 清空文件配置缓存
- 清空当前进程的配置文件状态缓存

### 3.3 注册

注册分为两类：

1. 无 callback 注册

只适用于 `pause` / `reset` / `abort` 这类不依赖回调函数的故障点。

2. 带 callback 注册

适用于需要修改返回值、输出参数、上下文状态的故障点。

注册时框架会记录：

- 故障点名
- 描述
- callback 指针
- callback 签名
- 注册引用计数

同名重复注册时，要求以下信息完全一致，否则返回 `FaultInjectionPointStatus::ERROR`：

- 描述字符串一致
- callback 地址一致
- callback 类型签名一致

### 3.4 激活

故障点可以通过两种方式激活：

1. 进程内 API 激活

- `FaultInjectionPointManager::Activate(name, type, timeAlive, userParam)`

2. 文件配置激活

- 写 `/tmp/mf_failpoints_<pid>.conf`
- 如果随后调用 `FaultInjectionPointRegistrar::Register()`，注册器会在内置故障点全部注册完成后检查该文件；只要文件已存在，就立刻执行一次 `Reload()`
- 运行中的后续更新通常不需要信号；只要配置文件内容或存在状态变化，下一次进入 `FaultInjectionPointManager` 路径时就会自动 `Reload()`

### 3.5 命中

业务代码通过如下形式埋点：

```cpp
Result ret = BM_OK;
FIP_START(ALLOC_LOCAL_MEMORY, &ret)
ret = segment->AllocLocalMemory(size, realSlice);
FIP_END;
```

命中时 `FaultInjectionPointManager::Begin()` 的处理逻辑是：

1. 若检测到当前 pid 配置文件发生变化，则先执行一次 `Reload()`
2. 查找故障点是否已注册
3. 判断是否处于激活状态，且 `timeAlive > 0`
4. 对 callback 类型额外校验签名是否与当前实参匹配
5. 命中后递减 `timeAlive`
6. 当 `timeAlive` 递减到 `0` 时自动失活
7. 根据动作类型决定是否执行原代码块

是否跳过原代码块由 `FaultInjectionPointExecution::skipBlock` 控制：

- `callback`：跳过原代码块
- `reset`：跳过原代码块
- `abort`：跳过原代码块
- `pause`：不跳过原代码块，只是先 sleep 再继续执行

## 4. 配置文件格式

### 4.1 基本格式

配置文件按行解析，每一行格式如下：

```text
<trace_point_name> <type> <time_alive> [user_param]
```

示例：

```text
ALLOC_LOCAL_MEMORY callback 1
MMAP callback 2
MMAP pause 3 200
MMAP abort 1
```

字段说明：

| 字段 | 含义 |
|---|---|
| `trace_point_name` | 故障点名称，必须和 `FIP_START(NAME, ...)` 中的名字一致 |
| `type` | 动作类型，支持 `callback` / `pause` / `reset` / `abort` |
| `time_alive` | 还能触发多少次，命中一次就减一，减到 `0` 后自动失活 |
| `user_param` | 可选参数，按字符串存入 `FaultInjectionPointParam::paramData` |

### 4.2 行解析规则

- 空行会被忽略
- 以 `#` 开头的行会被当作注释忽略
- 非法行会被跳过，并输出 warning 日志
- 同一个故障点在同一个配置文件里重复出现时，只接受第一条，后续重复项会被忽略

### 4.3 `user_param` 规则

`FaultInjectionPointParam::paramData` 长度固定为 32 字节，因此：

- `user_param` 最长只能保存 31 个可见字符，结尾还要留 `'\0'`
- 过长内容会被截断

当前内置动作对 `user_param` 的使用如下：

1. `callback`

原样传给 callback，具体含义由 callback 自己解释。

2. `pause`

把 `user_param` 当作毫秒数解析。

- 合法且大于 `0`：按指定毫秒数 sleep
- 为空、非法或等于 `0`：回退到默认值 `10000ms`

3. `reset`

当前不读取 `user_param`。

4. `abort`

当前不读取 `user_param`。

## 5. 四种动作的语义

### 5.1 `callback`

用途：

- 修改返回值
- 修改输出参数
- 模拟底层调用失败
- 注入额外状态

行为：

- 命中后执行注册时绑定的 callback
- 跳过 `FIP_START` 和 `FIP_END` 中间的原始代码块

如果故障点没有注册 callback，却试图以 `callback` 类型激活：

- 直接调用 `Activate()` 会返回 `CALLBACK_NULL`
- 通过文件 `Reload()` 激活时，这一行会被保留在文件配置里，但不会真正激活该点，并打印 warning

### 5.2 `pause`

用途：

- 模拟超时
- 模拟慢路径
- 放大竞态窗口

行为：

- 先 sleep 指定毫秒数
- 然后继续执行原代码块

### 5.3 `reset`

用途：

- 模拟进程被外部终止
- 验证上层恢复流程

行为：

- 命中后调用 `raise(SIGTERM)`
- 跳过原代码块

### 5.4 `abort`

用途：

- 模拟致命错误
- 验证 core dump / watchdog / supervisor 恢复逻辑

行为：

- 命中后调用 `std::abort()`
- 跳过原代码块

## 6. 如何激活故障

### 6.1 前提条件

先确认以下前提：

- 目标程序是 debug/asan 构建，编译时开启了 `MF_ENABLE_TRACEPOINT`
- 目标程序已经把需要的故障点注册进 `FaultInjectionPointManager`
- 对于仓库内置故障点，要确认目标流程会调用 `FaultInjectionPointRegistrar::Register()`

### 6.2 找到目标进程 PID

可以直接用 `pgrep` 或 `ps`：

```bash
pgrep -af bm_host_example
ps -ef | grep memfabric
```

假设 PID 是 `12345`，则对应配置文件路径就是：

```text
/tmp/mf_failpoints_12345.conf
```

### 6.3 写配置文件

例如要让 `ALLOC_LOCAL_MEMORY` 失败 1 次，让 `MMAP` 暂停 3 次、每次 200ms：

```bash
cat > /tmp/mf_failpoints_12345.conf <<'EOF'
ALLOC_LOCAL_MEMORY callback 1
MMAP pause 3 200
EOF
```

### 6.4 首次激活与后续更新

- 如果配置文件在 `FaultInjectionPointRegistrar::Register()` 结束前已经存在，注册器会立刻执行一次 `Reload()`，第一轮即可生效
- 如果配置文件是在进程运行过程中新增、删除或修改的，后续下一次进入 `FaultInjectionPointManager` 路径时会自动 `Reload()`

### 6.5 验证是否生效

通常有三种验证方式：

1. 直接复现目标路径

例如再次执行会触发 `ALLOC_LOCAL_MEMORY` 或 `MMAP` 的业务操作。

2. 看日志

框架会输出类似日志：

- `Triggered callback fault injection point 'ALLOC_LOCAL_MEMORY'`
- `Triggered pause fault injection point 'MMAP' ms=200`
- `Triggered reset fault injection point 'POINT'`
- `Triggered abort fault injection point 'POINT'`

3. 通过行为验证

例如：

- 返回值是否变成注入值
- 操作是否明显 sleep
- 进程是否被 `SIGTERM` 结束
- 进程是否 `abort`

### 6.6 如何取消故障

有三种方式：

1. 命中次数耗尽

`time_alive` 减到 `0` 后会自动失活。

2. 删除配置后重载

```bash
rm -f /tmp/mf_failpoints_12345.conf
```

配置文件删除后，后续下一次进入 `FaultInjectionPointManager` 路径时，`Reload()` 会清空当前文件驱动的激活状态。

3. 进程内主动关闭

```cpp
FaultInjectionPointManager::Deactivate("MMAP");
FaultInjectionPointManager::DeactivateAll();
```

## 7. 如何新增故障点

新增故障点建议按下面四步做。

### 7.1 第一步：选定注入位置和预期行为

先明确三个问题：

- 故障发生在哪一行或哪一个调用前后
- 需要跳过原逻辑，还是只做延时
- 需要修改哪些变量或返回值

如果只是想制造延迟，通常只需要 `pause`。

如果想直接让调用失败，通常用 `callback` 修改返回值最灵活。

### 7.2 第二步：在业务代码埋点

最小用法如下：

```cpp
#include "mf_failpoint.h"

int32_t ret = 0;
FIP_START(MY_NEW_POINT, &ret)
ret = RealCall();
FIP_END;
```

说明：

- `MY_NEW_POINT` 会被宏展开成字符串 `"MY_NEW_POINT"`
- 如果 callback 需要修改外部变量，必须把指针传进去，例如 `&ret`
- 如果传的是值而不是指针，callback 改到的只是内部临时副本

如果只是做 `pause`，也可以不传参数：

```cpp
FIP_START(MY_SLOW_POINT)
DoRealWork();
FIP_END;
```

### 7.3 第三步：注册故障点

有两种注册方式。

#### 方式 A：直接在模块初始化里注册

适合单点验证或局部模块自管理。

```cpp
void InjectMyNewPoint(ock::mf::FaultInjectionPointParam *userParam, int32_t *ret)
{
    (void)userParam;
    if (ret != nullptr) {
        *ret = -1;
    }
}

auto status = ock::mf::FaultInjectionPointManager::Register(
    "MY_NEW_POINT", "inject RealCall failure", &InjectMyNewPoint);
```

如果不需要 callback：

```cpp
auto status = ock::mf::FaultInjectionPointManager::Register(
    "MY_SLOW_POINT", "inject slow path");
```

#### 方式 B：加入 `FaultInjectionPointRegistrar`

适合要做成仓库内置默认故障点的场景。

当前做法可以参考 `src/util/csrc/mf_fault_injection_point_registry.cpp`：

- 定义故障点名和描述
- 提供统一 callback
- 在 `FaultInjectionPointRegistrar::Register()` 中调用 `RegisterPoint(...)`
- 在 `FaultInjectionPointRegistrar::Unregister()` 中补对应 `UnregisterPoint(...)`

如果你希望它跟随 BM 初始化自动注册，这一步是必要的，因为当前 `smem_bm_init()` 调用的是 `FaultInjectionPointRegistrar::Register()`。

### 7.4 第四步：补测试

建议至少补以下几类测试，`test/ut/testcase/utils/mf_failpoint_test.cpp` 里已经有现成样例：

- 注册/反注册
- callback 命中后是否跳过原代码块
- pause 是否真的 sleep
- 文件 reload 是否生效
- 先 reload 再 register 是否能继承待生效配置
- 配置文件变更后的自动重载是否生效

## 8. 新增故障点时的几个关键约束

### 8.1 callback 签名必须严格匹配

`FaultInjectionPointManager::Begin()` 会校验运行时实参与注册 callback 的签名是否一致。

例如：

```cpp
FIP_START(MY_POINT, &ret)
```

对应 callback 必须写成：

```cpp
void InjectMyPoint(ock::mf::FaultInjectionPointParam *userParam, int32_t *ret)
```

如果签名不匹配，框架会打印 warning，并且本次 callback 不会执行。

### 8.2 需要改外部状态时，优先传指针

这是最容易踩坑的点。

建议：

- 要改返回值，传 `&ret`
- 要改结构体内容，传结构体指针
- 要改上下文字段，传上下文对象指针

不要依赖按值传参，因为当前实现会把实参先存入内部 tuple，再把内部副本地址传给 callback。

### 8.3 描述字符串要稳定

同名重复注册时，描述不同会直接报错，所以描述不要写成会变化的内容。

### 8.4 默认注册点是否启用，取决于注册入口和编译宏

当前 `FaultInjectionPointRegistrar` 由 `MF_ENABLE_TRACEPOINT` 控制默认注册点注册。

因此如果你把新故障点做进默认注册器，需要同时确认：

- 对应模块的注册代码已经加到 `FaultInjectionPointRegistrar`
- 目标初始化流程确实调用了 `FaultInjectionPointRegistrar::Register()`
- 构建系统已经定义了 `MF_ENABLE_TRACEPOINT`

如果这些条件不满足，业务代码里即使埋了 `FIP_START(...)`，默认故障点也不会自动注册。

## 9. 推荐使用方式

### 9.1 开发调试阶段

推荐直接走“配置文件 + 自动重载”方式：

- 不需要重启进程
- 可以动态切换故障
- 可以快速复现偶发问题

### 9.2 单元测试阶段

推荐直接走 API：

- `Register(...)`
- `Activate(...)`
- `Deactivate(...)`
- `Reload()`

这样测试更稳定，也更容易断言行为。

### 9.3 默认故障点建设

如果某个故障点对排障价值高、复用频繁，建议：

- 在业务代码里保留固定埋点
- 在 `FaultInjectionPointRegistrar` 里做成默认注册点
- 在单测里补齐注册、激活和 reload 用例

## 10. 一个完整示例

### 10.1 新增一个返回失败的故障点

业务代码：

```cpp
#include "mf_failpoint.h"

int32_t ret = 0;
FIP_START(MY_ALLOC_POINT, &ret)
ret = RealAlloc();
FIP_END;
if (ret != 0) {
    return ret;
}
```

注册代码：

```cpp
void InjectMyAllocPoint(ock::mf::FaultInjectionPointParam *userParam, int32_t *ret)
{
    (void)userParam;
    if (ret != nullptr) {
        *ret = -1;
    }
}

ock::mf::FaultInjectionPointManager::Register(
    "MY_ALLOC_POINT", "inject RealAlloc failure", &InjectMyAllocPoint);
```

激活方式：

```bash
cat > /tmp/mf_failpoints_12345.conf <<'EOF'
MY_ALLOC_POINT callback 1
EOF
```

### 10.2 新增一个慢路径故障点

业务代码：

```cpp
FIP_START(MY_SLOW_POINT)
DoRealWork();
FIP_END;
```

注册代码：

```cpp
ock::mf::FaultInjectionPointManager::Register(
    "MY_SLOW_POINT", "inject slow path");
```

激活方式：

```bash
cat > /tmp/mf_failpoints_12345.conf <<'EOF'
MY_SLOW_POINT pause 5 500
EOF
```

上面这条配置表示该点后续命中 5 次时，每次先暂停 500ms，再继续执行原逻辑。

## 11. 与旧方案的差异总结

如果之前使用过旧的环境变量故障注入，需要注意下面这些变化：

| 旧方案 | 新方案 |
|---|---|
| `MF_FAILPOINTS` 环境变量驱动 | `/tmp/mf_failpoints_<pid>.conf` 文件驱动 |
| 按 `POINT@deviceId` 匹配 | 按进程内已注册的故障点名匹配 |
| 主要支持 `fail` / `sleep` | 支持 `callback` / `pause` / `reset` / `abort` |
| 进程启动前准备环境变量 | 进程运行中通过文件变更自动重载 |
| 设备维度匹配 | 当前实现不再内建 device 维度匹配 |

因此：

- 旧的 `MF_FAILPOINTS="MMAP@0=fail(count=1)"` 语法已经不再适用
- 新方案的核心入口是“注册故障点”和“向目标进程写配置文件”

## 12. 排障建议

如果配置了故障但没有生效，优先检查下面几项：

1. 是否是 debug/asan 构建，编译时是否定义了 `MF_ENABLE_TRACEPOINT`
2. 故障点名称是否和 `FIP_START(NAME, ...)` 完全一致
3. 目标进程是否真的调用过 `FaultInjectionPointManager::Init()` 或 `FaultInjectionPointRegistrar::Register()`
4. 配置文件路径里的 PID 是否写对
5. 配置文件是否已经写入目标进程可见的 `/tmp`
6. 配置文件写入、更新或删除后，目标进程是否又进入过一次 `FaultInjectionPointManager` 路径
7. 如果希望第一轮就生效，配置文件是否已经早于 `FaultInjectionPointRegistrar::Register()` 准备完成
8. callback 签名是否与宏实参严格匹配
9. `user_param` 是否超过 31 字符而被截断
