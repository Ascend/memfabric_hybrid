# go 版本

go version go1.24.12 linux/arm64

# 安装和测试

```shell
yum install etcd.aarch64
nohup etcd \
  --name test-etcd \
  --data-dir /tmp/etcd-data \
  --listen-client-urls http://<etcd_ip>:12335 \
  --advertise-client-urls http://127.0.0.1:12335 \
  --listen-peer-urls http://<etcd_ip>:12336 \
  --initial-advertise-peer-urls http://127.0.0.1:12336 \
  --initial-cluster test-etcd=http://127.0.0.1:12336 \
  --initial-cluster-token tkn1 \
  --initial-cluster-state new &

  etcdctl --endpoints=http://127.0.0.1:12335 put k v

  etcdctl --endpoints=http://127.0.0.1:12335 get k

  # List all keys and their values
etcdctl --endpoints=http://127.0.0.1:12335 get "" --prefix

# Delete all keys from the store
etcdctl --endpoints=http://127.0.0.1:12335 del "" --prefix

```

# 编译 ETCD 客户端库

```shell
go mod init etcd-client-cgo
go mod tidy
go build -o libetcd_client_v3.so -buildmode=c-shared etcd_client_v3.go
```

# 启用 ETCD 模式

1. **安装和启动 ETCD**：按照上述步骤安装并启动 ETCD 服务，确保它在指定端口运行。

2. **编译和部署客户端库**：
   - 编译 `libetcd_client_v3.so`。
   - 拷贝 `libetcd_client_v3.so` 到 `LD_LIBRARY_PATH` 路径下，例如：
     ```shell
     cp libetcd_client_v3.so /usr/local/lib/
     export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
     ```

3. **配置应用使用 ETCD**：
   - 在 MemFabric 应用中，将 configStore URL 改为 `etcd://<etcd_ip>:<etcd_port>` 格式，例如 `etcd://127.0.0.1:12335`。
   - 确保应用能加载 SO 文件（可能需要重启或设置环境变量）。

4. **验证**：
   - 运行应用，检查日志中是否有 ETCD 连接成功的消息。
   - 使用 `etcdctl` 查看 MemFabric 相关的键值对。

注意：ETCD 模式支持高可用集群，生产环境建议部署多节点 ETCD 集群。

# 常见问题