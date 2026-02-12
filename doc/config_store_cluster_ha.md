# config_store_cluster_ha

> Config Store 实现分析 (引入ETCD和降级机制)

---

## 目录

1. [架构概述](#架构概述)
2. [第一部分：无 ETCD 模式](#第一部分无-etcd-模式)
3. [第二部分：ETCD 模式 - 正常流程](#第二部分etcd-模式---正常流程)
4. [第三部分：ETCD 模式 - 网络分区](#第三部分etcd-模式---网络分区)

---

## 架构概述

### 核心组件

| 组件                  | 职责                                              | 关键常量                                      |
|---------------------|-------------------------------------------------|-------------------------------------------|
| `TcpConfigStore`    | TCP 配置存储，Server/Client 双角色                      | `CONNECT_RETRY_MAX_TIMES = 60`            |
| `AccStoreServer`    | TCP Server 实现，KV 存储管理                           | `HEARTBEAT_INTERVAL`, `HEARTBEAT_TIMEOUT` |
| `HybridConfigStore` | ETCD 模式混合存储，基于 TcpConfigStore 代理实现，负责 Leader 选举 | `ETCD_LEASE_TTL_SEC = 5`                  |
| `EtcdClientV3`      | ETCD v3 客户端，分布式协调，封装go api，c接口->c++接口           | Lease 自动续约                                |
| `NetworkChecker`    | 网络连通性检查 (非阻塞)，Server连通性检查，查找本地可用bind端口          | `CONNECTION_TIMEOUT_SEC = 3`              |

### 关键时序参数

| 参数             | 值                   | 说明                                               |
|----------------|---------------------|--------------------------------------------------|
| ETCD Lease TTL | 5 秒                 | Leader 注册信息的有效期                                  |
| Leader 健康检查间隔  | 4 秒                 | 检测 ETCD 连接和 Lease 状态                             |
| 随机退避           | 100-1000ms          | 避免选举惊群                                           |
| 网络检查超时         | 3 秒                 | 判断 Leader 是否可达                                   |
| 心跳超时           | 3 秒                 | 心跳检测超时时间                                         |
| 恢复等待           | 5秒wait连接 + 5秒wait清理 | 新 Leader 等待旧 Rank 重连，再等待unimport (异步执行，不阻塞老节点连接) |
| 客户端重连次数        | 60 次 (间隔1秒)         | Client 最大重连尝试次数，每次间隔1秒                           |

### ETCD 存储的关键 Key

| Key 常量                   | 示例值                | TTL | 用途                      |
|--------------------------|--------------------|-----|-------------------------|
| `ETCD_KEY_LEADER`        | "192.168.1.1:9112" | 5s  | 当前 Leader 地址            |
| `ETCD_KEY_LEADER_STATUS` | "true"             | 5s  | Leader 就绪状态，接受新的rank分配  |
| `ETCD_KEY_WORLD_SIZE`    | "3"                | 5s  | 集群大小                    |
| `ETCD_KEY_ALIVE_RANKS`   | "0,1,2"            | 5s  | 存活 Rank 列表，新leader，等待恢复 |

---

## 第一部分：无 ETCD 模式

### 1.1 初始化流程

**场景设定**: 节点1 (192.168.1.1:9112) 为固定的 Config Store Server 地址，节点2 作为 Client 连接。

```mermaid
sequenceDiagram
autonumber
participant N1 as 节点1 (Server)
participant Store as AccStoreServer
participant N2 as 节点2 (Client)

rect rgb(240, 248, 255)
Note over N1,Store: 阶段1: 节点1 启动 Server
N1->>N1: PrepareStore()
N1->>N1: localIp == storeUrl.ip ✓
N1->>Store: CreateStore(isServer=true)
Store->>Store: ServerStart()
Store->>Store: 监听 9112 端口
N1->>Store: ClientStart() 连接自己
Store-->>N1: 分配 rankId = 0
end

rect rgb(255, 250, 240)
Note over N2,Store: 阶段2: 节点2 作为 Client 连接
N2->>N2: PrepareStore()
N2->>N2: localIp != storeUrl.ip
N2->>Store: CreateStoreClient()
N2->>Store: ConnectToPeerServer()
Store->>Store: NewLinkHandler()
Store->>Store: FindOrInsertRank()
Store-->>N2: 分配 rankId = 1
end

Note over Store: aliveRankSet_ = {0, 1}
```

### 1.2 节点2 (Follower) 下线 ✅

**结果**: 集群继续正常运行，Server 自动清理节点2 的状态，通知其他 follower。

```mermaid
sequenceDiagram
autonumber
participant N1 as 节点1 (Server)
participant Store as AccStoreServer
participant N2 as 节点2 (Client)

N2->>N2: 进程退出 / 网络断开
N2--xStore: TCP 连接断开

rect rgb(255, 245, 238)
Note over Store: Server 端处理
Store->>Store: LinkBrokenHandler(linkId)
Store->>Store: 从 kvStore_ 查找对应 rankId
Store->>Store: aliveRankSet_.erase(rankId=1)
Store->>Store: heartBeatMap_.erase(linkId)
Store->>Store: PersistAliveRankIds()
end

Store->>Store: rankStateTaskQueue_.push(1)
Store->>N1: 通知 Watch 回调 (WATCH_RANK_LINK_DOWN)

Note over Store: ✅ 集群状态: aliveRankSet_ = {0}
```

### 1.3 节点1 (Config Store) 下线 ⚠️ 灾难性故障

> **严重问题**: 无 ETCD 模式下，Config Store Server 是**单点故障 (SPOF)**。Server 下线后，整个集群不可用，必须人工恢复！

```mermaid
sequenceDiagram
autonumber
participant N1 as 节点1 (Server)
participant Store as AccStoreServer
participant N2 as 节点2 (Client)

rect rgb(255, 200, 200)
N1->>N1: 进程崩溃 / 机器宕机
Store->>Store: 随进程终止
Note over Store: AccStoreServer 终止
end

N2->>N2: TCP 连接检测断开
N2->>N2: LinkBrokenHandler() 触发
N2->>N2: SetConnectStatus(false)

rect rgb(255, 230, 230)
loop 重连尝试 (最多60次，每次间隔1秒)
N2->>N2: ReConnectAfterBroken()
N2--xN1: Connection Refused
N2->>N2: sleep(1s)
end
end

N2->>N2: 重连全部失败
Note over N2: ❌ 集群完全不可用
```

### 1.4 新节点3 加入集群

```mermaid
sequenceDiagram
autonumber
participant N3 as 节点3 (新节点)
participant Store as AccStoreServer

N3->>N3: Initialize(storeURL, worldSize)
N3->>Store: CreateStoreClient(192.168.1.1, 9112)
N3->>Store: ConnectToPeerServer()

Store->>Store: NewLinkHandler(link)
Store->>Store: 检查 aliveRankSet_.size() < worldSize

alt worldSize 足够
Store->>Store: FindOrInsertRank()
Store->>Store: aliveRankSet_.insert(2)
Store-->>N3: 返回 rankId = 2
Note over Store: aliveRankSet_ = {0, 1, 2}
else worldSize 不足
Store-->>N3: ERROR: worldSize exceeded
N3->>N3: 初始化失败
end
```

---

## 第二部分：ETCD 模式 - 正常流程

### 2.1 三节点同时启动 (Leader 选举)

**场景设定**: 三个节点同时启动,通过 ETCD 进行 Leader 选举。使用随机退避 (100-1000ms) 避免惊群效应。

```mermaid
sequenceDiagram
autonumber
participant ETCD as ETCD
participant N1 as 节点1
participant N2 as 节点2
participant N3 as 节点3

rect rgb(240, 248, 255)
Note over N1,N3: 阶段1: 随机退避
par 随机睡眠 (100-1000ms)
N1->>N1: SleepRand() [假设 200ms]
N2->>N2: SleepRand() [假设 500ms]
N3->>N3: SleepRand() [假设 800ms]
end
end

rect rgb(255, 250, 205)
Note over N1,ETCD: 阶段2: 节点1 竞选 Leader
N1->>ETCD: Get(ETCD_KEY_LEADER)
ETCD-->>N1: 空值 (无 Leader)
N1->>ETCD: EtcdLockGuard::Lock()
ETCD-->>N1: 获取分布式锁成功
N1->>ETCD: Get(LEADER) [双重检查]
ETCD-->>N1: 仍为空
N1->>N1: TryBecomeLeader()
N1->>N1: StartServer()
N1->>N1: StartLeaderHealthCheck()
N1->>ETCD: Put(LEADER, "N1:9112", TTL=5s)
N1->>N1: ConnectClient(self)
N1->>ETCD: RawUnlock()
end
Note over N1: ✅ 节点1 成为 Leader (健康检查线程已启动)

rect rgb(240, 255, 240)
Note over N2,ETCD: 阶段3: 节点2 成为 Follower
N2->>ETCD: Get(LEADER)
ETCD-->>N2: "N1:9112"
N2->>N2: NetworkChecker 检测leader可达性
N2->>N2: BecomeFollower()
N2->>N1: ConnectClient()
end
Note over N2: ✅ 节点2 成为 Follower

rect rgb(255, 240, 245)
Note over N3,ETCD: 阶段4: 节点3 成为 Follower
N3->>ETCD: Get(LEADER)
ETCD-->>N3: "N1:9112"
N3->>N3: BecomeFollower()
N3->>N1: ConnectClient()
end
Note over N3: ✅ 节点3 成为 Follower
```

### 2.2 Leader 进程崩溃 ✅ 自动故障转移

```mermaid
sequenceDiagram
autonumber
participant ETCD as ETCD
participant N1 as 节点1 (Leader)
participant N2 as 节点2
participant N3 as 节点3

rect rgb(255, 200, 200)
N1->>N1: 进程崩溃
N1--xETCD: Lease 过期 (5s 后)
end

rect rgb(255, 245, 238)
par Follower 检测断连
N2->>N2: TCP 连接断开
N2->>N2: BrokenHandler → TriggerReElectionAsync()
N3->>N3: TCP 连接断开
N3->>N3: BrokenHandler → TriggerReElectionAsync()
end
end

rect rgb(255, 250, 205)
Note over N2,ETCD: 节点2 异步执行 UnifiedLoop 竞选新 Leader
N2->>N2: UnifiedLoop() [异步线程]
N2->>ETCD: Get(LEADER) → 旧值
N2->>N2: NetworkChecker → 不可达
N2->>ETCD: Lock()
N2->>N2: TryBecomeLeader()
N2->>N2: StartServer()
N2->>N2: StartLeaderHealthCheck()
N2->>N2: RestoreFromEtcd()
N2->>ETCD: Put(LEADER, "N2:9112")
Note over N2: 异步等待: Wait 5s (等待旧Rank连接) + Wait 5s (等待unimport清理)
Note over N2: 期间老节点可正常连接，但新节点暂不接受
end
Note over N2: ✅ 节点2 成为新 Leader

rect rgb(240, 255, 240)
N3->>N3: UnifiedLoop() [异步线程]
N3->>ETCD: Get(LEADER) → "N2:9112"
N3->>N3: BecomeFollower()
N3->>N2: ConnectClient()
end
Note over N3: ✅ 节点3 重新连接
```

---

## 第三部分：ETCD 模式 - 网络分区

### 3.1 Leader 与 ETCD 失联 (降级机制生效) ✅

> **降级机制保证安全**: Leader 检测到与 ETCD 失联后，主动停止 TCP Server，断开与所有 Follower 的连接，避免脑裂。

```mermaid
sequenceDiagram
autonumber
participant ETCD as ETCD
participant N1 as 节点1 (Leader)
participant N2 as 节点2 (Follower)
participant N3 as 节点3 (Follower)

rect rgb(255, 245, 238)
Note over N1,ETCD: T=0: 网络分区发生
N1--xETCD: 网络不可达
Note over N1: N1 与 ETCD 失联
Note over N1: 但 N1 ↔ N2/N3 TCP 正常
end

rect rgb(255, 250, 205)
Note over N1: T=4s: 健康检查线程检测
N1->>N1: LeaderHealthCheck()
N1->>ETCD: Get(ETCD_KEY_LEADER)
ETCD--xN1: 请求超时/失败
N1->>N1: 检测到 ETCD 不可达
end

rect rgb(255, 200, 200)
Note over N1: T=4s: Leader 降级 - Stop Server
N1->>N1: StopServer()
N1->>N1: isLeader_ = false
N1--xN2: 断开 TCP 连接
N1--xN3: 断开 TCP 连接
Note over N1: Leader 已降级
end

rect rgb(240, 248, 255)
Note over N2,N3: Follower 检测到断连
par BrokenHandler 触发异步重选
N2->>N2: LinkBrokenHandler()
N2->>N2: TriggerReElectionAsync()
N3->>N3: LinkBrokenHandler()
N3->>N3: TriggerReElectionAsync()
end
end

rect rgb(255, 230, 230)
Note over ETCD: T=5s: ETCD Lease 过期
ETCD->>ETCD: 删除 ETCD_KEY_LEADER
end

rect rgb(240, 255, 240)
Note over N2,ETCD: T=4s+: N2 异步执行 UnifiedLoop 竞选新 Leader
N2->>N2: UnifiedLoop() [异步线程]
N2->>ETCD: Get(LEADER) → 空
N2->>ETCD: Lock() ✓
N2->>N2: TryBecomeLeader()
N2->>ETCD: Put(LEADER, "N2:9112")
Note over N2: 异步等待: Wait 5s (等待连接) + Wait 5s (等待unimport)
Note over N2: 期间老节点可正常连接
Note over N2: ✅ N2 成为新 Leader
end

rect rgb(240, 255, 240)
N3->>N3: UnifiedLoop() [异步线程]
N3->>ETCD: Get(LEADER) → "N2:9112"
N3->>N2: BecomeFollower()
Note over N3: ✅ N3 连接新 Leader
end

rect rgb(255, 250, 205)
Note over N1: N1 尝试重新加入
N1->>N1: TriggerReElectionAsync()
N1->>N1: UnifiedLoop() [异步线程]
N1->>ETCD: InitEtcdConnection()
alt ETCD 恢复
ETCD-->>N1: 连接成功
N1->>ETCD: Get(LEADER) → "N2:9112"
N1->>N2: BecomeFollower()
Note over N1: ✅ N1 成为 Follower
else ETCD 仍不可达
ETCD--xN1: 连接失败
N1->>N1: continue (循环重试，最多60次，每次间隔1秒)
end
end
```

#### 安全保证

| 保证项  | 说明                       |
|------|--------------------------|
| 快速检测 | Leader 在 4 秒内检测到 ETCD 失联 |
| 主动降级 | 主动停止 TCP Server，断开所有连接   |
| 无脑裂  | 不会出现双主                   |
| 触发重选 | Follower 感知到断连后异步触发重新选举  |

#### 时序保证

| 事件        | 时间                |
|-----------|-------------------|
| 健康检查间隔    | 4 秒               |
| Lease TTL | 5 秒               |
| 心跳超时      | 3 秒               |
| Leader 降级 | ~4s (健康检查周期触发后立即) |

**确保旧 Leader 先降级再有新 Leader 产生。**

### 3.2 整个集群与 ETCD 失联 ⚠️ 服务中断

> **场景描述**: 所有节点都无法连接 ETCD。由于 Leader 有降级机制，检测到 ETCD 不可达后会主动停止服务。

```mermaid
sequenceDiagram
autonumber
participant ETCD as ETCD
participant N1 as 节点1 (Leader)
participant N2 as 节点2 (Follower)
participant N3 as 节点3 (Follower)

rect rgb(255, 230, 230)
ETCD--xN1: 网络不可达
ETCD--xN2: 网络不可达
ETCD--xN3: 网络不可达
Note over ETCD,N3: 所有节点都无法访问 ETCD
end

rect rgb(255, 200, 200)
Note over N1: T=4s: Leader 健康检查失败
N1->>N1: LeaderHealthCheck()
N1->>ETCD: Get(LEADER)
ETCD--xN1: 超时
N1->>N1: StopServer() - 降级
N1--xN2: 断开连接
N1--xN3: 断开连接
end

rect rgb(255, 245, 238)
par 所有节点异步触发 UnifiedLoop
N1->>N1: TriggerReElectionAsync()
N1->>N1: UnifiedLoop() [异步线程]
N2->>N2: TriggerReElectionAsync()
N2->>N2: UnifiedLoop() [异步线程]
N3->>N3: TriggerReElectionAsync()
N3->>N3: UnifiedLoop() [异步线程]
end
end

rect rgb(255, 230, 230)
loop 所有节点循环重试 (最多60次，每次间隔1秒)
N1->>ETCD: InitEtcdConnection()
ETCD--xN1: 失败
N1->>N1: sleep(1s)
N2->>ETCD: InitEtcdConnection()
ETCD--xN2: 失败
N2->>N2: sleep(1s)
N3->>ETCD: InitEtcdConnection()
ETCD--xN3: 失败
N3->>N3: sleep(1s)
end
end

Note over N1,N3: ❌ 集群不可用，等待 ETCD 恢复

rect rgb(240, 255, 240)
Note over ETCD: ETCD 恢复
ETCD->>ETCD: 服务恢复

Note over N2: N2 首先检测到
N2->>ETCD: InitEtcdConnection() ✓
N2->>ETCD: Get(LEADER) → 空
N2->>ETCD: Lock() ✓
N2->>N2: TryBecomeLeader()
Note over N2: 异步等待: Wait 5s (等待连接) + Wait 5s (等待unimport)
Note over N2: 期间老节点可正常连接
Note over N2: ✅ N2 成为 Leader

N1->>ETCD: Get(LEADER) → "N2:9112"
N1->>N2: BecomeFollower()
N3->>ETCD: Get(LEADER) → "N2:9112"
N3->>N2: BecomeFollower()
end

Note over N1,N3: ✅ 集群自动恢复
```

#### 关键点

- 由于 Leader 有降级机制，整个集群会停止服务
- 这是**正确的行为**：宁可停止服务也不能出现脑裂
- ETCD 恢复后，集群自动恢复正常

### 3.3 Follower 与 ETCD 失联 ✅ 影响有限

> **影响分析**: Follower 在正常运行时不需要访问 ETCD，仅在重新选举时需要。当前连接保持正常。

```mermaid
sequenceDiagram
autonumber
participant ETCD as ETCD
participant N1 as 节点1 (Leader)
participant N2 as 节点2 (Follower)

N2--xETCD: 网络不可达
Note over N2: N2 无法访问 ETCD
Note over N2: 但 N2 ↔ N1 TCP 正常

rect rgb(240, 255, 240)
Note over N2: 正常运行
loop 业务请求
N2->>N1: Set/Get/Watch 请求
N1-->>N2: 响应
end
Note over N2: ✅ 业务正常，无感知
end

rect rgb(255, 250, 205)
Note over N1: 假设 Leader 故障
N1->>N1: 进程崩溃
N2->>N2: TCP 连接断开
N2->>N2: BrokenHandler → TriggerReElectionAsync()
end

rect rgb(255, 230, 230)
Note over N2: 异步执行 UnifiedLoop 尝试重新选举
N2->>N2: UnifiedLoop() [异步线程]
N2->>ETCD: InitEtcdConnection()
ETCD--xN2: 连接失败
N2->>N2: sleep(1s)
N2->>N2: continue (循环重试，最多60次)
Note over N2: ⚠️ 无法参与选举，等待 ETCD 恢复
end

rect rgb(240, 255, 240)
Note over ETCD,N2: ETCD 对 N2 恢复
N2->>ETCD: InitEtcdConnection() ✓
N2->>ETCD: Get(LEADER)
alt 已有新 Leader
ETCD-->>N2: "N3:9112"
N2->>N2: BecomeFollower()
else 无 Leader
N2->>ETCD: Lock()
N2->>N2: TryBecomeLeader()
end
end
```

#### 影响矩阵

| 状态      | 影响                                    |
|---------|---------------------------------------|
| ✅ 不受影响  | 已建立的 TCP 连接正常、KV 存储读写正常、Watch 通知正常    |
| ⚠️ 潜在影响 | 如果 Leader 故障，无法参与选举；需等待 ETCD 恢复才能重新加入 |