# 集成测试 (Integration Tests)

集成测试目录，用于验证各组件在真实环境下的功能和性能。

## 目录说明

```
test/integration/
├── decrypt/              # 自定义解密库示例
├── etcd_backend/         # Etcd 后端测试工具
└── config_store/         # 配置存储与集群发现测试
```

## 组件列表

| **名称** | **介绍** | **路径** |
|---------|---------|----------|
| decrypt | 自定义解密库示例，用于 TLS 私钥密码解密 | [decrypt](./decrypt/README.md) |
| etcd_backend | Etcd 后端存储的并发 PrefixGet 性能测试 | [etcd_backend](./etcd_backend/README.md) |
| config_store | 配置存储自动选举、多进程 rendezvous、多集群隔离验证 | [config_store](./config_store/README.md) |